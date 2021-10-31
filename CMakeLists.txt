cmake_minimum_required(VERSION 3.20)
project(xbdm_gdb_bridge)

set(CMAKE_CXX_STANDARD 17)

FIND_PACKAGE(
        Boost 1.70
        COMPONENTS
        log
        program_options
        REQUIRED
)

INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})

add_executable(
        xbdm_gdb_bridge
        src/main.cpp
        src/discovery/discoverer.cpp
        src/discovery/discoverer.h
        src/net/address.cpp
        src/net/address.h
        src/net/ip_transport.cpp
        src/net/ip_transport.h
        src/util/timer.cpp
        src/util/timer.h
)

target_include_directories(
        xbdm_gdb_bridge
        PRIVATE src
)

TARGET_LINK_LIBRARIES(
        xbdm_gdb_bridge
        LINK_PUBLIC
        ${Boost_LIBRARIES}
)


