# Building

## Prerequisites

Building this project requires a working nxdk environment.

1. Check out the nxdk from https://github.com/XboxDev/nxdk.git
2. In the nxdk directory, run `make NXDK_ONLY=y`. You may need to source the `bin/activate` script first.

## Building

Once the nxdk is successfully built, set the `NXDK_DIR` environment variable to point at the root of the nxdk directory.
At that point you may run `cmake` build commands as usual.

Look at the .github/workflows/build.yml workflow for an example.
