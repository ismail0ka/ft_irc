#!/usr/bin/env python3
"""End-to-end test for ircserv.

Starts the binary on a free port, drives real TCP clients through it and
checks what comes back. Usage:

    python3 tests/e2e.py ./ircserv
"""

import socket
import subprocess
import sys
import time

PASSWORD = "s3cr3t"
FAILED = []
PASSED = 0


class Conn:
    def __init__(self, port, name):
        self.name = name
        self.buf = ""
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=3)
        self.sock.settimeout(0.6)

    def send(self, line):
        self.sock.sendall((line + "\r\n").encode())

    def pump(self, seconds=0.35):
        """Drain whatever the server sent us within the window."""
        deadline = time.time() + seconds
        while time.time() < deadline:
            try:
                data = self.sock.recv(65536)
            except socket.timeout:
                break
            except OSError:
                break
            if not data:
                break
            self.buf += data.decode(errors="replace")
        return self.buf

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


def check(what, got, want, negate=False):
    global PASSED
    hit = want in got
    if hit != negate:
        PASSED += 1
        return
    FAILED.append((what, want, got, negate))


def register(port, nick, user="u"):
    c = Conn(port, nick)
    c.send("PASS " + PASSWORD)
    c.send("NICK " + nick)
    c.send("USER %s 0 * :Real %s" % (user, nick))
    c.pump()
    return c


