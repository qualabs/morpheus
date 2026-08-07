#Makefile for morphdriver: run morpheus stand-alone for VOD

CC = g++
CFLAGS = -Wall -W -g
SRCS = ngx_morpheus_internal.cpp morpheus_main.cpp
OBJS = $(SRCS:.cpp=.o)

all: morphdriver

morphdriver: $(OBJS) pugixml.o cxxopts.hpp
	$(CC) $(CFLAGS) $(OBJS) pugixml.o -o $@

clean:
	rm -f ngx_morpheus_internal.o morpheus_main.o morphdriver

