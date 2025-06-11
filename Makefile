CC=		cc
CFLAGS=		-std=c99 -O2
CPPFLAGS=	-I/opt/pkg/include -DHAVE_GDBM=1
LDFLAGS=	-L/opt/pkg/lib
LIBS=		-lgdbm -lgd

.PHONY: test
test: counter
	./test_counter.sh

counter: counter.o roman.o image.o keyvalue.o
	${CC} -o $@ ${LDFLAGS} ${LIBS} counter.o roman.o image.o keyvalue.o

.c.o:
	${CC} -c $< ${CFLAGS} ${CPPFLAGS}

.PHONY: clean
clean:
	rm -f counter *.o
