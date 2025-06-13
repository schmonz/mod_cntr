cmake_minimum_required(VERSION 3.10)
project(counter C)

set(CMAKE_C_STANDARD 99)

add_compile_options(-O2 -D_XOPEN_SOURCE=600)

# add_compile_options(-Wall -Wextra -Wunused-variable -Wwrite-strings -Wformat=2 -Wformat-security -Wstack-protector -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fsanitize=address -g)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)
pkg_check_modules(LIBPNG REQUIRED IMPORTED_TARGET libpng)
pkg_check_modules(FASTCGI REQUIRED IMPORTED_TARGET fcgi)

set(SOURCES
    counter.c
    roman.c
    image.c
    keyvalue.c
)

add_executable(counter ${SOURCES})

target_link_libraries(counter PRIVATE PkgConfig::SQLITE3 PkgConfig::LIBPNG PkgConfig::FASTCGI)

enable_testing()

add_test(
    NAME counter_shell_test
    COMMAND ${CMAKE_SOURCE_DIR}/test_counter.sh
    WORKING_DIRECTORY ${CMAKE_BUILD_DIR}
)

set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "*.db")
