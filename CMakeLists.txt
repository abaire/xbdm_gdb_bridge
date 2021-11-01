cmake_minimum_required(VERSION 3.18)
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
        src/net/select_thread.cpp
        src/net/select_thread.h
        src/net/tcp_server.cpp
        src/net/tcp_server.h
        src/net/tcp_socket_base.cpp
        src/net/tcp_socket_base.h
        src/net/tcp_connection.cpp
        src/net/tcp_connection.h
        src/rdcp/rdcp_request.cpp
        src/rdcp/rdcp_request.h
        src/rdcp/rdcp_response.cpp
        src/rdcp/rdcp_response.h
        src/rdcp/xbdm_notification.cpp
        src/rdcp/xbdm_notification.h
        src/rdcp/xbdm_notification_transport.cpp
        src/rdcp/xbdm_notification_transport.h
        src/rdcp/xbdm_transport.cpp
        src/rdcp/xbdm_transport.h
        src/shell/shell.cpp
        src/shell/shell.h
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


