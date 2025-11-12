cmake_minimum_required(VERSION 3.24)

project(client
        VERSION 0.2.1
        DESCRIPTION ""
        LANGUAGES C)

set(CMAKE_C_STANDARD 17)

set(SANITIZE TRUE)

set(SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

set(SOURCE_LIST ${SOURCE_DIR}/main.c
        src/command_line.c
        include/command_line.h
        src/server_config.c
        include/server_config.h
        src/fsm.c
        include/fsm.h
        src/packet_config.c
        include/packet_config.h
        src/protocol.c
        include/protocol.h
        src/linked_list.c
        include/linked_list.h)
set(HEADER_LIST ""
        src/command_line.c
        include/command_line.h
        src/server_config.c
        include/server_config.h
        src/fsm.c
        include/fsm.h
        src/packet_config.c
        include/packet_config.h
        src/protocol.c
        include/protocol.h
        src/linked_list.c
        include/linked_list.h)

add_compile_definitions(_POSIX_C_SOURCE=200809L)
add_compile_definitions(_XOPEN_SOURCE=700)
add_compile_definitions(_GNU_SOURCE)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_compile_definitions(_DARWIN_C_SOURCE)
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    add_compile_definitions(__BSD_VISIBLE)
endif ()

include_directories(${INCLUDE_DIR})
add_compile_options("-Wall"
        "-Wextra"
        "-Wpedantic"
        "-Wshadow"
        "-Wstrict-overflow=4"
        "-Wswitch-default"
        "-Wswitch-enum"
        "-Wunused"
        "-Wunused-macros"
        "-Wdate-time"
        "-Winvalid-pch"
        "-Wmissing-declarations"
        "-Wmissing-include-dirs"
        "-Wmissing-prototypes"
        "-Wstrict-prototypes"
        "-Wundef"
        "-Wnull-dereference"
        "-Wstack-protector"
        "-Wdouble-promotion"
        "-Wvla"
        "-Walloca"
        "-Woverlength-strings"
        "-Wdisabled-optimization"
        "-Winline"
        "-Wcast-qual"
        "-Wfloat-equal"
        "-Wformat=2"
        "-Wfree-nonheap-object"
        "-Wshift-overflow"
        "-Wwrite-strings")

if (${SANITIZE})
    add_compile_options("-fsanitize=address")
    add_compile_options("-fsanitize=undefined")
    add_compile_options("-fsanitize-address-use-after-scope")
    add_compile_options("-fstack-protector-all")
    add_compile_options("-fdelete-null-pointer-checks")
    add_compile_options("-fno-omit-frame-pointer")

    if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        add_compile_options("-fsanitize=leak")
    endif ()

    add_link_options("-fsanitize=address")
    add_link_options("-fsanitize=bounds")
endif ()


if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    #    add_compile_options("-O2")
    add_compile_options("-Wunsuffixed-float-constants"
            "-Warith-conversion"
            "-Wunsafe-loop-optimizations"
            "-Wvector-operation-performance"
            "-Walloc-zero"
            "-Wtrampolines"
            "-Wtsan"
            "-Wformat-overflow=2"
            "-Wformat-signedness"
            "-Wjump-misses-init"
            "-Wformat-truncation=2")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
endif ()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(client ${SOURCE_LIST} ${HEADER_LIST})
target_include_directories(client PRIVATE include)
target_include_directories(client PRIVATE /usr/local/include)
target_link_directories(client PRIVATE /usr/local/lib)

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_include_directories(client PRIVATE /usr/include)
endif ()

set_target_properties(client PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR})

set_target_properties(client PROPERTIES OUTPUT_NAME "client")
install(TARGETS client DESTINATION bin)

