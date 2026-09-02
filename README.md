# ft_irc

An IRC server written in C++98, speaking enough of RFC 1459 / 2812 that a real
client — irssi, HexChat, WeeChat — connects, registers and chats without ever
knowing it isn't talking to a production daemon.

Handles many clients at once on a single thread, with no forking and without
ever blocking on a socket.

---

## Build

```bash
make
```

Produces `./ircserv`. Compiled with `-Wall -Wextra -Werror -std=c++98`, no
external libraries.

| Target | What it does |
| --- | --- |
| `make` / `make all` | Build `ircserv` |
| `make test` | Build and run the unit tests (parser + client buffers) |
| `make nettest` | Run the integration suite against a live `./ircserv` |
| `make testall` | Both of the above |
| `make debug` | Rebuild with `-g3 -fsanitize=address,undefined` |
| `make clean` | Remove object files |
| `make fclean` | Also remove the binaries |
| `make re` | `fclean` then `all` |

---

## Usage

```bash
./ircserv <port> <password>
```

- `port` — the port to listen on, between 1024 and 65535
- `password` — the connection password every client must send via `PASS`

```bash
./ircserv 6667 hunter2
```

Bad arguments are rejected before any socket is opened, with the reason on
stderr and a non-zero exit status.

### Connecting

With irssi:

```
/connect localhost 6667 hunter2 mynick
/join #42
```

With HexChat, add a network pointing at `localhost/6667`, put the password in
the network's **Password** field, and disable SSL.

With netcat, to watch the raw protocol:

```
nc -C localhost 6667
PASS hunter2
NICK mynick
USER mynick 0 * :Real Name
JOIN #42
PRIVMSG #42 :hello
```

`SIGINT` (Ctrl-C) shuts the server down cleanly through the normal exit path.

---

## Project layout

```
srcs/                    all implementation files
  main.cpp               argument parsing, signal setup, startup
  Server.cpp             sockets, the poll() loop, client lifetime
  Client.cpp             per-connection state and I/O buffers
  Channel.cpp            membership, operators, modes, invites
  Message.cpp            one parsed IRC line
  MessageParser.cpp      raw line -> Message
  CommandDispatcher.cpp  command name -> handler
  irc_utils.cpp          casemapping, validation, string chores
  registration.cpp       PASS NICK USER QUIT PING PONG
  messaging.cpp          PRIVMSG NOTICE
  channels.cpp           JOIN PART TOPIC NAMES
  oper.cpp               MODE KICK INVITE

includes/                all headers
  replies.hpp            every numeric reply, as macros

tests/
  test_parser.cpp        unit tests for the message parser
  test_client.cpp        unit tests for the client buffers
  net_test.sh            black-box integration tests over real sockets
```

---

## How it works

**One thread, one `poll()`.** The server never forks and never spawns a thread.
Every socket — the listener and every client — lives in a single
`std::vector<struct pollfd>` handed to one `poll()` call per iteration. The call
blocks with an infinite timeout, so an idle server burns no CPU at all.

**Nothing blocks.** The listening socket and every accepted socket are set
`O_NONBLOCK` before they are ever used. `recv()` and `send()` are only ever
called on a descriptor `poll()` has just reported ready, and a short `send()` is
normal rather than an error.

**Reads are reassembled, not assumed.** TCP is a byte stream, so a command can
arrive split across three packets, and three commands can arrive glued into one.
Incoming bytes are appended to a per-client buffer and complete lines are pulled
off it one at a time; a partial line simply waits for the rest. Both `\r\n` and a
bare `\n` terminate a line, since not every client sends CRLF.

**Writes are queued, never forced.** No handler ever calls `send()`. Replies are
pushed into the client's output buffer, `POLLOUT` is requested only while that
buffer has something in it, and the loop drains as much as the kernel will take.
A slow or stalled reader can never block the server or any other client.

**Deletion is deferred.** A handler that needs to drop a client asks
`Server::disconnect()`, which marks it and lets the loop reap it after the
current pass. Nothing is ever freed while the loop still holds a reference to it.

