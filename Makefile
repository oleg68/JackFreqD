CFLAGS=-Wall `pkg-config --cflags jack`
LDFLAGS=`pkg-config --libs jack`

cpu_freq: cpu_freq.c jack_cpu_load.c
