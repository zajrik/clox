cmake_minimum_required(VERSION 4.2)
project(clox C)

set(CMAKE_C_STANDARD 11)

include_directories(src include)

add_executable(clox
        include/chunk.h
        include/common.h
        include/compiler.h
        include/debug.h
        include/hash_table.h
        include/memory.h
        include/object.h
        include/scanner.h
        include/value.h
        include/vm.h
        src/chunk.c
        src/clox.c
        src/compiler.c
        src/debug.c
        src/hash_table.c
        src/memory.c
        src/object.c
        src/scanner.c
        src/value.c
        src/vm.c
)
