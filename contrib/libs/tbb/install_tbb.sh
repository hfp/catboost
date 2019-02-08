#!/bin/bash

BASENAME=$(command -v basename)
DIRNAME=$(command -v dirname)
SORT=$(command -v sort)
TAIL=$(command -v tail)
SED=$(command -v sed)
LS=$(command -v ls)
RM=$(command -v rm)
CP=$(command -v cp)

TBBPREFIX="/opt/intel /usr/local ${HOME}"
HERE=$(cd $(${DIRNAME} $0); pwd -P)

if [ "" != "${SORT}" ] && [ "" != "${TAIL}" ] && [ "" != "${SED}" ] && \
   [ "" != "${LS}" ] && [ "" != "${RM}" ] && [ "" != "${CP}" ] && \
   [ "" != "${BASENAME}" ] && [ "" != "${DIRNAME}" ];
then
  for DIR in ${TBBPREFIX}; do
    if [ "" != "${TBBROOT}" ]; then break; fi
    TBBHEAD=$(${LS} -1 ${DIR}/compilers_and_libraries_*/linux/tbb/include/tbb/tbb.h 2>/dev/null | ${SORT} -V | ${TAIL} -n1)
    TBBROOT=$(${DIRNAME} $(${DIRNAME} $(${DIRNAME} ${TBBHEAD} 2>/dev/null) 2>/dev/null) 2>/dev/null)
  done
  for DIR in ${TBBPREFIX}; do
    if [ "" != "${TBBROOT}" ]; then break; fi
    TBBHEAD=$(${LS} -1 ${DIR}/tbb*/include/tbb/tbb.h 2>/dev/null | ${SORT} -V | ${TAIL} -n1)
    TBBROOT=$(${DIRNAME} $(${DIRNAME} $(${DIRNAME} ${TBBHEAD} 2>/dev/null) 2>/dev/null) 2>/dev/null)
  done
  echo "Intel TBB found at ${TBBROOT}."
  if [ "${HERE}" != "${TBBROOT}" ]; then
    read -p "Installing to ${HERE}? [Y/N]" -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
      if [ -d ${HERE}/include ]; then
        echo -n " delete old directory include, and"
        ${RM} -r ${HERE}/include
      fi
      echo " deep-copy ${TBBROOT}/include"
      ${CP} -Lr ${TBBROOT}/include ${HERE}
      # truncate tbb_annotate.h to avoid external reference to advisor-annotate.h
      if [ -e ${HERE}/include/serial/tbb/tbb_annotate.h ]; then
        ${CP} /dev/null ${HERE}/include/serial/tbb/tbb_annotate.h
      fi
      if [ -d ${HERE}/lib ]; then
        echo -n " delete old directory lib, and"
        ${RM} -r ${HERE}/lib
      fi
      echo " deep-copy ${TBBROOT}/lib"
      ${CP} -Lr ${TBBROOT}/lib ${HERE}
      TBBRTNEW=$(${LS} -1 ${HERE}/lib/intel64 2>/dev/null | ${SORT} -V | ${TAIL} -n1)
      TBBRTCUR=$(${BASENAME} $(${SED} -n "s/ *-L\(..*\)/\1/p" ${HERE}/ya.make.template))
      if [ "${TBBRTNEW}" = "${TBBRTCUR}" ]; then
        ${CP} ${HERE}/ya.make.template ${HERE}/ya.make
      else
        ${SED} -e "s/\(.*\)-L\(..*\)${TBBRTCUR}/\1-L\2${TBBRTNEW}/" ${HERE}/ya.make.template > ${HERE}/ya.make
      fi
      echo "Successfully completed."
    else
      echo "No action performed."
      exit 0
    fi
  fi
else
  echo "Error: missing prerequisites!"
  exit 1
fi

