cmake_minimum_required(VERSION 3.13)
project(Server C)
project(Client C)

set(CMAKE_C_STANDARD 11)

FILE(GLOB ECHO_SOURCE "echo/*.c")
FILE(GLOB ECHO_HEADER "echo/*.h")
FILE(GLOB MULP_SOURCE "mul_process/*.c")
FILE(GLOB MULP_HEADER "mul_process/*.h")

add_executable(Server
        global.h
        global.c
        ${ECHO_SOURCE}
        ${ECHO_HEADER}
        ${MULP_SOURCE}
        ${MULP_HEADER}
        server_main.c)

add_executable(Client
        global.h
        global.c
        ${ECHO_SOURCE}
        ${ECHO_SOURCE}
        ${MULP_SOURCE}
        ${MULP_HEADER}
        client_main.c)