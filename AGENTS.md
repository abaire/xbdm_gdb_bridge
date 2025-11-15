# Building

## Prerequisites

Building this project requires a working nxdk environment.

1. Check out the nxdk from https://github.com/XboxDev/nxdk.git make sure to fetch submodules recursively.
2. Fetch the prerequisites for the nxdk, see the README file in the nxdk directory.
2. In the nxdk directory, source the `bin/activate` script and run `make NXDK_ONLY=y -j`.

## Building the app

1. Once the nxdk is successfully built, set the `NXDK_DIR` environment variable to point at the root of the nxdk directory.
1. Next, evaluate the `.github/workflows/build.yml` file and install the dependencies needed for your platform.
2. At that point you may run `cmake` build commands as usual.
