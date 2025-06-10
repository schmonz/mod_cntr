.PHONY: test
test: counter
	./test_mod_cntr.sh

counter: mod_cntr.o
	cc -o counter -L/opt/pkg/lib -lgdbm -lgd mod_cntr.o

mod_cntr.o: mod_cntr.c
	cc -Wno-parentheses -c mod_cntr.c -I/opt/pkg/include -DHAVE_GDBM=1

.PHONY: clean
clean:
	rm -f counter *.o
