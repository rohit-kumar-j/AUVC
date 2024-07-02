#!/bin/sh
set -xe # build output

curr_dir=$(pwd)
echo "$curr_dir"
mkdir -p build

clang++ -O0 -g -Wall -Wextra -std=c++11 -I/usr/local/include/opencv4/ -L/usr/local/lib64/ -lopencv_core -lopencv_imgcodecs -lopencv_highgui -lopencv_imgproc  tests/test_opencv.cxx -o ./build/opencv
