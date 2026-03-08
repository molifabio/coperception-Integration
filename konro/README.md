# konro
A modern resource manager

## Status

[![Building](https://github.com/HEAPLab/konro/actions/workflows/main.yml/badge.svg)](https://github.com/HEAPLab/konro/actions)

# Compilation instructions
* `mkdir build && cd build`
* `cmake ..`
* `make`

# Dependencies
- cmake
- pkg-config
- liblog4cpp5-dev
- libhwloc-dev
- libsensors-dev
- libopencv-dev

OpenCV is required only for the peopledetect demo.
If it is not installed, peopledetect is not compiled.

# Development

QtCreator has been used as IDE for development on Ubuntu 22.04.2 LTS.
It has also been tested on Ubuntu 18.04 LTS with an updated kernel (5.4.0-135-generic.)
