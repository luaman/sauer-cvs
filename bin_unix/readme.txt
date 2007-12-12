*nix versions of cube clients and standalone servers.
The clients function identical to the win32 client, see config.html.

Please run "sauerbraten_unix" from the root Sauerbraten dir to launch these.
Clients will need the following dynamic link libraries present:
* libGL (OpenGL)
* libGLU (OpenGL)
* SDL (>= 1.2.10)
* SDL_image (>= 1.2.10)
* SDL_mixer (>= 1.2.10)
* libpng
* libjpeg
* zlib

If native binaries for your platform are not included, then try the following:
1) Ensure you have the DEVELOPMENT VERSIONS of the above libraries installed.
2) Change to the src/ directory and type "make install".
3) Re-run the "sauerbraten_unix" script from the root Sauerbraten directory if it succeeded.

The servers (bin_unix/linux_server or bin_unix/native_server) should need no libs 
other than libstdc++ and zlib, no external files, no sound or video - just run it. 
Server ports are fixed at UDP 28785 and UDP 28786, currently.

Note that the "sauerbraten_unix" script is set up to write any files (saved maps, configs, etc.)
into the user's home directory at "~/.sauerbraten".
