.RECIPEPREFIX = >

pass: src/pass.c src/main.c src/upnp.c
> $(CC) src/pass.c src/main.c src/upnp.c src/log.c lib/libminiupnpc.a -o pass

install: pass
> cp pass /usr/local/bin
