#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define WORD_LENGTH 5
#define MAX_GUESSES 6

#define NEW_GAME_MSG "NEW"
#define NEW_GAME_LEN 3
#define GUESS_MSG_LEN 9
#define RESPONSE_LEN 12

typedef struct {
    int token;
    char *word;
    short guesses_left;
    int completed;
} game_state;

typedef struct {
    game_state *active_games;
    int num_active_games;
    int capacity;

    int total_wins;
    int total_losses;
    int total_games;
    int next_token;

    char **completed_words;
} server_state;

volatile sig_atomic_t server_running = 1;

char **valid_words;
int valid_word_count = 0;

/* ================= SIGNAL ================= */

void handle_sigusr1(int sig) {
    server_running = 0;
}

/* ================= HELPERS ================= */

int resize_games(server_state *server) {
    if (server->num_active_games >= server->capacity) {
        int new_cap = (server->capacity == 0) ? 10 : server->capacity * 2;
        game_state *temp = realloc(server->active_games, new_cap * sizeof(game_state));
        if (!temp) {
            fprintf(stderr, "ERROR: realloc failed\n");
            return 0;
        }
        server->active_games = temp;
        server->capacity = new_cap;
    }
    return 1;
}

void add_completed_word(server_state *server, const char *word) {
    char **temp = realloc(server->completed_words,
                          server->total_games * sizeof(char *));
    if (!temp) return;

    server->completed_words = temp;
    server->completed_words[server->total_games - 1] =
        calloc(WORD_LENGTH + 1, sizeof(char));

    if (server->completed_words[server->total_games - 1]) {
        strcpy(server->completed_words[server->total_games - 1], word);
    }
}

game_state* find_game(server_state *server, int token) {
    for (int i = 0; i < server->num_active_games; i++) {
        if (server->active_games[i].token == token)
            return &server->active_games[i];
    }
    return NULL;
}

void remove_game(server_state *server, int token) {
    for (int i = 0; i < server->num_active_games; i++) {
        if (server->active_games[i].token == token) {
            free(server->active_games[i].word);
            server->active_games[i] =
                server->active_games[server->num_active_games - 1];
            server->num_active_games--;
            return;
        }
    }
}

/* ================= WORD LOGIC ================= */

int is_valid_word(const char *guess) {
    for (int i = 0; i < valid_word_count; i++) {
        if (strcmp(guess, valid_words[i]) == 0) return 1;
    }
    return 0;
}

void generate_result(const char* guess, const char* word, char* result) {
    int letter_count[26] = {0};

    for (int i = 0; i < WORD_LENGTH; i++)
        letter_count[word[i] - 'a']++;

    for (int i = 0; i < WORD_LENGTH; i++) {
        if (guess[i] == word[i]) {
            result[i] = toupper(guess[i]);
            letter_count[guess[i] - 'a']--;
        } else {
            result[i] = '?';
        }
    }

    for (int i = 0; i < WORD_LENGTH; i++) {
        if (result[i] == '?') {
            if (letter_count[guess[i] - 'a'] > 0) {
                result[i] = tolower(guess[i]);
                letter_count[guess[i] - 'a']--;
            } else {
                result[i] = '-';
            }
        }
    }
}

/* ================= MAIN SERVER ================= */

int wordle_server(int argc, char** argv) {
    if (argc != 5) {
        fprintf(stderr, "Usage: ./server <port> <wordfile> <count> <seed>\n");
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);
    int word_count = atoi(argv[3]);
    int seed = atoi(argv[4]);

    srand(seed);
    signal(SIGUSR1, handle_sigusr1);

    /* INIT SERVER STATE */
    server_state server = {0};

    /* SOCKET SETUP */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(port)
    };

    bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[1024];

    /* ================= MAIN LOOP ================= */

    while (server_running) {

        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                             (struct sockaddr*)&client_addr, &client_len);

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(10000);
                continue;
            }
            perror("recv error");
            continue;
        }

        /* ===== NEW GAME ===== */
        if (n == NEW_GAME_LEN && memcmp(buffer, NEW_GAME_MSG, NEW_GAME_LEN) == 0) {

            server.next_token++;
            server.total_games++;

            if (!resize_games(&server)) continue;

            game_state new_game = {
                .token = server.next_token,
                .guesses_left = MAX_GUESSES,
                .completed = 0,
                .word = calloc(WORD_LENGTH + 1, sizeof(char))
            };

            if (!new_game.word) continue;

            strcpy(new_game.word,
                   valid_words[rand() % valid_word_count]);

            server.active_games[server.num_active_games++] = new_game;

            int response = htonl(server.next_token);
            sendto(sockfd, &response, sizeof(response), 0,
                   (struct sockaddr*)&client_addr, client_len);
        }

        /* ===== GUESS ===== */
        else if (n == GUESS_MSG_LEN) {

            int token = ntohl(*(int*)buffer);
            char guess[WORD_LENGTH + 1];

            memcpy(guess, buffer + 4, WORD_LENGTH);
            guess[WORD_LENGTH] = '\0';

            for (int i = 0; i < WORD_LENGTH; i++)
                guess[i] = tolower(guess[i]);

            game_state *game = find_game(&server, token);

            if (!game) continue;

            int valid = is_valid_word(guess);

            if (valid) {
                game->guesses_left--;

                if (strcmp(guess, game->word) == 0) {
                    server.total_wins++;
                    game->completed = 1;
                    add_completed_word(&server, game->word);
                    remove_game(&server, token);
                } else if (game->guesses_left == 0) {
                    server.total_losses++;
                    game->completed = 1;
                    add_completed_word(&server, game->word);
                    remove_game(&server, token);
                }
            }
        }
    }

    /* ===== SHUTDOWN ===== */
    for (int i = 0; i < server.num_active_games; i++) {
        if (!server.active_games[i].completed) {
            server.total_losses++;
            add_completed_word(&server, server.active_games[i].word);
        }
        free(server.active_games[i].word);
    }

    free(server.active_games);
    close(sockfd);

    return EXIT_SUCCESS;
}