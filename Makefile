CC=		cc
CFLAGS=		-std=c99 -O2
CPPFLAGS!=	pkg-config --cflags sqlite3 libpng
LDFLAGS!=	pkg-config --libs sqlite3 libpng

test: counter
	./test_counter.sh

counter: counter.o roman.o image.o keyvalue.o
	${CC} -o $@ ${LDFLAGS} counter.o roman.o image.o keyvalue.o

.c.o:
	${CC} -c $< ${CFLAGS} ${CPPFLAGS}

clean:
	rm -f counter *.o *.db

.PHONY: test clean
