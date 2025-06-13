cmake_minimum_required(VERSION 3.10)
project(counter C)

set(CMAKE_C_STANDARD 99)
add_compile_options(-D_XOPEN_SOURCE=600) # -Wall -Wextra -Werror -Wunused-variable -Wwrite-strings -Wformat=2 -Wformat-security -Wstack-protector -fstack-protector-strong -D_FORTIFY_SOURCE=2)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -g -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
    add_compile_options(-O1)  # Better debugging with ASan
else()
    add_compile_options(-O2)
endif()

find_program(CPPCHECK cppcheck)
if(CPPCHECK)
    message(STATUS "cppcheck found: ${CPPCHECK}")
else()
    message(STATUS "cppcheck not found. cppcheck target will not be available.")
endif()

find_program(SCAN_BUILD scan-build)
if(SCAN_BUILD)
    message(STATUS "scan-build found: ${SCAN_BUILD}")
else()
    message(STATUS "scan-build not found. clang-analyzer target will not be available.")
endif()

find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
    message(STATUS "clang-tidy found: ${CLANG_TIDY}")
else()
    message(STATUS "clang-tidy not found. clang-tidy target will not be available.")
endif()

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

if(CPPCHECK)
    add_custom_target(static-analysis-cppcheck
        COMMAND ${CPPCHECK}
            --enable=all
            --std=c99
            --inconclusive
            --suppress=missingIncludeSystem
            --suppress=unmatchedSuppression
            --suppress=unusedFunction
            --error-exitcode=1
            -I ${CMAKE_SOURCE_DIR}
            ${SOURCES}
        COMMENT "Running cppcheck"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

if(SCAN_BUILD)
    add_custom_target(static-analysis-clang
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/scan-build-results
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/scan-build-results
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
        COMMAND ${SCAN_BUILD} -o ${CMAKE_BINARY_DIR}/scan-build-results ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target counter
        COMMENT "Running clang analyzer"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

if(CLANG_TIDY)
    add_custom_target(static-analysis-clang-tidy
        COMMAND ${CMAKE_COMMAND} -E echo "Running clang-tidy..."
        COMMAND ${CLANG_TIDY}
            -p=${CMAKE_BINARY_DIR}
            -checks=bugprone-*,cert-*,clang-analyzer-*,cppcoreguidelines-*,misc-*,modernize-*,performance-*,portability-*,readability-*,-cert-err34-c,-modernize-macro-to-enum,-readability-function-cognitive-complexity
            ${SOURCES}
        DEPENDS counter
        COMMENT "Running clang-tidy"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
