CFLAGS=-Wall -g `pkg-config --cflags jack`
LDFLAGS=`pkg-config --libs jack` -lpthread -lm

all: jackfreqd

jackfreqd: jackfreqd.c jack_cpu_load.c procps.c

install: jackfreqd
	install -o root -g root -m 755 -d $(DESTDIR)/usr/sbin
	install -o root -g root -m 755 -s jackfreqd $(DESTDIR)/usr/sbin
	install -o root -g root -m 755 -d $(DESTDIR)/etc/init.d
	install -o root -g root -m 755 jackfreqd.init $(DESTDIR)/etc/init.d/jackfreqd

uninstall:
	/bin/rm $(DESTDIR)/usr/sbin/jackfreqd

purge: uninstall
	/bin/rm $(DESTDIR)/etc/init.d/jackfreqd

clean:
	/bin/rm -f jackfreqd procps jacktest jackxrun

.PHONY: install uninstall purge clean

### test and debug tools ###

procps: procps.c
	gcc -o procps procps.c -DMAIN

jacktest: jacktest.c
	gcc -o jacktest jacktest.c -ljack -Wall

jackxrun: jackxrun.c
	gcc -o jackxrun jackxrun.c -ljack -Wall
