#!/bin/bash

HERE=$(cd $(dirname $0); pwd -P)
BUILD_DIR=${HERE}/catboost/python-package/catboost
WHEEL_DIR=${BUILD_DIR}/..

export YA_CACHE_DIR=${HOME}/catboost-cache
mkdir -p ${YA_CACHE_DIR}

cd ${BUILD_DIR}
unset CC CXX
${HERE}/ya make -r -k -DHAVE_CUDA=no \
  --target-platform-c-compiler=gcc --target-platform-cxx-compiler=g++ \
  --c-compiler=gcc --cxx-compiler=g++ #-v

cd ${WHEEL_DIR}
export PYTHONPATH=$PYTHONPATH:${WHEEL_DIR}
python mk_wheel.py -r
