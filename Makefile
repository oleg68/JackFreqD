SBIN_DIR?=/usr/sbin
MAN_DIR?=/usr/share/man
SYS_CONF_DIR?=/etc
SYSTEMD_UNIT_DIR?=$(SYS_CONF_DIR)/systemd/system
INITD_DIR?=$(SYS_CONF_DIR)/rc.d/init.d

LDFLAGS?=-Wl,--as-needed
SYSTEMCTL:=$(shell which systemctl)

	
override CFLAGS+=-Wall -g `pkg-config --cflags jack`
LOADLIBES=`pkg-config --libs jack` -lpthread -lrt -lm -Wl,--as-needed

all: jackfreqd

jackfreqd: jackfreqd.c jack_cpu_load.c procps.c

install: jackfreqd
	install -m 755 -d $(DESTDIR)$(SBIN_DIR)
	install -m 755 -s jackfreqd $(DESTDIR)$(SBIN_DIR)

	@if test -x "$(SYSTEMCTL)" && test -d "$(SYSTEMD_UNIT_DIR)"; then\
	  install -m 755 -d $(DESTDIR)$(SYSTEMD_UNIT_DIR);\
	  install -m 644 jackfreq.service $(DESTDIR)$(SYSTEMD_UNIT_DIR)/jackfreq.service;\
	else\
	  install -m 755 -d $(DESTDIR)$(INITD_DIR);\
	  install -m 755 jackfreqd.init $(DESTDIR)$(INITD_DIR)/jackfreqd;\
	fi
	install -m 755 -d $(DESTDIR)$(MAN_DIR)/man1
	install -m 644 jackfreqd.1 $(DESTDIR)$(MAN_DIR)/man1/jackfreqd.1

uninstall:
	/bin/rm -f $(DESTDIR)$(SBIN_DIR)/jackfreqd
	/bin/rm -f $(DESTDIR)$(MAN_DIR)/man1/jackfreqd.1

purge: uninstall
	/bin/rm -f $(DESTDIR)$(INITD_DIR)/jackfreqd
	/bin/rm -f $(DESTDIR)$(SYSTEMD_UNIT_DIR)/jackfreq.service

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
