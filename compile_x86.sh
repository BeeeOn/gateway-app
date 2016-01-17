#!/bin/bash

# Native compilation
# TN, 2015

# Jump to the script's dir
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd ${DIR}

export BUILD_DIR=build_x86
export INSTALL_DIR=install_x86
export PC_PLATFORM=true

/bin/bash ./compile_common.sh.inc $@
cd ${BUILD_DIR}
make install
