PREFIX?=$(DESTDIR)/usr
LDFLAGS?=-Wl,--as-needed
SYSCONFDIR=$(DESTDIR)/etc
SYSTEMD_UNIT_DIR?=$(SYSCONFDIR)/systemd/system
SYSTEMCTL:=$(shell which systemctl)

	
override CFLAGS+=-Wall -g `pkg-config --cflags jack`
LOADLIBES=`pkg-config --libs jack` -lpthread -lrt -lm -Wl,--as-needed

all: jackfreqd

jackfreqd: jackfreqd.c jack_cpu_load.c procps.c

install: jackfreqd
	install -m 755 -d $(PREFIX)/sbin
	install -m 755 -s jackfreqd $(PREFIX)/sbin

	@if test -x "$(SYSTEMCTL)" && test -d "/etc/systemd/system"; then\
	  install -m 755 -d $(SYSTEMD_UNIT_DIR);\
	  install -m 644 jackfreq.service $(SYSTEMD_UNIT_DIR)/jackfreq.service;\
	else\
	  install -m 755 -d $(SYSCONFDIR)/init.d;\
	  install -m 755 jackfreqd.init $(SYSCONFDIR)/init.d/jackfreqd;\
	fi
	install -m 755 -d $(PREFIX)/share/man/man1
	install -m 644 jackfreqd.1 $(PREFIX)/share/man/man1/jackfreqd.1

uninstall:
	/bin/rm -f $(PREFIX)/sbin/jackfreqd
	/bin/rm -f $(PREFIX)/share/man/man1/jackfreqd.1

purge: uninstall
	/bin/rm -f $(DESTDIR)/etc/init.d/jackfreqd
	/bin/rm -f $(SYSTEMD_UNIT_DIR)/jackfreq.service

clean:
	/bin/rm -f jackfreqd procps busyjack jackxrun

.PHONY: install uninstall purge clean

### test and debug tools ###

procps: procps.c
	gcc -o procps procps.c -DMAIN

busyjack: tools/busyjack.c
	gcc -o busyjack tools/busyjack.c -ljack -Wall

jackxrun: tools/jackxrun.c
	gcc -o jackxrun tools/jackxrun.c -ljack -Wall
