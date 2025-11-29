cmake_minimum_required(VERSION 3.18)

project(xbdm_gdb_bridge)

set(CMAKE_VERBOSE_MAKEFILE TRUE)

enable_testing()
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CMakePushCheckState)
set(_CMAKE_PROCESSING_LANGUAGE "C")
include(CheckSymbolExists)
include(ExternalProject)
include(FindPkgConfig)

set(NXDK_DIR $ENV{NXDK_DIR} CACHE PATH "Path to the nxdk installation root")

if (NOT NXDK_DIR)
    message(FATAL_ERROR "Error: NXDK_DIR is not set. "
            "Please set the NXDK_DIR environment variable, "
            "or pass -DNXDK_DIR=/path/to/nxdk to cmake.")
endif ()
set(ENV{NXDK_DIR} ${NXDK_DIR})

set(TEST_TIMEOUT_SECONDS 2 CACHE INTERNAL "Default timeout for tests, in seconds")
set(TEST_MOCK_XBDM_PORT 0 CACHE INTERNAL "Port used for the mock XBDM server in tests")

ExternalProject_Add(
        dyndxt_loader
        GIT_REPOSITORY https://github.com/abaire/nxdk_dyndxt.git
        GIT_TAG f7c6bceab191ea7218920bb662eb5132dfb212ea
        CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${NXDK_DIR}/share/toolchain-nxdk.cmake
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DBUILD_TESTING=OFF
        BUILD_BYPRODUCTS
        <INSTALL_DIR>/lib/bootstrap_l1.asm.obj
        <INSTALL_DIR>/lib/bootstrap_l2.asm.obj
        <INSTALL_DIR>/lib/dynamic_dxt_loader.lib
        <INSTALL_DIR>/lib/libdynamic_dxt_loader.dll
        <INSTALL_DIR>/lib/xbdm.lib
)
ExternalProject_Get_Property(dyndxt_loader INSTALL_DIR)
set(dyndxt_lib_dir ${INSTALL_DIR}/lib)
set(dyndxt_include_dir ${INSTALL_DIR}/include)

# Import the nvtrace DynamicDXT
ExternalProject_Add(
        ntrc_dyndxt
        GIT_REPOSITORY https://github.com/abaire/nxdk_ntrc_dyndxt.git
        GIT_TAG 26d708c4b4e2d96119bfd2628ede88b310f62ce6
        CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${NXDK_DIR}/share/toolchain-nxdk.cmake
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DBUILD_TESTING=OFF
        BUILD_BYPRODUCTS
        <INSTALL_DIR>/lib/libntrc_dyndxt.dll
        <INSTALL_DIR>/include/ntrc_dyndxt.h
)
ExternalProject_Get_Property(ntrc_dyndxt INSTALL_DIR)
set(ntrc_dyndxt_lib_dir ${INSTALL_DIR}/lib)
set(ntrc_dyndxt_include_dir ${INSTALL_DIR}/include)

set(REPLXX_FILENAME "libreplxx.a")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(REPLXX_FILENAME "libreplxx-d.a")
endif()
ExternalProject_Add(
        replxx
        GIT_REPOSITORY https://github.com/AmokHuginnsson/replxx.git
        GIT_TAG release-0.0.4
        CMAKE_ARGS
        -DREPLXX_BUILD_EXAMPLES=OFF
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_DEBUG_POSTFIX=
        BUILD_BYPRODUCTS
        <INSTALL_DIR>/lib/${REPLXX_FILENAME}
        <INSTALL_DIR>/include/replxx.h
)
ExternalProject_Get_Property(replxx INSTALL_DIR)
set(replxx_include_dir ${INSTALL_DIR}/include)
set(REPLXX_STATIC_LIB ${INSTALL_DIR}/lib/${REPLXX_FILENAME})
add_library(libreplxx STATIC IMPORTED GLOBAL)
set_target_properties(libreplxx PROPERTIES IMPORTED_LOCATION "${REPLXX_STATIC_LIB}")
add_dependencies(libreplxx replxx)