def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else "./ircserv"
    port = 16667

    srv = subprocess.Popen([binary, str(port), PASSWORD],
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    time.sleep(0.4)
    if srv.poll() is not None:
        print("server died on startup:", srv.stderr.read().decode())
        return 1

    try:
        # ---- registration ------------------------------------------------
        alice = register(port, "alice")
        check("welcome 001", alice.buf, "001 alice")
        check("myinfo 004", alice.buf, "004 alice")

        # ---- wrong password is refused -----------------------------------
        bad = Conn(port, "bad")
        bad.send("PASS wrong")
        bad.pump()
        check("bad password 464", bad.buf, "464")
        bad.close()

        # ---- commands before registration --------------------------------
        early = Conn(port, "early")
        early.send("JOIN #nope")
        early.pump()
        check("unregistered 451", early.buf, "451")
        early.close()

        # ---- unknown command ---------------------------------------------
        alice.buf = ""
        alice.send("WHATEVER")
        alice.pump()
        check("unknown command 421", alice.buf, "421 alice WHATEVER")

        # ---- nick collision ----------------------------------------------
        clash = Conn(port, "clash")
        clash.send("PASS " + PASSWORD)
        clash.send("NICK alice")
        clash.pump()
        check("nick in use 433", clash.buf, "433")
        clash.close()

        # ---- join, names, first joiner is op -----------------------------
        alice.buf = ""
        alice.send("JOIN #42")
        alice.pump()
        check("join echo", alice.buf, "JOIN :#42")
        check("names has @alice", alice.buf, "@alice")
        check("end of names 366", alice.buf, "366")

        bob = register(port, "bob")
        bob.buf = ""
        alice.buf = ""
        bob.send("JOIN #42")
        bob.pump()
        alice.pump()
        check("bob sees own join", bob.buf, "JOIN :#42")
        # std::set<Client*> orders by pointer, so the names list order is not
        # deterministic -- check membership, not layout.
        check("alice is flagged op", bob.buf, "@alice")
        check("bob is in the list", bob.buf, "bob")
        check("alice sees bob join", alice.buf, "JOIN :#42")
        check("join carries a prefix", alice.buf, "bob!")

        # ---- privmsg to channel ------------------------------------------
        bob.buf = ""
        alice.buf = ""
        alice.send("PRIVMSG #42 :hello there")
        alice.pump()
        bob.pump()
        check("bob got channel msg", bob.buf, "PRIVMSG #42 :hello there")
        check("no echo to sender", alice.buf, "PRIVMSG #42", negate=True)

        # ---- privmsg to nick ---------------------------------------------
        bob.buf = ""
        alice.send("PRIVMSG bob :direct")
        bob.pump()
        check("private msg", bob.buf, "PRIVMSG bob :direct")

        alice.buf = ""
        alice.send("PRIVMSG ghost :hi")
        alice.pump()
        check("no such nick 401", alice.buf, "401")

        # ---- NOTICE never errors -----------------------------------------
        alice.buf = ""
        alice.send("NOTICE ghost :hi")
        alice.pump()
        check("notice is silent", alice.buf, "401", negate=True)

        # ---- topic --------------------------------------------------------
        bob.buf = ""
        alice.buf = ""
        alice.send("TOPIC #42 :our topic")
        alice.pump()
        bob.pump()
        check("topic broadcast", bob.buf, "TOPIC #42 :our topic")

        bob.buf = ""
        bob.send("MODE #42 +t")
        bob.pump()
        check("non-op cannot +t", bob.buf, "482")

        alice.send("MODE #42 +t")
        alice.pump()
        bob.buf = ""
        bob.send("TOPIC #42 :hijack")
        bob.pump()
        check("+t blocks non-op topic", bob.buf, "482")

        # ---- modes: key ---------------------------------------------------
        alice.buf = ""
        alice.send("MODE #42 +k letmein")
        alice.pump()
        check("mode +k broadcast", alice.buf, "MODE #42 +k letmein")

        carol = register(port, "carol")
        carol.buf = ""
        carol.send("JOIN #42")
        carol.pump()
        check("wrong key 475", carol.buf, "475")

        carol.buf = ""
        carol.send("JOIN #42 letmein")
        carol.pump()
        check("right key joins", carol.buf, "JOIN :#42")

        # ---- modes: limit -------------------------------------------------
        alice.send("MODE #42 -k letmein")
        alice.send("MODE #42 +l 3")
        alice.pump()
        dave = register(port, "dave")
        dave.buf = ""
        dave.send("JOIN #42")
        dave.pump()
        check("channel full 471", dave.buf, "471")

        # ---- modes: invite only + INVITE ----------------------------------
        alice.send("MODE #42 +i")
        alice.send("MODE #42 -l")
        alice.pump()
        dave.buf = ""
        dave.send("JOIN #42")
        dave.pump()
        check("invite only 473", dave.buf, "473")

        alice.buf = ""
        dave.buf = ""
        alice.send("INVITE dave #42")
        alice.pump()
        dave.pump()
        check("inviting 341", alice.buf, "341")
        check("dave got INVITE", dave.buf, "INVITE dave :#42")
        dave.buf = ""
        dave.send("JOIN #42")
        dave.pump()
        check("invited can join", dave.buf, "JOIN :#42")

        # ---- op privileges -------------------------------------------------
        bob.buf = ""
        bob.send("KICK #42 carol :out")
        bob.pump()
        check("non-op cannot kick", bob.buf, "482")

        alice.send("MODE #42 +o bob")
        alice.pump()
        bob.buf = ""
        carol.buf = ""
        bob.send("KICK #42 carol :out")
        bob.pump()
        carol.pump()
        check("op kick works", carol.buf, "KICK #42 carol :out")

        # ---- part ----------------------------------------------------------
        alice.buf = ""
        dave.send("PART #42 :bye")
        dave.pump()
        alice.pump()
        check("part broadcast", alice.buf, "PART #42 :bye")

        # ---- ping/pong ------------------------------------------------------
        alice.buf = ""
        alice.send("PING :token123")
        alice.pump()
        check("pong", alice.buf, "PONG ircserv :token123")

        # ---- nick change is announced ---------------------------------------
        bob.buf = ""
        alice.send("NICK alice2")
        alice.pump()
        bob.pump()
        check("nick change seen", bob.buf, "NICK :alice2")
        check("nick change uses old prefix", bob.buf, "alice!")

        # ---- quit propagates -------------------------------------------------
        bob.buf = ""
        alice.send("QUIT :gone fishing")
        alice.pump()
        bob.pump()
        check("quit broadcast", bob.buf, "QUIT :gone fishing")

        # ---- a dead client is really gone -----------------------------------
        bob.buf = ""
        bob.send("PRIVMSG alice2 :are you there")
        bob.pump()
        check("quitter is unreachable", bob.buf, "401")

        # ---- the server is still standing ------------------------------------
        late = register(port, "late")
        check("server still serving", late.buf, "001 late")

        for c in (alice, bob, carol, dave, late):
            c.close()

    finally:
        srv.terminate()
        try:
            srv.wait(timeout=3)
        except subprocess.TimeoutExpired:
            srv.kill()

    for what, want, got, negate in FAILED:
        print("FAIL  %s\n      %s [%s]\n      in: %s"
              % (what, "did NOT expect" if negate else "expected", want,
                 got.replace("\r\n", " | ")))
    print("%d/%d checks passed" % (PASSED, PASSED + len(FAILED)))
    return 1 if FAILED else 0


if __name__ == "__main__":
    sys.exit(main())
