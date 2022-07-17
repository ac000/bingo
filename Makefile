CC       = gcc
CFLAGS  += -Wall -Wextra -Wno-deprecated-declarations -Wvla -std=gnu99 -g -O2 -fstack-protector-strong -fPIC `pkg-config --cflags gtk+-3.0 glib-2.0 gmodule-2.0`
LDFLAGS += -Wl,-z,defs,-z,relro,-z,now,--as-needed -pie
LIBS     = `pkg-config --libs gtk+-3.0 glib-2.0 gmodule-2.0` -lac -lrt

bingo: bingo.o
	$(CC) ${LDFLAGS} -o bingo bingo.o ${LIBS}

bingo.o: bingo.c
	$(CC) $(CFLAGS) -c bingo.c

clean:
	rm -f bingo *.o