ExternalProject_Add(
        capstone
        GIT_REPOSITORY https://github.com/capstone-engine/capstone.git
        GIT_TAG 5.0.6
        CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCAPSTONE_BUILD_CSTOOL=OFF
        -DCAPSTONE_ARCHITECTURE_DEFAULT=OFF
        -DCAPSTONE_X86_SUPPORT=ON
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        BUILD_BYPRODUCTS
        <INSTALL_DIR>/lib/libcapstone.a
        <INSTALL_DIR>/include/capstone/capstone.h
)
ExternalProject_Get_Property(capstone INSTALL_DIR)
set(capstone_include_dir ${INSTALL_DIR}/include)
add_library(libcapstone STATIC IMPORTED GLOBAL)
set_target_properties(libcapstone PROPERTIES IMPORTED_LOCATION "${INSTALL_DIR}/lib/libcapstone.a")
add_dependencies(libcapstone capstone)


if (POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif ()
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

option(ENABLE_HIGH_VERBOSITY_LOGGING "Enable low level, highly verbose logging")

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
configure_file("test/configure_test.h.cmake" "${GENERATED_FILES_DIR}/configure_test.h")

include_directories("${GENERATED_FILES_DIR}")

# Resource packer ------------------------------------
add_executable(resource_packer EXCLUDE_FROM_ALL util/resource_packer/resource_packer.cpp)

function(pack_resource input output symbol_name)
    add_custom_command(
            OUTPUT ${output}
            COMMAND resource_packer
            ${input}
            ${output}
            ${symbol_name}
            DEPENDS ${input}
            VERBATIM
    )
endfunction()

# Bootstrap packing ----------------------------------

pack_resource(
        ${dyndxt_lib_dir}/bootstrap_l1.asm.obj
        ${GENERATED_FILES_DIR}/bootstrap_l1_xbox.h
        kBootstrapL1
)

pack_resource(
        ${dyndxt_lib_dir}/bootstrap_l2.asm.obj
        ${GENERATED_FILES_DIR}/bootstrap_l2_xbox.h
        kBootstrapL2
)

pack_resource(
        ${dyndxt_lib_dir}/libdynamic_dxt_loader.dll
        ${GENERATED_FILES_DIR}/dynamic_dxt_loader_xbox.h
        kDynDXTLoader
)

# ntrc tracer packing --------------------------------

pack_resource(
        ${ntrc_dyndxt_lib_dir}/libntrc_dyndxt.dll
        ${GENERATED_FILES_DIR}/ntrc_dyndxt_xbox.h
        kNTRCDynDXT
)


# lodepng --------------------------------------------

add_library(
        lodepng
        third_party/lodepng/lodepng.cpp
        third_party/lodepng/lodepng.h
)

target_compile_definitions(
        lodepng
        PRIVATE
        LODEPNG_COMPILE_ENCODER
)

target_include_directories(
        lodepng
        PUBLIC
        third_party/lodepng
)

# xbdm executable ------------------------------------
add_library(
        xbdm_gdb_bridge_dyndxt_loader
        STATIC
        src/dyndxt_loader/dll_linker.cpp
        src/dyndxt_loader/dll_linker.h
        src/dyndxt_loader/dxt_library.cpp
        src/dyndxt_loader/dxt_library.h
        src/dyndxt_loader/dyndxt_requests.cpp
        src/dyndxt_loader/dyndxt_requests.h
        src/dyndxt_loader/loader.cpp
        src/dyndxt_loader/loader.h
        src/dyndxt_loader/ResolveExportList.cpp
        src/dyndxt_loader/ResolveExportList.h
        src/dyndxt_loader/xbdm_exports.cpp
        src/dyndxt_loader/xbdm_exports.def.h
        src/dyndxt_loader/xbdm_exports.h
        src/dyndxt_loader/xboxkrnl_exports.cpp
        src/dyndxt_loader/xboxkrnl_exports.def.h
        src/dyndxt_loader/xboxkrnl_exports.h
        ${GENERATED_FILES_DIR}/bootstrap_l1_xbox.h
        ${GENERATED_FILES_DIR}/bootstrap_l2_xbox.h
        ${GENERATED_FILES_DIR}/dynamic_dxt_loader_xbox.h
)
target_include_directories(
        xbdm_gdb_bridge_dyndxt_loader
        PUBLIC
        third_party/nxdk
        ${ntrc_dyndxt_include_dir}
        PRIVATE
        src
)
target_link_libraries(
        xbdm_gdb_bridge_dyndxt_loader
        LINK_PUBLIC
        Boost::log
        ${RC_DEPENDS}
        xbdm_gdb_bridge_xbox_xbdm_context
)
add_dependencies(xbdm_gdb_bridge_dyndxt_loader dyndxt_loader)


add_library(
        xbdm_gdb_bridge_gdb
        STATIC
        src/gdb/gdb_packet.cpp
        src/gdb/gdb_packet.h
        src/gdb/gdb_transport.cpp
        src/gdb/gdb_transport.h
)
target_include_directories(
        xbdm_gdb_bridge_gdb
        PRIVATE
        src
)
target_link_libraries(
        xbdm_gdb_bridge_gdb
        LINK_PUBLIC
        Boost::log
)


add_library(
        xbdm_gdb_bridge_net
        STATIC
        src/net/delegating_server.cpp
        src/net/delegating_server.h
        src/net/ip_address.cpp
        src/net/ip_address.h
        src/net/select_thread.cpp
        src/net/select_thread.h
        src/net/selectable_base.cpp
        src/net/selectable_base.h
        src/net/signaling_base.cpp
        src/net/signaling_base.h
        src/net/task_connection.cpp
        src/net/task_connection.h
        src/net/tcp_connection.cpp
        src/net/tcp_connection.h
        src/net/tcp_server.cpp
        src/net/tcp_server.h
        src/net/tcp_socket_base.cpp
        src/net/tcp_socket_base.h
        src/util/thread_debug_util.cpp
        src/util/thread_debug_util.h
)
target_include_directories(
        xbdm_gdb_bridge_net
        PRIVATE
        src
)
target_link_libraries(
        xbdm_gdb_bridge_net
        LINK_PUBLIC
        Boost::log
        xbdm_gdb_bridge_util
)


add_library(
        xbdm_gdb_bridge_notification
        STATIC
        src/notification/xbdm_notification.cpp
        src/notification/xbdm_notification.h
        src/notification/xbdm_notification_transport.cpp
        src/notification/xbdm_notification_transport.h
)
target_include_directories(
        xbdm_gdb_bridge_notification
        PRIVATE
        src
)
target_link_libraries(
        xbdm_gdb_bridge_notification
        LINK_PUBLIC
        Boost::log
        xbdm_gdb_bridge_rdcp
)


add_library(
        xbdm_gdb_bridge_rdcp
        STATIC
        src/rdcp/rdcp_processed_request.cpp
        src/rdcp/rdcp_processed_request.h
        src/rdcp/rdcp_request.cpp
        src/rdcp/rdcp_request.h
        src/rdcp/rdcp_response.cpp
        src/rdcp/rdcp_response.h
        src/rdcp/rdcp_response_processors.cpp
        src/rdcp/rdcp_response_processors.h
        src/rdcp/types/execution_state.h
        src/rdcp/types/memory_region.cpp
        src/rdcp/types/memory_region.h
        src/rdcp/types/module.cpp
        src/rdcp/types/module.h
        src/rdcp/types/section.cpp
        src/rdcp/types/section.h
        src/rdcp/types/thread_context.h
        src/rdcp/xbdm_requests.cpp
        src/rdcp/xbdm_requests.h
        src/rdcp/xbdm_stop_reasons.h
        src/rdcp/xbdm_transport.cpp
        src/rdcp/xbdm_transport.h
)
target_include_directories(
        xbdm_gdb_bridge_rdcp
        PRIVATE
        src
)
target_link_libraries(
        xbdm_gdb_bridge_rdcp
        LINK_PUBLIC
        Boost::log
        xbdm_gdb_bridge_util
)


add_library(
        swizzle
        STATIC
        third_party/xemu/swizzle.c
        third_party/xemu/swizzle.h
)
target_include_directories(
        swizzle
        PUBLIC
        third_party/xemu
)


add_library(
        xbdm_gdb_bridge_tracer
        STATIC
        src/tracer/frame_capture.cpp
        src/tracer/frame_capture.h
        src/tracer/image_util.cpp
        src/tracer/image_util.h
        src/tracer/notification_ntrc.cpp
        src/tracer/notification_ntrc.h
        src/tracer/tracer.cpp
        src/tracer/tracer.h
        src/tracer/tracer_xbox_shared.h
        ${GENERATED_FILES_DIR}/ntrc_dyndxt_xbox.h
        ${ntrc_dyndxt_include_dir}/ntrc_dyndxt.h
)
target_include_directories(
        xbdm_gdb_bridge_tracer
        PUBLIC
        ${ntrc_dyndxt_include_dir}
        PRIVATE
        src
)
target_link_libraries(
        xbdm_gdb_bridge_tracer
        LINK_PUBLIC
        ${RC_DEPENDS}
        Boost::log
        lodepng
        swizzle
        xbdm_gdb_bridge_notification
)
add_dependencies(xbdm_gdb_bridge_tracer ntrc_dyndxt)


add_library(
        xbdm_gdb_bridge_util
        STATIC
        src/util/config_path.cpp
        src/util/config_path.h
        src/util/logging.cpp
        src/util/logging.h
        src/util/optional.h
        src/util/parsing.cpp
        src/util/parsing.h
        src/util/path.cpp
        src/util/path.h
        src/util/timer.cpp
        src/util/timer.h
)
target_include_directories(
        xbdm_gdb_bridge_util
        PRIVATE
        src
)
target_link_libraries(
        xbdm_gdb_bridge_util
        LINK_PUBLIC
        Boost::log
)


add_library(
        xbdm_gdb_bridge_xbox_xbdm_context
        STATIC
        src/xbox/xbdm_context.cpp
        src/xbox/xbdm_context.h
)
target_include_directories(
        xbdm_gdb_bridge_xbox_xbdm_context
        PRIVATE
        src
)
target_link_libraries(
        xbdm_gdb_bridge_xbox_xbdm_context
        LINK_PUBLIC
        Boost::log
        xbdm_gdb_bridge_notification
)


add_library(
        xbdm_gdb_bridge_xbox_debugger
        STATIC
        src/xbox/debugger/debugger_expression_parser.cpp
        src/xbox/debugger/debugger_expression_parser.h
        src/xbox/debugger/thread.cpp
        src/xbox/debugger/thread.h
        src/xbox/debugger/xbdm_debugger.cpp
        src/xbox/debugger/xbdm_debugger.h
        src/xbox/debugger/debugger_xbox_interface.cpp
        src/xbox/debugger/debugger_xbox_interface.h
)
target_include_directories(
        xbdm_gdb_bridge_xbox_debugger
        PRIVATE
        src
)
target_link_libraries(
        xbdm_gdb_bridge_xbox_debugger
        LINK_PUBLIC
        Boost::log
        xbdm_gdb_bridge_rdcp
        xbdm_gdb_bridge_xbox_xbdm_context
)


add_library(
        xbdm_gdb_bridge_shell
        STATIC
        src/shell/command.cpp
        src/shell/command.h
        src/shell/commands.cpp
        src/shell/commands.h
        src/shell/debugger_commands.cpp
        src/shell/debugger_commands.h
        src/shell/dyndxt_commands.cpp
        src/shell/dyndxt_commands.h
        src/shell/file_util.cpp
        src/shell/file_util.h
        src/shell/macro_commands.cpp
        src/shell/macro_commands.h
        src/shell/screenshot_converter.cpp
        src/shell/screenshot_converter.h
        src/shell/shell.cpp
        src/shell/shell.h
        src/shell/shell_commands.cpp
        src/shell/shell_commands.h
        src/shell/tracer_commands.cpp
        src/shell/tracer_commands.h
)
target_include_directories(
        xbdm_gdb_bridge_shell
        PRIVATE
        src
        PUBLIC
        ${replxx_include_dir}
        ${capstone_include_dir}
)
target_link_libraries(
        xbdm_gdb_bridge_shell
        LINK_PUBLIC
        Boost::log
        ${SDL2_LINK_LIBRARIES}
        ${SDL2IMAGE_LINK_LIBRARIES}
        xbdm_gdb_bridge_dyndxt_loader
        xbdm_gdb_bridge_net
        xbdm_gdb_bridge_rdcp
        xbdm_gdb_bridge_tracer
        xbdm_gdb_bridge_util
        xbdm_gdb_bridge_xbox_debugger
        xbdm_gdb_bridge_xbox_xbdm_context
        xbdm_gdb_bridge_xbox_interface
        LINK_PRIVATE
        libreplxx
        libcapstone
)

add_library(
        xbdm_gdb_bridge_shell_gdb
        STATIC
        src/shell/gdb/gdb_commands.cpp
        src/shell/gdb/gdb_commands.h
)
target_include_directories(
        xbdm_gdb_bridge_shell_gdb
        PRIVATE
        src
)
target_link_libraries(
        xbdm_gdb_bridge_shell_gdb
        LINK_PUBLIC
        Boost::log
        xbdm_gdb_bridge_shell
        xbdm_gdb_bridge_xbox_bridge
)

add_library(
        xbdm_gdb_bridge_xbox_interface
        STATIC
        src/xbox/xbox_interface.cpp
        src/xbox/xbox_interface.h
)
target_include_directories(
        xbdm_gdb_bridge_xbox_interface
        PRIVATE
        src
)
target_link_libraries(
        xbdm_gdb_bridge_xbox_interface
        LINK_PUBLIC
        Boost::log
        xbdm_gdb_bridge_gdb
)


add_library(
        xbdm_gdb_bridge_xbox_bridge
        STATIC
        src/xbox/bridge/gdb_bridge.cpp
        src/xbox/bridge/gdb_bridge.h
        src/xbox/bridge/gdb_registers.cpp
        src/xbox/bridge/gdb_registers.h
        src/xbox/bridge/gdb_xbox_interface.cpp
        src/xbox/bridge/gdb_xbox_interface.h
)
target_include_directories(
        xbdm_gdb_bridge_xbox_bridge
        PRIVATE
        src
)
target_link_libraries(
        xbdm_gdb_bridge_xbox_bridge
        LINK_PUBLIC
        Boost::log
        xbdm_gdb_bridge_gdb
        xbdm_gdb_bridge_xbox_interface
)


add_executable(
        xbdm_gdb_bridge
        src/main.cpp
        src/discovery/discoverer.cpp
        src/discovery/discoverer.h
        third_party/nxdk/winapi/winnt.h
        third_party/nxdk/xboxkrnl/xboxdef.h
)

target_include_directories(
        xbdm_gdb_bridge
        PRIVATE
        handler_loader_bootstrap
        src
        third_party/nxdk
        third_party/xemu
)

target_link_libraries(
        xbdm_gdb_bridge
        LINK_PUBLIC
        Boost::log
        Boost::program_options
        ${RC_DEPENDS}
        LINK_PRIVATE
        xbdm_gdb_bridge_xbox_bridge
        xbdm_gdb_bridge_net
        xbdm_gdb_bridge_notification
        xbdm_gdb_bridge_rdcp
        xbdm_gdb_bridge_shell
        xbdm_gdb_bridge_shell_gdb
        xbdm_gdb_bridge_tracer
        xbdm_gdb_bridge_util
        xbdm_gdb_bridge_xbox_debugger
        xbdm_gdb_bridge_xbox_xbdm_context
)

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
)
target_include_directories(
        rdcp_tests
        PRIVATE src
        PRIVATE test
)
target_link_libraries(
        rdcp_tests
        LINK_PRIVATE
        Boost::log
        Boost::unit_test_framework
        LINK_PRIVATE
        xbdm_gdb_bridge_rdcp
        test_util
)
add_test(NAME rdcp_tests COMMAND rdcp_tests)

