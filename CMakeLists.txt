cmake_minimum_required(VERSION 3.18)
project(xbdm_gdb_bridge)

set(CMAKE_VERBOSE_MAKEFILE TRUE)

enable_testing()
set(CMAKE_CXX_STANDARD 17)
include(CMakePushCheckState)
set(_CMAKE_PROCESSING_LANGUAGE "C")
include(CheckSymbolExists)
include(FindPkgConfig)

find_package(
        Boost 1.70
        COMPONENTS
        log
        program_options
        unit_test_framework
        REQUIRED
)

PKG_SEARCH_MODULE(SDL2 REQUIRED sdl2)
PKG_SEARCH_MODULE(SDL2IMAGE REQUIRED SDL2_image>=2.0.0)

set(GENERATED_FILES_DIR "${CMAKE_BINARY_DIR}/generated")

include_directories(${SDL2_INCLUDE_DIRS} ${SDL2IMAGE_INCLUDE_DIRS})

option(ENABLE_HIGH_VERBOSITY_LOGGING  "Enable low level, highly verbose logging")

include_directories("${Boost_INCLUDE_DIR}" "${GENERATED_FILES_DIR}")

cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_LIBRARIES ${SOCKET_LIBRARIES})
if (WIN32)
    check_symbol_exists(htonll Winsock2.h HAVE_HTONLL)
else (WIN32)
    check_symbol_exists(htonll arpa/inet.h HAVE_HTONLL)
endif (WIN32)
cmake_pop_check_state()

cmake_push_check_state(RESET)
check_symbol_exists(be64toh endian.h HAVE_BE64TOH)
cmake_pop_check_state()

