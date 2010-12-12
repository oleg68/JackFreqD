CFLAGS=-Wall -g `pkg-config --cflags jack`
LDFLAGS=`pkg-config --libs jack` -lpthread -lm

jackfreqd: jackfreqd.c jack_cpu_load.c procps.c

procps: procps.c
	gcc -o procps procps.c -DMAIN

jacktest: jacktest.c
	gcc -o jacktest jacktest.c -ljack -Wall

jackxrun: jackxrun.c
	gcc -o jackxrun jackxrun.c -ljack -Wall
