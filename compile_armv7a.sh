#!/bin/bash

# Crosscompilation for ARM
# TN, 2015

# Jump to the script's dir
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd ${DIR}

# Source toolchain for cross compilation
. /usr/local/beeeon-i686/environment-setup-armv7a-vfp-neon-oe-linux-gnueabi

# -D LEDS_ENABLED
export BUILD_DIR=build_armv7a
export INSTALL_DIR=install_armv7a
export CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=$OECORE_NATIVE_SYSROOT/usr/share/cmake/OEToolchainConfig.cmake"

/bin/bash compile_common.sh.inc $@
