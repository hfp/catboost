#!/bin/bash

EXPERIMENT=$1
shift

python run.py --experiment ${EXPERIMENT} \
  --dataset-dir ${HOME}/catboost-models \
  --iterations 400 --learners cat $*
