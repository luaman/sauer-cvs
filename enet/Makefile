CC=gcc
COPTFLAGS=-ansi -Wall -pedantic -O3 -fomit-frame-pointer
CFLAGS=$(COPTFLAGS) -Iinclude -DHAS_POLL -DHAS_FCNTL -DHAS_MSGHDR_FLAGS
OBJS= \
	host.o \
	list.o \
	memory.o \
	packet.o \
	peer.o \
	protocol.o \
	unix.o \
	win32.o

default: all

all: libenet.a

clean:
	-rm -f libenet.a $(OBJS)

libenet.a: $(OBJS)
	ar ruv libenet.a $(OBJS)
	ranlib libenet.a