# util_tests
add_executable(
        util_tests
        test/util/test_command_line_command_tokenizer.cpp
        test/util/test_main.cpp
        test/util/test_parsing.cpp
        test/util/test_path.cpp
)
target_include_directories(
        util_tests
        PRIVATE src
        PRIVATE test
)
target_link_libraries(
        util_tests
        LINK_PRIVATE
        Boost::log
        Boost::unit_test_framework
        LINK_PRIVATE
        test_util
        xbdm_gdb_bridge_util
)
add_test(NAME util_tests COMMAND util_tests)

# dxt_library_loader_tests
add_executable(
        dxt_library_loader_tests
        test/dxt_library_loader/test_dxt_library.cpp
        test/dxt_library_loader/test_main.cpp
        src/dyndxt_loader/dxt_library.cpp
        src/dyndxt_loader/dxt_library.h
)
target_include_directories(
        dxt_library_loader_tests
        PRIVATE src
        PRIVATE test
        PRIVATE third_party/nxdk
)
target_link_libraries(
        dxt_library_loader_tests
        LINK_PRIVATE
        Boost::log
        Boost::unit_test_framework
        LINK_PRIVATE
        test_util
        xbdm_gdb_bridge_util
)
add_dependencies(dxt_library_loader_tests dyndxt_loader)
add_test(NAME dxt_library_loader_tests COMMAND dxt_library_loader_tests)

