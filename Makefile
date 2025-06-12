CC=		cc
CFLAGS=		-std=c99 -O2 # -Wall -Wextra -Wunused-variable -Wwrite-strings -Wformat=2 -Wformat-security -Wstack-protector -fstack-protector-strong -D_FORTIFY_SOURCE=2 # -fsanitize=address -g
CPPFLAGS!=	pkg-config --cflags sqlite3 libpng
CPPFLAGS+=	-D_XOPEN_SOURCE=600
LDFLAGS!=	pkg-config --libs sqlite3 libpng

test: counter
	./test_counter.sh

counter: counter.o roman.o image.o keyvalue.o
	${CC} -o $@ counter.o roman.o image.o keyvalue.o ${LDFLAGS}

.c.o:
	${CC} -c $< ${CFLAGS} ${CPPFLAGS}

clean:
	rm -f counter *.o *.db

.PHONY: test clean
