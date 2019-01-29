#!/bin/bash

HERE=$(cd $(dirname $0); pwd -P)

# additional requirement(s):
#  USE_STL_SYSTEM=yes

export YA_CACHE_DIR=${HOME}/catboost-cache
export CC=${HERE}/ycc.sh
export CXX=${CC}

cd catboost/python-package/catboost
${HERE}/ya make -r -k -DHAVE_CUDA=no #-v
cd ..
export PYTHONPATH=$PYTHONPATH:$(pwd)
python mk_wheel.py -r
