PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra

all: z13-dwt

z13-dwt: src/z13-dwt.c
	$(CC) $(CFLAGS) -o $@ $<

install: z13-dwt
	install -Dm755 z13-dwt $(DESTDIR)$(BINDIR)/z13-dwt

clean:
	rm -f z13-dwt

.PHONY: all install clean
