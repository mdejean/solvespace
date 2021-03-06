#!/bin/sh -xe

if echo $TRAVIS_TAG | grep ^v; then BUILD_TYPE=RelWithDebInfo; else BUILD_TYPE=Debug; fi

mkdir build
cd build
# We build without the GUI until Travis updates to an Ubuntu version with GTK 3.22.
cmake .. -DCMAKE_C_COMPILER=clang-3.9 -DCMAKE_CXX_COMPILER=clang++-3.9 \
  -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
  -DENABLE_GUI=OFF \
  -DENABLE_SANITIZERS=ON
make VERBOSE=1
