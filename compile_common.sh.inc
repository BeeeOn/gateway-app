# Helper script for build
# TN, 2015

# check (the newest) fw_version from git tag and export it to CMake - recommended format of tag is "vX.Y"
FW=$(git describe --dirty)
if [ "$FW" != "" ]; then
  export FW_VERSION=$FW
fi

# checking if "I want to use relative paths to configs!" mode is active (type whatever you want to params of this script)
if [ $# -gt 0 ]; then
  export RELATIVE_PATHS=true
fi


mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}
cmake ${CMAKE_ARGS} -DCMAKE_INSTALL_PREFIX=../${INSTALL_DIR} ..

make -j`nproc`
