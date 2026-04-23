/* hw4-client.c */

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>

struct sockaddr_in udp_server;

int send_to_server( int sd, char * buffer, int bytes );
int read_from_server( int sd, char * buffer );
int create_socket( char ** argv );

int main( int argc, char ** argv )
{
  if ( ( argc != 4 && argc != 5 ) || ( argc == 4 && strcmp( *(argv + 3), "NEW" ) != 0 ) )
  {
    fprintf( stderr, "ERROR: Invalid arguments\n" );
    fprintf( stderr, "USAGE: ./hw4-client.out <host> <port> NEW\n" );
    fprintf( stderr, "   or: ./hw4-client.out <host> <port> <token> <guess>\n" );
    return EXIT_FAILURE;
  }

  int sd = create_socket( argv );

  char * buffer = calloc( 13, sizeof( char ) );

  if ( argc == 4 )   /* NEW */
  {
    strcpy( buffer, "NEW" );
    if ( send_to_server( sd, buffer, 3 ) == EXIT_FAILURE ) return EXIT_FAILURE;
    if ( read_from_server( sd, buffer ) == EXIT_FAILURE ) return EXIT_FAILURE;
    int game_token = ntohl( *(int *)buffer );
    printf( "New game assigned game_token %d\n", game_token );
  }
  else   /* continued game play */
  {
    int game_token = atoi( *(argv + 3) );
    *(int *)buffer = htonl( game_token );
    char * guess = *(argv + 4);
    strcpy( buffer + 4, guess );
    if ( send_to_server( sd, buffer, 9 ) == EXIT_FAILURE ) return EXIT_FAILURE;
    if ( read_from_server( sd, buffer ) == EXIT_FAILURE ) return EXIT_FAILURE;
  }
  
  free( buffer );

  return EXIT_SUCCESS;
}

int send_to_server( int sd, char * buffer, int bytes )
{
  printf( "Sending to server\n" );
  int len = sizeof( udp_server );
  int rc = sendto( sd, buffer, bytes, 0, (struct sockaddr *)&udp_server, (socklen_t)len );
  if ( rc == -1 ) { perror( "sendto() failed" ); return EXIT_FAILURE; }
  return EXIT_SUCCESS;
}

int allcaps( char * buffer )
{
  while ( *buffer ) if ( !isupper( *buffer++ ) ) return 0;
  return 1;
}

int read_from_server( int sd, char * buffer )
{
  int n = recvfrom( sd, buffer, 12, 0, NULL, NULL );

  if ( n == -1 ) { perror( "recvfrom() failed" ); return EXIT_FAILURE; }

  if ( n != 4 && n != 12 )
  { fprintf( stderr, "ERROR: rcvd %d bytes\n", n ); return EXIT_FAILURE; }

  int game_token = ntohl( *(int *)buffer );
  printf( "GAME %d: ", game_token );

  if ( n == 4 ) printf( "new game started\n" );

  if ( n == 12 )
  {
    switch ( *(buffer + 4) )
    {
      case 'N': printf( "invalid guess" ); break;
      case 'Y': printf( "result: %s", buffer + 7 ); break;
      default: printf( "valid is '%c'\n", *(buffer + 4) ); return EXIT_FAILURE;
    }

    short guesses = ntohs( *(short *)(buffer + 5) );
    printf( " -- %d guesses remaining\n", guesses );
    if ( guesses == 0 ) printf( "No more guesses!\n" );
    if ( allcaps( buffer + 7 ) ) printf( "YOU WON!\n" );
  }

  return EXIT_SUCCESS;
}

int create_socket( char ** argv )
{
  /* Create the UDP client socket (SOCK_DGRAM) */
  int sd = socket( PF_INET, SOCK_DGRAM, 0 );
  if ( sd == -1 ) { perror( "socket() failed" ); return EXIT_FAILURE; }

  /* Resolve the hostname */
  struct hostent * hp = gethostbyname( *(argv + 1) );
  if ( hp == NULL ) { fprintf( stderr, "ERROR: gethostbyname() failed\n" ); exit( EXIT_FAILURE ); }

  /* populate the socket structure */
  udp_server.sin_family = PF_INET;
  memcpy( (void *)&udp_server.sin_addr, (void *)hp->h_addr, hp->h_length );
  udp_server.sin_port = htons( atoi( *(argv + 2) ) );

  return sd;
}