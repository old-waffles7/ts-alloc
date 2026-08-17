#!/bin/bash

cmake -S .. -B ../build
cmake -DVERBOSE_TRACE=ON -DCMAKE_BUILD_TYPE=Debug ../build && cmake --build ../build && cp ../build/libtsalloc.so .

