CC=gcc
CFLAGS=-Wall -Wextra -g -O2 -fstack-protector-strong -fPIC -Wno-deprecated-declarations
LDFLAGS=-Wl,-z,now -pie
LIBS=`pkg-config --libs gtk+-3.0 glib-2.0 gmodule-2.0` -lac -lrt
INCS=`pkg-config --cflags gtk+-3.0 glib-2.0 gmodule-2.0`

bingo: bingo.o
	$(CC) ${LDFLAGS} -o bingo bingo.o ${LIBS}

bingo.o: bingo.c
	$(CC) $(CFLAGS) -c bingo.c ${INCS}

clean:
	rm -f bingo *.o
