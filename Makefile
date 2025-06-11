CPPFLAGS=	-I/opt/pkg/include
LDFLAGS=	-L/opt/pkg/lib
LIBS=		-lgdbm -lgd

.PHONY: test
test: counter
	./test_counter.sh

counter: counter.o roman.o image.o keyvalue.o
	cc -o counter ${LDFLAGS} ${LIBS} counter.o roman.o image.o keyvalue.o

keyvalue.o: keyvalue.c
	cc -c keyvalue.c ${CPPFLAGS} -DHAVE_GDBM=1

image.o: image.c
	cc -c image.c ${CPPFLAGS}

roman.o: roman.c
	cc -c roman.c ${CPPFLAGS}

counter.o: counter.c
	cc -c counter.c ${CPPFLAGS}

.PHONY: clean
clean:
	rm -f counter *.o
