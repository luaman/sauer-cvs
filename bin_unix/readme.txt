*nix versions of cube clients and standalone servers.

The clients function identical to the win32 client, see config.html.
Run them from the root cube dir (chmod em as exe first).
Clients will need the following dynamic link libraries present:
opengl, glu, sdl, sdl_image, sdl_mixer, png, jpeg, zlib (1.2.1 for
all SDL libs, do a ldd for details).

The servers need NO libs, no external files, no sound or video,
just run it :) Server port is fixed at 28765, currently.

Make sure to chmod +x these binaries and the cube_unix script
before running them.

eihrul
eihrul@tunes.org
