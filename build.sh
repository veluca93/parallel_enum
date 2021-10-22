#!/bin/bash -e

git submodule update --init --recursive --remote --merge
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..
make -j$(nproc)
