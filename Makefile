PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
LIBEXECDIR ?= $(PREFIX)/libexec
SYSTEMD_UNITDIR ?= /etc/systemd/system
CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra

all: z13-dwt

z13-dwt: src/z13-dwt.c
	$(CC) $(CFLAGS) -o $@ $<

install: z13-dwt
	install -Dm755 z13-dwt $(DESTDIR)$(BINDIR)/z13-dwt
	install -Dm755 scripts/run-z13-dwt.sh $(DESTDIR)$(LIBEXECDIR)/z13-dwt/run-z13-dwt.sh
	install -Dm644 systemd/z13-dwt.service $(DESTDIR)$(SYSTEMD_UNITDIR)/z13-dwt.service

clean:
	rm -f z13-dwt

.PHONY: all install clean
