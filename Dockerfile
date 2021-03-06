FROM ubuntu:latest

ADD . /wheel

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    build-essential git gcc-8 g++-8 \
    python python-dev python-pip python-setuptools \
    libtbb2 libtbb-dev

RUN git clone --single-branch --branch develop https://github.com/hfp/catboost

RUN cd catboost && echo "y" | ./contrib/libs/tbb/install.sh
RUN cd catboost && ./make.sh
RUN mv /catboost/catboost/python-package/catboost-*.whl /wheel
