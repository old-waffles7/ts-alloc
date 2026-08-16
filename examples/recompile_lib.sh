#!/bin/bash

cmake -DCMAKE_BUILD_TYPE=Debug ../build && cmake --build ../build && cp ../build/libtsalloc.so .

