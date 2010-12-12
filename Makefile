CFLAGS=-Wall -g `pkg-config --cflags jack`
LDFLAGS=`pkg-config --libs jack` -lpthread -lm

all: jackfreqd

jackfreqd: jackfreqd.c jack_cpu_load.c procps.c

install: jackfreqd
	install -m 755 jackfreqd /usr/sbin

uninstall:
	rm /usr/sbin/jackfreqd

clean:
	rm -rf jackfreqd procps jacktest jackxrun

.PHONY: install uninstall clean

### test and debug tools ###

procps: procps.c
	gcc -o procps procps.c -DMAIN

jacktest: jacktest.c
	gcc -o jacktest jacktest.c -ljack -Wall

jackxrun: jackxrun.c
	gcc -o jackxrun jackxrun.c -ljack -Wall
