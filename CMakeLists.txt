cmake_minimum_required(VERSION 3.18)

project(xbdm_gdb_bridge)

set(CMAKE_VERBOSE_MAKEFILE TRUE)

enable_testing()
set(CMAKE_CXX_STANDARD 17)
include(CMakePushCheckState)
set(_CMAKE_PROCESSING_LANGUAGE "C")
include(CheckSymbolExists)
include (ExternalProject)
include(FindPkgConfig)

ExternalProject_Add(
        dyndxt_loader
        PREFIX ddxt
        GIT_REPOSITORY https://github.com/abaire/nxdk_dyndxt.git
        GIT_TAG origin/main
        CMAKE_ARGS
            -DCMAKE_TOOLCHAIN_FILE=$ENV{NXDK_DIR}/share/toolchain-nxdk.cmake
            -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        BUILD_BYPRODUCTS
        ddxt/lib/bootstrap_l1.asm.obj
        ddxt/lib/bootstrap_l2.asm.obj
        ddxt/lib/dynamic_dxt_loader.lib
        ddxt/lib/libdynamic_dxt_loader.dll
        ddxt/lib/xbdm.lib
)
ExternalProject_Get_Property(dyndxt_loader INSTALL_DIR)
set(dyndxt_lib_dir ${INSTALL_DIR}/lib)
set(dyndxt_include_dir ${INSTALL_DIR}/include)

# Import the nvtrace DynamicDXT
ExternalProject_Add(
        ntrc_dyndxt
        PREFIX ntrc
        GIT_REPOSITORY https://github.com/abaire/nxdk_ntrc_dyndxt.git
        GIT_TAG origin/main
        CMAKE_ARGS
            -DCMAKE_TOOLCHAIN_FILE=$ENV{NXDK_DIR}/share/toolchain-nxdk.cmake
            -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        BUILD_BYPRODUCTS
        ntrc/lib/libntrc_dyndxt.dll
        ntrc/include/ntrc_dyndxt.h
)
ExternalProject_Get_Property(ntrc_dyndxt INSTALL_DIR)
set(ntrc_dyndxt_lib_dir ${INSTALL_DIR}/lib)
set(ntrc_dyndxt_include_dir ${INSTALL_DIR}/include)

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
configure_file("test/configure_test.h.cmake" "${GENERATED_FILES_DIR}/configure_test.h")

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

add_executable(
        xbdm_gdb_bridge
        src/main.cpp
        src/discovery/discoverer.cpp
        src/discovery/discoverer.h
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
        src/tracer/frame_capture.cpp
        src/tracer/frame_capture.h
        src/tracer/image_util.cpp
        src/tracer/image_util.h
        src/tracer/notification_ntrc.cpp
        src/tracer/notification_ntrc.h
        src/tracer/tracer.cpp
        src/tracer/tracer.h
        src/tracer/tracer_xbox_shared.h
        src/xbox/debugger/thread.cpp
        src/xbox/debugger/thread.h
        src/xbox/debugger/xbdm_debugger.cpp
        src/xbox/debugger/xbdm_debugger.h
        third_party/nxdk/winapi/winnt.h
        third_party/nxdk/xboxkrnl/xboxdef.h
        third_party/xemu/swizzle.c
        third_party/xemu/swizzle.h
        ${GENERATED_FILES_DIR}/bootstrap_l1_xbox.h
        ${GENERATED_FILES_DIR}/bootstrap_l2_xbox.h
        ${GENERATED_FILES_DIR}/dynamic_dxt_loader_xbox.h
        ${GENERATED_FILES_DIR}/ntrc_dyndxt_xbox.h
        ${ntrc_dyndxt_include_dir}/ntrc_dyndxt.h
)

target_include_directories(
        xbdm_gdb_bridge
        PRIVATE
        handler_loader_bootstrap
        src
        third_party/nxdk
        third_party/xemu
        ${ntrc_dyndxt_include_dir}
)

target_link_libraries(
        xbdm_gdb_bridge
        LINK_PUBLIC
        ${Boost_LIBRARIES}
        ${SDL2_LINK_LIBRARIES}
        ${SDL2IMAGE_LINK_LIBRARIES}
        ${RC_DEPENDS}
        LINK_PRIVATE
        lodepng
)

add_dependencies(xbdm_gdb_bridge dyndxt_loader ntrc_dyndxt)

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
        ${Boost_LIBRARIES}
        LINK_PRIVATE
        test_util
)
add_dependencies(dxt_library_loader_tests dyndxt_loader)
add_test(NAME dxt_library_loader_tests COMMAND dxt_library_loader_tests)
