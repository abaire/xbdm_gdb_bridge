cmake_minimum_required(VERSION 3.18)
project(handler_loader_bootstrap)
export(PACKAGE handler_loader_bootstrap)

set(CMAKE_CXX_STANDARD 17)

include(CMakeFindBinUtils)
include(Compiler/${CMAKE_C_COMPILER_ID}-FindBinUtils OPTIONAL)
unset(_CMAKE_PROCESSING_LANGUAGE)

set(CMAKE_VERBOSE_MAKEFILE TRUE)
set(CMAKE_SYSTEM_NAME WindowsStore)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(
        GENERATE_RAW_BINARY
        "${CMAKE_SOURCE_DIR}/handler_loader_bootstrap/generate_raw_binary.sh ${CMAKE_BINARY_DIR} <OBJECTS> --"
)

# Produce a raw binary.
# Note that this must be done before `enable_language`.
set(CMAKE_ASM_NASM_OBJECT_FORMAT bin)
enable_language(ASM_NASM)

enable_language(C)

# L1 bootstrap ---------------------------------------
set(
        L1BOOTSTRAP_SOURCES
        bootstrap_l1.asm
)

set_source_files_properties(${L1BOOTSTRAP_SOURCES} PROPERTIES LANGUAGE ASM_NASM)
add_library(bootstrap_l1 STATIC EXCLUDE_FROM_ALL ${L1BOOTSTRAP_SOURCES})
set_target_properties(
        bootstrap_l1
        PROPERTIES
        RULE_LAUNCH_LINK "${GENERATE_RAW_BINARY}"
        COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}"
)

# L2 bootstrap loader --------------------------------
set(
        L2BOOTSTRAP_THUNK_SOURCES
        bootstrap_l2_thunk.asm
)

set_source_files_properties(${L2BOOTSTRAP_THUNK_SOURCES} PROPERTIES LANGUAGE ASM_NASM)
add_library(bootstrap_l2_thunk STATIC EXCLUDE_FROM_ALL ${L2BOOTSTRAP_THUNK_SOURCES})
set_target_properties(
        bootstrap_l2_thunk
        PROPERTIES
        RULE_LAUNCH_LINK "${GENERATE_RAW_BINARY}"
        COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}"
)

# L2 bootstrap ---------------------------------------

set(
        L2BOOTSTRAP_SOURCES
        bootstrap_l2.asm
)

set_source_files_properties(${L2BOOTSTRAP_SOURCES} PROPERTIES LANGUAGE ASM_NASM)
add_library(bootstrap_l2 STATIC EXCLUDE_FROM_ALL ${L2BOOTSTRAP_SOURCES})
set_target_properties(
        bootstrap_l2
        PROPERTIES
        RULE_LAUNCH_LINK "${GENERATE_RAW_BINARY}"
        COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}"
)
