.RECIPEPREFIX = >

OS = $(shell uname -s)

libfile=lib/libminiupnpc.a
ifeq ($(OS), Darwin)
libfile=lib/mac-libminiupnpc.a
endif

pass: src/pass.c src/main.c src/upnp.c
> $(CC) src/pass.c src/main.c src/upnp.c $(libfile) -o pass

install: pass
> cp pass /usr/local/bin
