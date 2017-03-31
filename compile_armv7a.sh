#!/bin/bash

# Crosscompilation for ARM
# TN, 2015

# Jump to the script's dir
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd ${DIR}

# Source toolchain for cross compilation
if [ -f "/usr/local/beeeon-i686/environment-setup-armv7a-vfp-neon-oe-linux-gnueabi" ]; then
	. /usr/local/beeeon-i686/environment-setup-armv7a-vfp-neon-oe-linux-gnueabi
elif [ -f "/usr/beeeon-i686-armv7a-vfp-neon-toolchain-0.9.3+99rc1/environment-setup-armv7a-vfp-neon-oe-linux-gnueabi" ]; then
	. /usr/beeeon-i686-armv7a-vfp-neon-toolchain-0.9.3+99rc1/environment-setup-armv7a-vfp-neon-oe-linux-gnueabi
elif [ -f "/usr/local/beeeon-i686-armv7a-vfp-neon-toolchain-0.9.3+99rc1/environment-setup-armv7a-vfp-neon-oe-linux-gnueabi" ]; then
	. /usr/local/beeeon-i686-armv7a-vfp-neon-toolchain-0.9.3+99rc1/environment-setup-armv7a-vfp-neon-oe-linux-gnueabi
fi

# -D LEDS_ENABLED
export BUILD_DIR=build_armv7a
export INSTALL_DIR=install_armv7a
export CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=$OECORE_NATIVE_SYSROOT/usr/share/cmake/OEToolchainConfig.cmake"

/bin/bash compile_common.sh.inc $@
