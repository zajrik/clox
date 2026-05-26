cmake_minimum_required(VERSION 4.2)
project(clox C)

set(CMAKE_C_STANDARD 11)

include_directories(src src/headers)

add_executable(clox
        src/headers/chunk.h
        src/headers/common.h
        src/headers/compiler.h
        src/headers/debug.h
        src/headers/memory.h
        src/headers/scanner.h
        src/headers/value.h
        src/headers/vm.h
        src/chunk.c
        src/clox.c
        src/compiler.c
        src/debug.c
        src/scanner.c
        src/value.c
        src/vm.c)