`SIGPIPE` is ignored, so a write to a vanished peer returns an error instead of
killing the process.

---

## Registration

A client is registered once `PASS`, `NICK` and `USER` have all arrived, in any
order. Only then does the welcome burst go out — `001` through `004`, `005`
(ISUPPORT) and the MOTD — and it is sent exactly once.

Before registration completes, only `PASS`, `NICK`, `USER`, `QUIT`, `PING` and
`PONG` are accepted; anything else earns `451 ERR_NOTREGISTERED`. A wrong
password is terminal: the client is told why with `464` and disconnected, rather
than left hanging half-registered.

Nicknames are validated (first character a letter or one of ``[]\`_^{|}``, then
those plus digits and `-`), capped at 30 characters, and compared using RFC 1459
casemapping — so `#Foo` and `#foo` are the same channel, and `Bob` and `bob` are
the same nick.

---

## Commands

| Command | Notes |
| --- | --- |
| `PASS` | Connection password. Must match, or the client is dropped |
| `NICK` | Set or change nickname; collisions rejected with `433` |
| `USER` | Username and realname |
| `QUIT` | Orderly disconnect, announced to every channel the client was in |
| `PING` / `PONG` | Keepalive both ways |
| `PRIVMSG` | To a nick or a channel; comma-separated target lists supported |
| `NOTICE` | Same, but never generates an automatic error reply |
| `JOIN` | With key support, and comma-separated channel lists |
| `PART` | With an optional part message |
| `TOPIC` | View or set, subject to `+t` |
| `NAMES` | List the members of a channel |
| `KICK` | Operator only; removes a member with a reason |
| `INVITE` | Operator only; invites a nick, satisfying `+i` |
| `MODE` | Channel modes, below |

### Channel modes

| Mode | Effect |
| --- | --- |
| `+i` / `-i` | Invite-only |
| `+t` / `-t` | Only operators may change the topic |
| `+k <key>` / `-k` | Set or clear the channel key (password) |
| `+o <nick>` / `-o <nick>` | Grant or revoke operator status |
| `+l <n>` / `-l` | Set or clear the member limit |

Modes are applied left to right, and the change announced back to the channel
reflects exactly what actually took effect — an unknown mode character produces
`472`, and `+o` on a non-member produces `441`, without silently dropping the
rest of the string.

The first client to create a channel becomes its operator.

---

## Testing

```bash
make testall
```

**Unit tests** (`make test`) cover the two pieces with the most edge cases and
the least need for a socket: the message parser (prefixes, trailing parameters,
the 15-parameter rule, malformed input) and the client's input and output
buffering.

**Integration tests** (`make nettest`) drive a real `./ircserv` over real TCP
sockets, and test the network layer as a black box:

1. **Line reassembly** — one command split across three packets, several
   commands glued into one write, bare `LF`, and a byte-at-a-time trickle with
   the `CR` and `LF` split apart.
2. **Many clients at once** — a burst of simultaneous connections, after which
   the first client must still be responsive.
3. **Teardown** — orderly `QUIT`, rejected password, a client that vanishes
   without warning, a connection that sends nothing at all, and a
   half-registered client disappearing.
4. **Robustness** — a legal long line not mistaken for flooding, junk and blank
   lines not killing the connection.
5. **Resources** — no file descriptors leaked over 40 connections, and zero CPU
   consumed while idle.
6. **Shutdown** — `SIGINT` exits through `run()`'s normal path.

Run `./tests/net_test.sh -v` to see every server reply as it happens.

For manual protocol debugging, irssi's `/rawlog open` and HexChat's
*Window → Raw Log* show every line in both directions, which is far more useful
than the rendered UI when a numeric doesn't match what a client expects.

---

## Constraints respected

- C++98 only — no C++11 or later features
- No external libraries, no Boost
- No forking
- All I/O is non-blocking, driven by a single `poll()`
- `recv`/`send` are never called on a descriptor `poll()` hasn't marked ready
- The server never hangs, and never crashes on client disconnect

---

## Authors

*(fill in)*
