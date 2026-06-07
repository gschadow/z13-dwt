# Copyright (c) 2026, Gunther Schadow. All rights reserved.

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
SYSTEMD_UNITDIR ?= /etc/systemd/system
CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra
SYSTEMCTL ?= systemctl

all: z13-dwt

z13-dwt: src/z13-dwt.c
	$(CC) $(CFLAGS) -o $@ $<

install: z13-dwt
	install -Dm755 z13-dwt $(DESTDIR)$(BINDIR)/z13-dwt
	install -Dm644 systemd/z13-dwt.service $(DESTDIR)$(SYSTEMD_UNITDIR)/z13-dwt.service

clean:
	rm -f z13-dwt

enable-service:
	$(SYSTEMCTL) daemon-reload
	$(SYSTEMCTL) enable --now z13-dwt.service

restart-service:
	$(SYSTEMCTL) daemon-reload
	$(SYSTEMCTL) restart z13-dwt.service

.PHONY: all install clean enable-service restart-service
