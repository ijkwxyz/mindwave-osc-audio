CXX=g++
CC=gcc
CFLAGS=-O3 -DAUDIO_NULL
CXXFLAGS=$(CFLAGS) -std=c++0x
LIBS=

DEPS = AppParams.h RecorderPlayback.h oscpkt.hh

.DEFAULT_GOAL := mw2osc

all: mw2osc

main.o: main.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CXXFLAGS)

tgsp.o: ThinkGearStreamParser.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)    
    
mw2osc: main.o tgsp.o
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)
    
.PHONY: clean

clean:
	rm -f *.o mw2osc
    
