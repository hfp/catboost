#!/bin/bash

DIRNAME=$(command -v dirname)
SORT=$(command -v sort)
TAIL=$(command -v tail)
ENV=$(command -v env)
RM=$(command -v rm)
CP=$(command -v cp)

TBBPREFIX="/opt/intel /usr/local ${HOME}"
HERE=$(cd $(${DIRNAME} $0); pwd -P)

if [ "" != "${DIRNAME}" ] && [ "" != "${SORT}" ] && [ "" != "${TAIL}" ] && [ "" != "${RM}" ] && [ "" != "${CP}" ]; then
  for DIR in ${TBBPREFIX}; do
    if [ "" != "${TBBROOT}" ]; then break; fi
    TBBHEAD=$(ls -1 ${DIR}/compilers_and_libraries_*/linux/tbb/include/tbb/tbb.h 2>/dev/null | ${SORT} -V | ${TAIL} -n1)
    TBBROOT=$(${DIRNAME} $(${DIRNAME} $(${DIRNAME} ${TBBHEAD} 2>/dev/null) 2>/dev/null) 2>/dev/null)
  done
  for DIR in ${TBBPREFIX}; do
    if [ "" != "${TBBROOT}" ]; then break; fi
    TBBHEAD=$(ls -1 ${DIR}/tbb*/include/tbb/tbb.h 2>/dev/null | ${SORT} -V | ${TAIL} -n1)
    TBBROOT=$(${DIRNAME} $(${DIRNAME} $(${DIRNAME} ${TBBHEAD} 2>/dev/null) 2>/dev/null) 2>/dev/null)
  done
  echo "Intel TBB found at ${TBBROOT}."
  if [ "${HERE}" != "${TBBROOT}" ]; then
    read -p "Install it to ${HERE}? [Y/N]" -n 1 -r
    if [[ $REPLY =~ ^[Yy]$ ]]; then
      if [ -d ${HERE}/include ]; then
        echo "Deleting old ${HERE}/include"
        ${RM} -r ${HERE}/include
      fi
      if [ -d ${HERE}/lib ]; then
        echo "Deleting old ${HERE}/lib"
        ${RM} -r ${HERE}/lib
      fi
      echo "Copying ${TBBROOT}/include"
      ${CP} -Hr ${TBBROOT}/include ${HERE}
      echo "Copying ${TBBROOT}/lib"
      ${CP} -Hr ${TBBROOT}/lib ${HERE}
    else
      exit 1
    fi
  fi
else
  echo "Error: missing prerequisites!"
  exit 1
fi
