cmake_minimum_required(VERSION 3.18)
project(xbdm_gdb_bridge)

enable_testing()
set(CMAKE_CXX_STANDARD 17)

find_package(
        Boost 1.70
        COMPONENTS
        log
        program_options
        unit_test_framework
        REQUIRED
)

include_directories(${Boost_INCLUDE_DIR})

add_executable(
        xbdm_gdb_bridge
        src/main.cpp
        src/discovery/discoverer.cpp
        src/discovery/discoverer.h
        src/gdb/gdb_packet.cpp
        src/gdb/gdb_packet.h
        src/gdb/gdb_transport.cpp
        src/gdb/gdb_transport.h
        src/net/ip_address.cpp
        src/net/ip_address.h
        src/net/delegating_server.cpp
        src/net/delegating_server.h
        src/net/select_thread.cpp
        src/net/select_thread.h
        src/net/tcp_server.cpp
        src/net/tcp_server.h
        src/net/tcp_socket_base.cpp
        src/net/tcp_socket_base.h
        src/net/tcp_connection.cpp
        src/net/tcp_connection.h
        src/notification/xbdm_notification.cpp
        src/notification/xbdm_notification.h
        src/notification/xbdm_notification_transport.cpp
        src/notification/xbdm_notification_transport.h
        src/rdcp/rdcp_processed_request.cpp
        src/rdcp/rdcp_processed_request.h
        src/rdcp/rdcp_request.cpp
        src/rdcp/rdcp_request.h
        src/rdcp/rdcp_response.cpp
        src/rdcp/rdcp_response.h
        src/rdcp/xbdm_requests.h
        src/rdcp/xbdm_transport.cpp
        src/rdcp/xbdm_transport.h
        src/shell/command.cpp
        src/shell/command.h
        src/shell/debugger_commands.h
        src/shell/shell.cpp
        src/shell/shell.h
        src/shell/shell_commands.h
        src/util/timer.cpp
        src/util/timer.h
        src/xbox/xbox_interface.cpp
        src/xbox/xbox_interface.h
)

target_include_directories(
        xbdm_gdb_bridge
        PRIVATE src
)

target_link_libraries(
        xbdm_gdb_bridge
        LINK_PUBLIC
        ${Boost_LIBRARIES}
)

# Tests ----------------------------------------------

add_executable(
        test_rdcp
        test/rdcp/test_rdcp_processed_request.cpp
        test/test_util/vector.h
        src/rdcp/rdcp_processed_request.cpp
        src/rdcp/rdcp_processed_request.h
)
target_include_directories(
        test_rdcp
        PRIVATE src
        PRIVATE test
)
target_link_libraries(
        test_rdcp
        LINK_PUBLIC
        ${Boost_LIBRARIES}
)


add_test(NAME test_rdcp COMMAND test_rdcp)