set(CMAKE_CXX_FLAGS_DEBUG "-ggdb -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

configure_file("src/configure.h.cmake" "${GENERATED_FILES_DIR}/configure.h")

# Resource packer ------------------------------------
add_executable(resource_packer EXCLUDE_FROM_ALL util/resource_packer/resource_packer.cpp)

# Bootstrap packing ----------------------------------

add_custom_command(
        OUTPUT "${GENERATED_FILES_DIR}/bootstrap_l1.h"
        COMMAND resource_packer
            "${CMAKE_BINARY_DIR}/bootstrap_l1.asm.o"
            "${GENERATED_FILES_DIR}/bootstrap_l1.h"
            "kBootstrapL1"
        DEPENDS "${CMAKE_BINARY_DIR}/bootstrap_l1.asm.o"
        VERBATIM
)

add_custom_command(
        OUTPUT "${GENERATED_FILES_DIR}/bootstrap_l2_thunk.h"
        COMMAND resource_packer
            "${CMAKE_BINARY_DIR}/bootstrap_l2_thunk.asm.o"
            "${GENERATED_FILES_DIR}/bootstrap_l2_thunk.h"
            "kBootstrapL2Thunk"
        DEPENDS "${CMAKE_BINARY_DIR}/bootstrap_l2_thunk.asm.o"
        VERBATIM
)

# xbdm executable ------------------------------------

add_executable(
        xbdm_gdb_bridge
        src/main.cpp
        src/discovery/discoverer.cpp
        src/discovery/discoverer.h
        src/gdb/gdb_packet.cpp
        src/gdb/gdb_packet.h
        src/gdb/gdb_transport.cpp
        src/gdb/gdb_transport.h
        src/handler_loader/coff_loader.cpp
        src/handler_loader/coff_loader.h
        src/handler_loader/dxt_library.cpp
        src/handler_loader/dxt_library.h
        src/handler_loader/handler_loader.cpp
        src/handler_loader/handler_loader.h
        src/handler_loader/handler_requests.h
        src/handler_loader/xbdm_exports.cpp
        src/handler_loader/xbdm_exports.def.h
        src/handler_loader/xbdm_exports.h
        src/handler_loader/xboxkrnl_exports.cpp
        src/handler_loader/xboxkrnl_exports.def.h
        src/handler_loader/xboxkrnl_exports.h
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
        src/rdcp/rdcp_response_processors.cpp
        src/rdcp/rdcp_response_processors.h
        src/rdcp/xbdm_requests.cpp
        src/rdcp/xbdm_requests.h
        src/rdcp/xbdm_stop_reasons.h
        src/rdcp/types/thread_context.h
        src/rdcp/xbdm_transport.cpp
        src/rdcp/xbdm_transport.h
        src/shell/command.cpp
        src/shell/command.h
        src/shell/commands.cpp
        src/shell/commands.h
        src/shell/debugger_commands.cpp
        src/shell/debugger_commands.h
        src/shell/handler_commands.cpp
        src/shell/handler_commands.h
        src/shell/screenshot_converter.cpp
        src/shell/screenshot_converter.h
        src/shell/shell.cpp
        src/shell/shell.h
        src/shell/shell_commands.h
        src/util/logging.cpp
        src/util/logging.h
        src/util/optional.h
        src/util/parsing.cpp
        src/util/parsing.h
        src/util/path.cpp
        src/util/path.h
        src/util/timer.cpp
        src/util/timer.h
        src/xbox/xbdm_context.cpp
        src/xbox/xbdm_context.h
        src/xbox/xbox_interface.cpp
        src/xbox/xbox_interface.h
        src/xbox/bridge/gdb_bridge.cpp
        src/xbox/bridge/gdb_bridge.h
        src/xbox/bridge/gdb_registers.cpp
        src/xbox/bridge/gdb_registers.h
        src/rdcp/types/memory_region.cpp
        src/rdcp/types/memory_region.h
        src/rdcp/types/execution_state.h
        src/rdcp/types/module.cpp
        src/rdcp/types/module.h
        src/rdcp/types/section.cpp
        src/rdcp/types/section.h
        src/xbox/debugger/thread.cpp
        src/xbox/debugger/thread.h
        src/xbox/debugger/xbdm_debugger.cpp
        src/xbox/debugger/xbdm_debugger.h
        third_party/nxdk/winapi/winnt.h
        third_party/nxdk/xboxkrnl/xboxdef.h
        ${GENERATED_FILES_DIR}/bootstrap_l1.h
        ${GENERATED_FILES_DIR}/bootstrap_l2_thunk.h
)

target_include_directories(
        xbdm_gdb_bridge
        PRIVATE
        handler_loader_bootstrap
        src
        third_party/nxdk
)

target_link_libraries(
        xbdm_gdb_bridge
        LINK_PUBLIC
        ${Boost_LIBRARIES}
        ${SDL2_LINK_LIBRARIES}
        ${SDL2IMAGE_LINK_LIBRARIES}
)

add_dependencies(xbdm_gdb_bridge bootstrap_l1 bootstrap_l2_thunk)

# Tests ----------------------------------------------

add_library(
        test_util
        test/test_util/vector.cpp
        test/test_util/vector.h
)

# rdcp_tests
add_executable(
        rdcp_tests
        test/rdcp/test_main.cpp
        test/rdcp/test_rdcp_processed_request.cpp
        test/rdcp/test_xbdm_requests.cpp
        src/rdcp/rdcp_processed_request.cpp
        src/rdcp/rdcp_processed_request.h
        src/rdcp/rdcp_response_processors.cpp
        src/rdcp/rdcp_response_processors.h
        src/rdcp/xbdm_requests.h
        src/util/optional.h
        src/util/parsing.cpp
        src/util/parsing.h
)
target_include_directories(
        rdcp_tests
        PRIVATE src
        PRIVATE test
)
target_link_libraries(
        rdcp_tests
        LINK_PRIVATE
        ${Boost_LIBRARIES}
        LINK_PRIVATE
        test_util
)
add_test(NAME rdcp_tests COMMAND rdcp_tests)

# util_tests
add_executable(
        util_tests
        test/util/test_main.cpp
        test/util/test_parsing.cpp
        test/util/test_path.cpp
        src/util/parsing.cpp
        src/util/parsing.h
        src/util/path.cpp
        src/util/path.h
)
target_include_directories(
        util_tests
        PRIVATE src
        PRIVATE test
)
target_link_libraries(
        util_tests
        LINK_PRIVATE
        ${Boost_LIBRARIES}
        LINK_PRIVATE
        test_util
)
add_test(NAME util_tests COMMAND util_tests)

# coff_loader_tests
add_executable(
        coff_loader_tests
        test/coff_loader/test_coff_loader.cpp
        test/coff_loader/test_main.cpp
        src/handler_loader/coff_loader.cpp
        src/handler_loader/coff_loader.h
        src/handler_loader/dxt_library.cpp
        src/handler_loader/dxt_library.h
)
target_include_directories(
        coff_loader_tests
        PRIVATE src
        PRIVATE test
        PRIVATE third_party/nxdk
)
target_link_libraries(
        coff_loader_tests
        LINK_PRIVATE
        ${Boost_LIBRARIES}
        LINK_PRIVATE
        test_util
)
add_test(NAME coff_loader_tests COMMAND coff_loader_tests)
