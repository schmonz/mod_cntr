CPPFLAGS=	-I/opt/pkg/include
LDFLAGS=	-L/opt/pkg/lib
LIBS=		-lgdbm -lgd

.PHONY: test
test: counter
	./test_mod_cntr.sh

counter: mod_cntr.o roman.o image.o keyvalue.o
	cc -o counter ${LDFLAGS} ${LIBS} mod_cntr.o roman.o image.o keyvalue.o

keyvalue.o: keyvalue.c
	cc -Wno-parentheses -c keyvalue.c ${CPPFLAGS} -DHAVE_GDBM=1

image.o: image.c
	cc -Wno-parentheses -c image.c ${CPPFLAGS}

roman.o: roman.c
	cc -c roman.c ${CPPFLAGS}

mod_cntr.o: mod_cntr.c
	cc -c mod_cntr.c ${CPPFLAGS}

.PHONY: clean
clean:
	rm -f counter *.o
