header = -I ./include
gxx = gcc

dlls = raylib.dll   \
			 renderer.dll \
			 player.dll   \

build: src/main.c raylib.dll
	$(gxx) src/renderer.c raylib.dll -shared -o renderer.dll $(header)
	$(gxx) src/player.c raylib.dll -shared -o player.dll $(header)

	$(gxx) src/main.c $(dlls) -o app.exe $(header)
	app.exe

build-view: ddf/viewer.c
	$(gxx) ddf/viewer.c raylib.dll -o view.exe $(header)