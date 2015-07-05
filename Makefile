CPP=g++
CC=gcc
CFLAGS=-O3 -DAUDIO_NULL
LIBS=

DEPS = AppParams.h RecorderPlayback.h oscpkt.hh

.DEFAULT_GOAL := mw2osc

all: mw2osc

main.o: main.cpp $(DEPS)
	$(CPP) -c -o $@ $< $(CFLAGS)

tgsp.o: ThinkGearStreamParser.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)    
    
mw2osc: main.o tgsp.o
	$(CPP) -o $@ $^ $(CFLAGS) $(LIBS)
    
.PHONY: clean

clean:
	rm -f *.o mw2osc
    