#!/bin/bash

set -e

# 若不存在build目录，则创建
if [ ! -d `pwd`/build ]; then
    mkdir `pwd`/build
fi

rm -rf `pwd`/build/*

cd `pwd`/build &&
cmake .. &&
make
