#!/bin/bash

cmake -DVERBOSE_TRACE=ON -DCMAKE_BUILD_TYPE=Debug ../build && cmake --build ../build && cp ../build/libtsalloc.so .

