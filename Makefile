#Makefile for morphdriver: run morpheus stand-alone for VOD

CC = g++
CFLAGS = -Wall -W -g
SRCS = ngx_morpheus_internal.cpp morpheus_main.cpp pugixml.cpp
OBJS = $(SRCS:.cpp=.o)

all: morphdriver

morphdriver: $(OBJS) cxxopts.hpp
	$(CC) $(CFLAGS) $(SRCS) -o $@

clean:
	rm -f *.o morphdriver

