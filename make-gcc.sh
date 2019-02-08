#!/bin/bash

HERE=$(cd $(dirname $0); pwd -P)
BUILD_DIR=${HERE}/catboost/python-package/catboost
WHEEL_DIR=${BUILD_DIR}/..

YCXX=g++
YCC=gcc
YARGS="-r -DHAVE_CUDA=no --c-compiler=${YCC} --cxx-compiler=${YCXX} $@"

unset CXX
export CC=${YCC}
export PYTHONPATH=$PYTHONPATH:${WHEEL_DIR}
export YA_CACHE_DIR=/tmp/catboost-cache
mkdir -p ${YA_CACHE_DIR}

cd ${BUILD_DIR}
${HERE}/ya make ${YARGS}

cd ${WHEEL_DIR}
python mk_wheel.py ${YARGS}
