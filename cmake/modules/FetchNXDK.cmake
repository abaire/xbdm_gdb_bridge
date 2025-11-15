include(ExternalProject)

set(NXDK_SOURCE_DIR "${CMAKE_BINARY_DIR}/nxdk-prefix/src/nxdk")

set(NXDK_DIR "${NXDK_SOURCE_DIR}" CACHE INTERNAL "Path to NXDK installation")

find_program(MAKE_COMMAND NAMES gmake make REQUIRED)

set(NXDK_DEBUG_FLAG "n" CACHE STRING "Enable debug build for the NXDK submodule. 'y' or 'n'.")
set(NXDK_LTO_FLAG "n" CACHE STRING "Enable link time optimization for the NXDK submodule. 'y' or 'n'.")

set(NXDK_BUILD_COMMAND
        bash -c
        "export NXDK_DIR='${NXDK_SOURCE_DIR}' && \
     cd '${NXDK_DIR}/bin' && \
     . activate -s && \
     cd '${NXDK_DIR}' && \
     DEBUG=${NXDK_DEBUG_FLAG} LTO=${NXDK_LTO_FLAG} NXDK_ONLY=y ${MAKE_COMMAND} -j && \
     ${MAKE_COMMAND} tools -j"
)

ExternalProject_Add(
        nxdk
        GIT_REPOSITORY https://github.com/XboxDev/nxdk.git
        GIT_TAG d0ae96acc926a6c3f6254b289a1a9a3c1db182f1
        SOURCE_DIR "${NXDK_SOURCE_DIR}"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND "${NXDK_BUILD_COMMAND}"
        BUILD_IN_SOURCE 1
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS
        <SOURCE_DIR>/lib/libnxdk.lib
        <SOURCE_DIR>/share/toolchain-nxdk.cmake
        <SOURCE_DIR>/tools/extract-xiso/build/extract-xiso
)

set(ENV{NXDK_DIR} "${NXDK_DIR}")
