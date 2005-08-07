CXX=g++
CXXOPTFLAGS=-Wall -O3 -fsigned-char -fomit-frame-pointer
CXXFLAGS=$(CXXOPTFLAGS) -I../enet/include `sdl-config --cflags`

PLATFORM_PREFIX=linux
CLIENT_LIBS=-L../enet -lenet `sdl-config --libs` -lSDL_image -lSDL_mixer -lz -lGL -lGLU 
CLIENT_OBJS= \
	client.o \
	clientextras.o \
	clientgame.o \
	command.o \
	console.o \
	entities.o \
	main.o \
	menus.o \
	monster.o \
	physics.o \
	renderextras.o \
	rendergl.o \
	rendermd2.o \
	renderparticles.o \
	rendertext.o \
	server.o \
	serverbrowser.o \
	sound.o \
	tools.o \
	weapon.o \
	world.o \
	worldio.o \
        geom.o \
        octa.o \
        octaedit.o \
	octarender.o \
	lightmap.o \
	water.o

SERVER_LIBS=-L../enet -lenet
SERVER_OBJS= \
	standalone.o \
	tools.o

default: all

all: enet client server

enet:
	-make -C ../enet all

clean:
	-rm -f $(SERVER_OBJS) $(CLIENT_OBJS) sauer_server sauer_client
	-make -C ../enet/ clean

standalone.o:
	$(CXX) $(CXXFLAGS) -DSTANDALONE -o standalone.o -c server.cpp

client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o sauer_client $(CLIENT_OBJS) $(CLIENT_LIBS)

server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o sauer_server $(SERVER_OBJS) $(SERVER_LIBS)  
	
install: all
	cp sauer_client ../sauerbraten/bin_unix/$(PLATFORM_PREFIX)_client
	strip ../sauerbraten/bin_unix/$(PLATFORM_PREFIX)_client
	cp sauer_server ../sauerbraten/bin_unix/$(PLATFORM_PREFIX)_server
	strip ../sauerbraten/bin_unix/$(PLATFORM_PREFIX)_server
