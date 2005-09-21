CXX=g++
CXXOPTFLAGS=-Wall -fsigned-char -O0 -fomit-frame-pointer
CXXFLAGS=$(CXXOPTFLAGS) -I../enet/include -I. -Ishared -Iengine -Ifpsgame `sdl-config --cflags`

PLATFORM_PREFIX=linux
CLIENT_LIBS=-L../enet -lenet `sdl-config --libs` -lSDL_image -lSDL_mixer -lz -lGL -lGLU 
CLIENT_OBJS= \
	shared/tools.o \
	engine/client.o \
	engine/command.o \
	engine/console.o \
	engine/cubeloader.o \
	engine/lightmap.o \
	engine/main.o \
	engine/menus.o \
	engine/octa.o \
	engine/octaedit.o \
	engine/octarender.o \
	engine/physics.o \
	engine/renderextras.o \
	engine/rendergl.o \
	engine/rendermodel.o \
	engine/renderparticles.o \
	engine/rendertext.o \
	engine/server.o \
	engine/serverbrowser.o \
	engine/sound.o \
	engine/water.o \
	engine/world.o \
	engine/worldio.o \
	fpsgame/fps.o \

SERVER_LIBS=-L../enet -lenet
SERVER_OBJS= \
	shared/tools.o \
	engine/server-standalone.o \
	fpsgame/fps-standalone.o

default: all

all: enet client server

enet:
	-make -C ../enet all

clean:
	-rm -f $(SERVER_OBJS) $(CLIENT_OBJS) sauer_server sauer_client
	-make -C ../enet/ clean

engine/server-standalone.o:
	$(CXX) $(CXXFLAGS) -DSTANDALONE -c -o engine/server-standalone.o engine/server.cpp
	
fpsgame/fps-standalone.o:
	$(CXX) $(CXXFLAGS) -DSTANDALONE -c -o fpsgame/fps-standalone.o fpsgame/fps.cpp

client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -g -o sauer_client $(CLIENT_OBJS) $(CLIENT_LIBS)

server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o sauer_server $(SERVER_OBJS) $(SERVER_LIBS)  
	
install: all
	cp sauer_client ../sauerbraten/bin_unix/$(PLATFORM_PREFIX)_client
	cp sauer_server ../sauerbraten/bin_unix/$(PLATFORM_PREFIX)_server
	strip ../sauerbraten/bin_unix/$(PLATFORM_PREFIX)_server
