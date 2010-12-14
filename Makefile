CFLAGS+=-Wall -g `pkg-config --cflags jack`
LDFLAGS+=`pkg-config --libs jack` -lpthread -lm -Wl,--as-needed

all: jackfreqd

jackfreqd: jackfreqd.c jack_cpu_load.c procps.c

install: jackfreqd
	install -o root -g root -m 755 -d $(DESTDIR)/usr/sbin
	install -o root -g root -m 755 -s jackfreqd $(DESTDIR)/usr/sbin
	install -o root -g root -m 755 -d $(DESTDIR)/etc/init.d
	install -o root -g root -m 755 jackfreqd.init $(DESTDIR)/etc/init.d/jackfreqd
	install -o root -g root -m 755 -d $(DESTDIR)/usr/share/man/man1
	install -o root -g root -m 644 jackfreqd.1 $(DESTDIR)/usr/share/man/man1/jackfreqd.1

uninstall:
	/bin/rm -f $(DESTDIR)/usr/sbin/jackfreqd
	/bin/rm -f $(DESTDIR)/usr/share/man/man1/jackfreqd.1

purge: uninstall
	/bin/rm -f $(DESTDIR)/etc/init.d/jackfreqd

clean:
	/bin/rm -f jackfreqd procps jacktest jackxrun

.PHONY: install uninstall purge clean

### test and debug tools ###

procps: procps.c
	gcc -o procps procps.c -DMAIN

busyjack: tools/busyjack.c
	gcc -o busyjack tools/busyjack.c -ljack -Wall

jackxrun: tools/jackxrun.c
	gcc -o jackxrun tools/jackxrun.c -ljack -Wall