# Mock XBDM server
add_library(
        mock_xbdm_server
        test/test_util/mock_xbdm_server/mock_server_debugger_interface_fixture.cpp
        test/test_util/mock_xbdm_server/mock_server_debugger_interface_fixture.h
        test/test_util/mock_xbdm_server/mock_xbdm_client_transport.cpp
        test/test_util/mock_xbdm_server/mock_xbdm_client_transport.h
        test/test_util/mock_xbdm_server/mock_xbdm_server.cpp
        test/test_util/mock_xbdm_server/mock_xbdm_server.h
        test/test_util/mock_xbdm_server/mock_xbox_state.cpp
        test/test_util/mock_xbdm_server/mock_xbox_state.h
)
target_include_directories(
        mock_xbdm_server
        PRIVATE src
        PRIVATE test
)
target_link_libraries(
        mock_xbdm_server
        LINK_PRIVATE
        Boost::log
        Boost::unit_test_framework
        LINK_PRIVATE
        test_util
)

# test_debugger_expression_parser_tests
add_executable(
        test_debugger_expression_parser_tests
        test/xbox/debugger/test_main.cpp
        test/xbox/debugger/test_debugger_expression_parser.cpp
)
target_include_directories(
        test_debugger_expression_parser_tests
        PRIVATE src
        PRIVATE test
)
target_link_libraries(
        test_debugger_expression_parser_tests
        LINK_PRIVATE
        Boost::log
        Boost::unit_test_framework
        xbdm_gdb_bridge_xbox_debugger
        test_util
)
add_test(NAME test_debugger_expression_parser_tests COMMAND test_debugger_expression_parser_tests)


