CXX=g++
CXXOPTFLAGS=-O3 -fomit-frame-pointer
CXXFLAGS=$(CXXOPTFLAGS)	-Wall -fsigned-char -Ienet/include -I. -Ishared -Iengine -Ifpsgame `sdl-config --cflags`

PLATFORM_PREFIX=native
CLIENT_LIBS=-Lenet -lenet `sdl-config --libs` -lSDL_image -lSDL_mixer -lz -lGL -lGLU 
CLIENT_OBJS= \
	shared/tools.o \
	shared/geom.o \
	engine/3dgui.o \
	engine/client.o	\
	engine/command.o \
	engine/console.o \
	engine/crypto.o \
	engine/cubeloader.o \
	engine/lightmap.o \
	engine/main.o \
	engine/menus.o \
	engine/normal.o	\
	engine/octa.o \
	engine/octaedit.o \
	engine/octarender.o \
	engine/physics.o \
	engine/renderextras.o \
	engine/rendergl.o \
	engine/rendermodel.o \
	engine/renderparticles.o \
	engine/rendersky.o \
	engine/rendertext.o \
	engine/renderva.o \
	engine/server.o	\
	engine/serverbrowser.o \
	engine/spheretree.o \
	engine/sound.o \
	engine/water.o \
	engine/world.o \
	engine/worldio.o \
	fpsgame/fps.o \
	rpggame/rpg.o

SERVER_LIBS=-Lenet -lenet
SERVER_OBJS= \
	shared/tools.o \
	engine/server-standalone.o \
	fpsgame/fps-standalone.o

default: all

all: libenet client server

enet/Makefile:
	cd enet; ./configure
	
libenet: enet/Makefile
	$(MAKE)	-C enet/ all

clean: enet/Makefile
	-$(RM) $(SERVER_OBJS) $(CLIENT_OBJS) sauer_server sauer_client
	$(MAKE)	-C enet/ clean

engine/server-standalone.o:
	$(CXX) $(CXXFLAGS) -DSTANDALONE	-c -o engine/server-standalone.o engine/server.cpp
	
fpsgame/fps-standalone.o:
	$(CXX) $(CXXFLAGS) -DSTANDALONE	-c -o fpsgame/fps-standalone.o fpsgame/fps.cpp

client:	$(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o sauer_client $(CLIENT_OBJS) $(CLIENT_LIBS)

server:	$(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o sauer_server $(SERVER_OBJS) $(SERVER_LIBS)  
	
install: all
	cp sauer_client	../bin_unix/$(PLATFORM_PREFIX)_client
	cp sauer_server	../bin_unix/$(PLATFORM_PREFIX)_server
	strip ../bin_unix/$(PLATFORM_PREFIX)_client
	strip ../bin_unix/$(PLATFORM_PREFIX)_server
