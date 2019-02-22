#!/bin/bash

HERE=$(cd $(dirname $0); pwd -P)
BUILD_DIR=${HERE}/catboost/python-package/catboost
WHEEL_DIR=${BUILD_DIR}/..

VERSION_LLVC=$(($(echo "10000*__clang_major__+ 100*__clang_minor__" | clang   -E -P - 2>/dev/null)))
VERSION_LLVX=$(($(echo "10000*__clang_major__+ 100*__clang_minor__" | clang++ -E -P - 2>/dev/null)))
VERSION_GCC0=$(($(echo "10000*__GNUC__+ 100*__GNUC_MINOR__" | gcc   -E -P - 2>/dev/null)))
VERSION_GXX0=$(($(echo "10000*__GNUC__+ 100*__GNUC_MINOR__" | g++   -E -P - 2>/dev/null)))
VERSION_GCC8=$(($(echo "10000*__GNUC__+ 100*__GNUC_MINOR__" | gcc-8 -E -P - 2>/dev/null)))
VERSION_GXX8=$(($(echo "10000*__GNUC__+ 100*__GNUC_MINOR__" | g++-8 -E -P - 2>/dev/null)))

if [ "0" != "$((80000 < VERSION_GCC8))" ]; then
  YCC=gcc-8
elif [ "0" != "$((80000 < VERSION_GCC0))" ]; then
  YCC=gcc
elif [ "0" != "$((70000 < VERSION_LLVC))" ]; then
  YCC=clang
fi
if [ "0" != "$((80000 < VERSION_GXX8))" ]; then
  YCXX=g++-8
elif [ "0" != "$((80000 < VERSION_GXX0))" ]; then
  YCXX=g++
elif [ "0" != "$((70000 < VERSION_LLVX))" ]; then
  YCC=clang++
fi

YARGS="-DHAVE_CUDA=no --c-compiler=${YCC} --cxx-compiler=${YCXX} -r $@"

unset CXX
export CC=${YCC}
export PYTHONPATH=$PYTHONPATH:${WHEEL_DIR}
export YA_CACHE_DIR=/tmp/catboost-cache
mkdir -p ${YA_CACHE_DIR}
chmod +x ${HERE}/ya

cd ${BUILD_DIR}
${HERE}/ya make ${YARGS}

cd ${WHEEL_DIR}
python mk_wheel.py ${YARGS}
