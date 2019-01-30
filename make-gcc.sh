#!/bin/bash

HERE=$(cd $(dirname $0); pwd -P)

export YA_CACHE_DIR=${HOME}/catboost-cache
export AR=gcc-ar
export CC=gcc
unset CXX

cd catboost/python-package/catboost
${HERE}/ya make -r -k -DHAVE_CUDA=no #-v
cd ..
export PYTHONPATH=$PYTHONPATH:$(pwd)
python mk_wheel.py -r
