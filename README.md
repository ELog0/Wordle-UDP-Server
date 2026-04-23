# Wordle UDP Client-Server System (C)

## Overview

This project implements a **client-server Wordle game in C using UDP sockets**. The system consists of a non-blocking server that manages multiple concurrent game sessions and a client that communicates with the server to start games and submit guesses.

The server processes requests using a custom binary protocol and maintains per-game state, including guesses remaining and win/loss tracking.

---

## Features

* UDP-based client-server architecture
* Non-blocking server using `fcntl` and `O_NONBLOCK`
* Concurrent game session management using unique tokens
* Custom binary protocol for communication
* Word validation and result generation (Wordle-style feedback)
* Graceful shutdown using `SIGUSR1`
* Dynamic memory management (`malloc`, `realloc`, `free`)

---

## System Architecture

### Server

The server:

* Listens for UDP datagrams
* Handles new game requests (`"NEW"`)
* Assigns a unique game token per session
* Tracks active games dynamically
* Processes guesses and returns results
* Manages memory for active and completed games

### Client

The client:

* Sends requests to the server
* Starts new games or submits guesses
* Parses and displays server responses

### Protocol

#### New Game Request

* Payload: `"NEW"` (3 bytes)
* Response: 4-byte integer (game token)

#### Guess Request

* Payload:

  * 4 bytes: game token (network byte order)
  * 5 bytes: guess (string)
* Total: 9 bytes

#### Response

* 4 bytes: game token
* 1 byte: validity (`'Y'` or `'N'`)
* 2 bytes: guesses remaining
* 5 bytes: result string

Total: 12 bytes

---

## Example Usage

### Compile

```bash
gcc wordle-UDP-server.c -o server
gcc wordle-client.c -o client
```

### Run Server

```bash
./server <port> <word-file> <num-words> <seed>
```

### Start New Game

```bash
./client localhost 8111 NEW
```

### Submit Guess

```bash
./client localhost 8111 <token> STARE
```

---

## Key Concepts Demonstrated

* Low-level socket programming (UDP)
* Network byte order (`htonl`, `ntohl`)
* Custom protocol design
* Dynamic memory management in C
* Event-driven server loop
* Signal handling (`SIGUSR1`)
* Stateful session management

---

## File Structure

* `wordle-UDP-server.c` — main server implementation
* `wordle-client.c` — client for interacting with the server
* `write-hw4-datagram.c` — utility for generating test datagrams

---

## Future Improvements

* Add TCP support
* Implement multithreading for scalability
* Improve protocol robustness (error codes, versioning)
* Add logging and metrics
* Replace linear search with hash-based lookup for words



---
