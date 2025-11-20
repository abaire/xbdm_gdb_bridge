# Building

To build this project successfully you must follow the instructions in this section.

1. Run the following script to clone the nxdk, install dependencies, build the nxdk, and build the project.

 ```bash
 # Clone nxdk and checkout the correct commit
 cd <someplace_outside_of_this_repository>
 git clone --recursive https://github.com/XboxDev/nxdk.git && \
 cd nxdk && \
 git checkout 3b24b559124aba8fcb94fddff1d2dc9ef32de461 && \
 git submodule update --init --recursive && \
 cd .. && \
 \
 # Install dependencies
 sudo apt-get update && sudo apt-get install -y \
 bison \
 cmake \
 flex \
 libboost-all-dev \
 libsdl2-dev \
 libsdl2-image-dev \
 llvm \
 lld \
 nasm && \
 \
 # Build nxdk
 cd nxdk && \
 eval $(./bin/activate -s) && \
 make -j NXDK_ONLY=y && \
 make tools -j && \
 cd .. && \
 \
 # Build the project
 export NXDK_DIR="$(pwd)/nxdk" && \
 cmake -B build -DCMAKE_BUILD_TYPE=Release -DNXDK_DIR="$(pwd)/nxdk" -DTEST_TIMEOUT_SECONDS=10 && \
 cmake --build build -- -j$(nproc) && \
 \
 # Run tests
 cd build && ctest --output-on-failure
 ```

# Editing

* You must format any code changes with `clang-format`.
* You must make sure that all automated tests pass before proposing changes.
* You must NOT commit the nxdk directory or add it as a submodule unless explicitly asked to do so by the user.
