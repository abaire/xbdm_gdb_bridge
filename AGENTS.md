# Building

To build this project successfully you must follow the instructions in this section.

1. Check out the nxdk from https://github.com/XboxDev/nxdk.git, fetching submodules recursively.
1. Fetch the prerequisites for the nxdk, see the README file in the nxdk directory.
1. In the nxdk directory, source the `bin/activate` script and run `make NXDK_ONLY=y -j`.
1. Once the nxdk is successfully built, set the `NXDK_DIR` environment variable to point at the root of the nxdk
   directory.
1. Next, install the following packages:
    * cmake
    * libboost-all-dev
    * libsdl2-dev
    * libsdl2-image-dev
    * llvm
    * lld
    * nasm

   You must check the `.github/workflows/build.yml` to make sure that nothing is missing from this list.
