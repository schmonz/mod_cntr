all: builddir.sh CMakeLists.txt *.c *.h
	build_dir=$$(./builddir.sh); \
	mkdir -p $$build_dir && cd $$build_dir && cmake .. && ${MAKE}

test: builddir.sh all
	build_dir=$$(./builddir.sh); \
	cd $$build_dir && ctest --verbose

clean: builddir.sh
	build_dir=$$(./builddir.sh); \
	rm -rf $$build_dir

.PHONY: all test clean
