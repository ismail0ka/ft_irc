#!/usr/bin/env bash
# ============================================================================
#   net_test.sh                                    NETWORK LAYER - integration
#
#   Black-box tests for Server.cpp / Client.cpp over real TCP sockets.
#
#   These do NOT test IRC. They do not check numerics, channel modes or
#   operator rules -- those belong to the parser, dispatcher and Channel.
#   Everything here targets the network layer: line reassembly, many clients
#   on one thread, teardown, resource hygiene and refusal to block.
#
#     ./tests/net_test.sh          run against ./ircserv
#     ./tests/net_test.sh -v       also print each server reply
# ============================================================================

set -u

PORT=${PORT:-6900}
PASS=testpass
BIN=./ircserv
LOG=$(mktemp)
VERBOSE=0
[ "${1:-}" = "-v" ] && VERBOSE=1

PASSED=0
FAILED=0
SRV=

cleanup() {
	[ -n "$SRV" ] && kill -INT "$SRV" 2>/dev/null
	wait "$SRV" 2>/dev/null
	rm -f "$LOG"
}
trap cleanup EXIT

ok()   { PASSED=$((PASSED + 1)); printf '  \033[32mPASS\033[0m  %s\n' "$1"; }
bad()  { FAILED=$((FAILED + 1)); printf '  \033[31mFAIL\033[0m  %s\n' "$1"
         [ -n "${2:-}" ] && printf '        %s\n' "$2"; }
part() { printf '\n\033[1m%s\033[0m\n' "$1"; }

# assert that $2 contains $3
has() {
	if printf '%s' "$2" | grep -qF -- "$3"; then ok "$1"
	else bad "$1" "expected to find: $3"; fi
}
hasnt() {
	if printf '%s' "$2" | grep -qF -- "$3"; then bad "$1" "did not expect: $3"
	else ok "$1"; fi
}

# read everything available on fd $1, giving up after $2 seconds of silence
recv() {
	local out="" line
	while IFS= read -r -t "$2" -u "$1" line; do out+="${line}"$'\n'; done
	printf '%s' "$out" | tr -d '\r'
	[ "$VERBOSE" = 1 ] && printf '%s' "$out" | sed 's/^/        > /' >&2
	return 0
}

register() { printf 'PASS %s\r\nNICK %s\r\nUSER %s 0 * :R\r\n' "$PASS" "$2" "$2" >&"$1"; }

alive() { kill -0 "$SRV" 2>/dev/null; }

# ---------------------------------------------------------------- start up

part "starting server"

[ -x "$BIN" ] || { echo "  $BIN not built -- run make first"; exit 1; }

"$BIN" "$PORT" "$PASS" >"$LOG" 2>&1 &
SRV=$!

