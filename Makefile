CC=		cc
CFLAGS=		-std=c99
CPPFLAGS=	-I/opt/pkg/include
LDFLAGS=	-L/opt/pkg/lib
LIBS=		-lgdbm -lgd

.PHONY: test
test: counter
	./test_counter.sh

counter: counter.o roman.o image.o keyvalue.o
	${CC} ${LDFLAGS} ${LIBS} counter.o roman.o image.o keyvalue.o -o counter

keyvalue.o: keyvalue.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c keyvalue.c -DHAVE_GDBM=1

image.o: image.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c image.c

roman.o: roman.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c roman.c

counter.o: counter.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c counter.c

.PHONY: clean
clean:
	rm -f counter *.o
