CFLAGS=-Wall -g `pkg-config --cflags jack`
LDFLAGS=`pkg-config --libs jack`

jackfreqd: jackfreqd.c jack_cpu_load.c procps.c

procps: procps.c
	gcc -o procps procps.c -DMAIN
