#!/bin/bash

HERE=$(cd $(dirname $0); pwd -P)

# additional requirement(s):
#  USE_STL_SYSTEM=yes
#  INTEL_COMPILER=yes

export YA_CACHE_DIR=${HOME}/catboost-cache
export AR=xiar
export CC=icc
unset CXX

cd catboost/python-package/catboost
${HERE}/ya make -r -k -DHAVE_CUDA=no #-v
cd ..
export PYTHONPATH=$PYTHONPATH:$(pwd)
python mk_wheel.py -r
