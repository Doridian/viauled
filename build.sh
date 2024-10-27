#!/bin/bash

set -euo pipefail
set -x

rm -rf build && mkdir build
cd build
cmake ..
make -j$(nproc)
