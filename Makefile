#Makefile for morphdriver: run morpheus stand-alone

CC = g++
CFLAGS = -Wall -W -g
SRCS = ngx_morpheus_internal.cpp morpheus_main.cpp

all: morphdriver

morphdriver:
	$(CC) $(CFLAGS) $(SRCS) -o $@

clean:
	rm -f *.o morphdriver

