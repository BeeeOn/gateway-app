#!/bin/bash

# Crosscompilation for PPC
# MP 2016, ripped off TN

# Jump to the script's dir
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd ${DIR}

# Source toolchain for cross compilation
. /usr/local/beeeon-i686-mpc8540/environment-setup

# -D LEDS_ENABLED
export BUILD_DIR=build_mpc8540
export INSTALL_DIR=install_mpc8540
export STAGING_DIR=$INSTALL_DIR
export CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=$PPC_BASEDIR/ToolchainConfig.cmake -DPOCO_NO_FLOAT=ON -DADAAPP_NO_SYSTEMD=ON"

/bin/bash compile_common.sh.inc $@