for _ in $(seq 40); do
	(exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null && break
	sleep 0.1
done

if alive; then ok "server started on port $PORT (pid $SRV)"
else bad "server failed to start"; cat "$LOG"; exit 1; fi

FD0_COUNT=$(ls /proc/"$SRV"/fd 2>/dev/null | wc -l)

# ------------------------------------------------- 1. line reassembly

part "1. turning a byte stream into lines"

exec {A}<>/dev/tcp/127.0.0.1/$PORT
register "$A" alice
OUT=$(recv "$A" 0.6)
has "a complete registration is understood" "$OUT" " 001 "

# the subject's own test: one command delivered in three packets
printf 'JO' >&"$A";     sleep 0.2
printf 'IN #net' >&"$A"; sleep 0.2
MID=$(recv "$A" 0.3)
hasnt "an incomplete command does nothing yet" "$MID" "JOIN"
printf '\r\n' >&"$A"
OUT=$(recv "$A" 0.6)
has "the same command completes when the rest arrives" "$OUT" "JOIN"

# several commands arriving glued together in one write
printf 'PING one\r\nPING two\r\nPING three\r\n' >&"$A"
OUT=$(recv "$A" 0.6)
has "batched command 1 of 3 ran" "$OUT" "one"
has "batched command 2 of 3 ran" "$OUT" "two"
has "batched command 3 of 3 ran" "$OUT" "three"

# bare LF, as sent by nc without -C
printf 'PING bareLF\n' >&"$A"
OUT=$(recv "$A" 0.6)
has "bare LF is accepted as a line ending" "$OUT" "bareLF"

# a byte at a time, with the CR and LF split apart
for ch in P I N G ' ' d r i p; do printf '%s' "$ch" >&"$A"; done
printf '\r' >&"$A"; sleep 0.2
printf '\n' >&"$A"
OUT=$(recv "$A" 0.6)
has "a command dripped one byte at a time still works" "$OUT" "drip"

# ------------------------------------------------- 2. many clients at once

part "2. many clients on one thread"

declare -a FDS=()
N=20
for i in $(seq 1 $N); do
	exec {fd}<>/dev/tcp/127.0.0.1/$PORT
	FDS+=("$fd")
	register "$fd" "user$i"
done
sleep 1

WELCOMED=0
for fd in "${FDS[@]}"; do
	OUT=$(recv "$fd" 0.2)
	printf '%s' "$OUT" | grep -q " 001 " && WELCOMED=$((WELCOMED + 1))
done

if [ "$WELCOMED" -eq "$N" ]; then ok "$N simultaneous clients all served ($WELCOMED/$N)"
else bad "$N simultaneous clients all served" "only $WELCOMED/$N were welcomed"; fi

# the first client must still be responsive after that burst
printf 'PING afterburst\r\n' >&"$A"
OUT=$(recv "$A" 0.6)
has "an earlier client is not starved by newer ones" "$OUT" "afterburst"

for fd in "${FDS[@]}"; do exec {fd}<&- 2>/dev/null; done
sleep 0.5

# ------------------------------------------------- 3. teardown paths

part "3. every way a client can leave"

# 3a. orderly QUIT -> ERROR line, then the server closes
exec {B}<>/dev/tcp/127.0.0.1/$PORT
register "$B" quitter
recv "$B" 0.5 >/dev/null
printf 'QUIT :done\r\n' >&"$B"
OUT=$(recv "$B" 0.8)
has "QUIT is answered with ERROR :Closing link" "$OUT" "ERROR :Closing link"
exec {B}<&- 2>/dev/null

# 3b. rejected password -> told why, then closed
exec {C}<>/dev/tcp/127.0.0.1/$PORT
printf 'PASS wrongpassword\r\n' >&"$C"
OUT=$(recv "$C" 0.8)
has "a wrong password is refused" "$OUT" " 464 "
has "and the link is closed" "$OUT" "ERROR :Closing link"
exec {C}<&- 2>/dev/null

# 3c. a client that vanishes without warning
exec {D}<>/dev/tcp/127.0.0.1/$PORT
register "$D" vanisher
recv "$D" 0.4 >/dev/null
exec {D}<&-
sleep 0.4
alive && ok "a client vanishing mid-session does not disturb the server" \
        || bad "a client vanishing mid-session does not disturb the server"

# 3d. connect and drop without sending a single byte
exec {E}<>/dev/tcp/127.0.0.1/$PORT
exec {E}<&-
sleep 0.3
alive && ok "connect-then-drop with no data is handled" \
        || bad "connect-then-drop with no data is handled"

# 3e. half-registered client disappearing
exec {F}<>/dev/tcp/127.0.0.1/$PORT
printf 'PASS %s\r\nNICK halfway\r\n' "$PASS" >&"$F"
sleep 0.2
exec {F}<&-
sleep 0.3
alive && ok "a half-registered client disappearing is handled" \
        || bad "a half-registered client disappearing is handled"

# ------------------------------------------------- 4. hostile input

part "4. input that should not bring the server down"

# 600 bytes with no newline: the flood guard should drop this client only
exec {G}<>/dev/tcp/127.0.0.1/$PORT
register "$G" flooder
recv "$G" 0.4 >/dev/null
head -c 600 /dev/zero | tr '\0' 'A' >&"$G" 2>/dev/null
OUT=$(recv "$G" 0.8)
has "an endless line without CRLF is cut off" "$OUT" "ERROR"
exec {G}<&- 2>/dev/null
sleep 0.3
alive && ok "and the server survives it" || bad "and the server survives it"

# a legal 512-byte line must NOT be cut off
exec {H}<>/dev/tcp/127.0.0.1/$PORT
register "$H" longline
recv "$H" 0.4 >/dev/null
LONG=$(head -c 400 /dev/zero | tr '\0' 'x')
printf 'PING %s\r\n' "$LONG" >&"$H"
OUT=$(recv "$H" 0.6)
hasnt "a legal long line is not mistaken for a flood" "$OUT" "ERROR"
exec {H}<&- 2>/dev/null

# unknown commands and junk bytes
exec {I}<>/dev/tcp/127.0.0.1/$PORT
register "$I" junk
recv "$I" 0.4 >/dev/null
printf '\r\n\r\n   \r\nNOTACOMMAND a b c\r\n\x01\x02\x03\r\nPING stillhere\r\n' >&"$I"
OUT=$(recv "$I" 0.8)
has "junk and blank lines do not stop the connection" "$OUT" "stillhere"
exec {I}<&- 2>/dev/null
sleep 0.2
alive && ok "and the server survives that too" || bad "and the server survives that too"

# ------------------------------------------------- 5. resource hygiene

part "5. resources"

# rapid connect/disconnect churn, then compare the open fd count
for _ in $(seq 40); do
	exec {Z}<>/dev/tcp/127.0.0.1/$PORT
	printf 'PASS %s\r\n' "$PASS" >&"$Z"
	exec {Z}<&-
done
sleep 1
FD1_COUNT=$(ls /proc/"$SRV"/fd 2>/dev/null | wc -l)

if [ "$FD1_COUNT" -le $((FD0_COUNT + 2)) ]; then
	ok "no file descriptors leaked over 40 connections ($FD0_COUNT -> $FD1_COUNT)"
else
	bad "no file descriptors leaked over 40 connections" "$FD0_COUNT -> $FD1_COUNT open fds"
fi

# an idle server must be asleep in poll(), not spinning
read -r -a ST < /proc/"$SRV"/stat
T0=$(( ${ST[13]} + ${ST[14]} ))
sleep 2
read -r -a ST < /proc/"$SRV"/stat
T1=$(( ${ST[13]} + ${ST[14]} ))
TICKS=$(( T1 - T0 ))

if [ "$TICKS" -le 2 ]; then
	ok "idle server burns no CPU ($TICKS ticks over 2s -- it is asleep in poll)"
else
	bad "idle server burns no CPU" "$TICKS ticks over 2s -- the loop is spinning"
fi

printf 'PING lastword\r\n' >&"$A"
OUT=$(recv "$A" 0.6)
has "still serving at the end of the run" "$OUT" "lastword"
exec {A}<&- 2>/dev/null

# ------------------------------------------------- 6. shutdown

part "6. shutdown"

kill -INT "$SRV" 2>/dev/null
for _ in $(seq 30); do alive || break; sleep 0.1; done

if alive; then bad "SIGINT stops the server"; kill -9 "$SRV" 2>/dev/null
else ok "SIGINT stops the server cleanly"; fi

grep -q "shutting down" "$LOG" && ok "shutdown ran through run()'s normal exit" \
                               || bad "shutdown ran through run()'s normal exit"
SRV=

# ------------------------------------------------- summary

printf '\n────────────────────────────────────────\n'
if [ "$FAILED" -eq 0 ]; then
	printf '\033[32mOK\033[0m   %d checks passed\n' "$PASSED"
	exit 0
else
	printf '\033[31mFAIL\033[0m %d of %d checks failed\n' "$FAILED" "$((PASSED + FAILED))"
	exit 1
fi