# shell_tests
add_executable(
        shell_tests
        test/shell/test_main.cpp
        test/shell/test_commands.cpp
        test/shell/test_debugger_commands.cpp
        test/shell/test_disassemble.cpp
        test/shell/test_dyndxt_commands.cpp
        test/shell/test_command_break.cpp
)
target_include_directories(
        shell_tests
        PRIVATE src
        PRIVATE test
)
target_link_libraries(
        shell_tests
        LINK_PRIVATE
        Boost::log
        Boost::unit_test_framework
        mock_xbdm_server
        xbdm_gdb_bridge_shell
        test_util
)
add_test(NAME shell_tests COMMAND shell_tests)


# xbox_debugger_tests
add_executable(
        xbox_debugger_tests
        test/xbox/debugger/test_main.cpp
        test/xbox/debugger/test_xbdm_debugger.cpp
        test/xbox/debugger/test_xbdm_debugger_transparency.cpp
        test/xbox/debugger/test_thread.cpp
)
target_include_directories(
        xbox_debugger_tests
        PRIVATE src
        PRIVATE test
)
target_link_libraries(
        xbox_debugger_tests
        LINK_PRIVATE
        Boost::log
        Boost::unit_test_framework
        mock_xbdm_server
        xbdm_gdb_bridge_net
        xbdm_gdb_bridge_rdcp
        xbdm_gdb_bridge_xbox_debugger
        xbdm_gdb_bridge_xbox_xbdm_context
        test_util
)
add_test(NAME xbox_debugger_tests COMMAND xbox_debugger_tests)

# mock_xbdm_server_tests
add_executable(
        mock_xbdm_server_tests
        test/test_util/mock_xbdm_server/test_fetch_memory_region.cpp
)
target_include_directories(
        mock_xbdm_server_tests
        PRIVATE src
        PRIVATE test
)
target_link_libraries(
        mock_xbdm_server_tests
        LINK_PRIVATE
        Boost::log
        Boost::unit_test_framework
        mock_xbdm_server
        test_util
)
add_test(NAME mock_xbdm_server_tests COMMAND mock_xbdm_server_tests)
