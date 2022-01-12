#Makefile for morphdriver: run morpheus stand-alone for VOD

CC = g++
CFLAGS = -Wall -W -g
SRCS = ngx_morpheus_internal.cpp morpheus_main.cpp
OBJS = $(SRCS:.cpp=.o)

all: morphdriver

morphdriver: $(OBJS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

clean:
	rm -f *.o morphdriver

