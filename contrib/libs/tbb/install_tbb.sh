#!/bin/bash

BASENAME=$(command -v basename)
DIRNAME=$(command -v dirname)
MKDIR=$(command -v mkdir)
SORT=$(command -v sort)
TAIL=$(command -v tail)
SED=$(command -v sed)
LS=$(command -v ls)
RM=$(command -v rm)
CP=$(command -v cp)

TBBPLTFRM=intel64
TBBPREFIX="/opt/intel /usr/local /usr ${HOME}"
TBBLIBDIR="lib/${TBBPLTFRM} lib/x86_64-linux-gnu lib64"
HERE=$(cd $(${DIRNAME} $0); pwd -P)

if [ "" != "${SORT}" ] && [ "" != "${TAIL}" ] && [ "" != "${SED}" ] && \
   [ "" != "${LS}" ] && [ "" != "${RM}" ] && [ "" != "${CP}" ] && \
   [ "" != "${BASENAME}" ] && [ "" != "${DIRNAME}" ] && \
   [ "" != "${MKDIR}" ];
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
  for DIR in ${TBBPREFIX}; do
    if [ "" != "${TBBROOT}" ]; then break; fi
    TBBHEAD=$(${LS} -1 ${DIR}/include/tbb/tbb.h 2>/dev/null | ${SORT} -V | ${TAIL} -n1)
    TBBROOT=$(${DIRNAME} $(${DIRNAME} $(${DIRNAME} ${TBBHEAD} 2>/dev/null) 2>/dev/null) 2>/dev/null)
  done
  if [ "" != "${TBBROOT}" ] && [ "${HERE}" != "${TBBROOT}" ]; then
    echo "Intel TBB found at ${TBBROOT}."
    read -p "Installing to ${HERE}? [Y/N]" -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
      if [ -d ${HERE}/include ]; then
        echo -n " delete old directory include, and"
        ${RM} -r ${HERE}/include
      fi
      echo " deep-copy from ${TBBROOT}/include"
      ${MKDIR} -p ${HERE}/include && ${CP} -Lr ${TBBROOT}/include/tbb ${HERE}/include
      # truncate tbb_annotate.h to avoid external reference to advisor-annotate.h
      if [ -e ${TBBROOT}/include/serial/tbb/tbb_annotate.h ]; then
        ${CP} -Lr ${TBBROOT}/include/serial ${HERE}/include
        ${CP} /dev/null ${HERE}/include/serial/tbb/tbb_annotate.h
      else
        ${MKDIR} -p ${HERE}/include/serial/tbb
        ${CP} /dev/null ${HERE}/include/serial/tbb/parallel_for.h
      fi
      if [ -e ${TBBROOT}/include/tbb/machine/xbox360_ppc.h ]; then
        ${CP} /dev/null ${HERE}/include/tbb/machine/xbox360_ppc.h
      fi
      if [ -e ${TBBROOT}/include/tbb/machine/windows_api.h ]; then
        ${SED} -e "s/#include *<xtl\.h>//" \
          ${TBBROOT}/include/tbb/machine/windows_api.h > \
          ${HERE}/include/tbb/machine/windows_api.h
      fi
      if [ -e ${TBBROOT}/include/tbb/concurrent_vector.h ]; then
        ${SED} -e "s/: *my_early_size/: my_early_size.load(std::memory_order_relaxed)/" \
          ${TBBROOT}/include/tbb/concurrent_vector.h > \
          ${HERE}/include/tbb/concurrent_vector.h
      fi
      if [ -d ${HERE}/lib ]; then
        echo -n " delete old directory lib, and"
        ${RM} -r ${HERE}/lib
      fi
      echo " deep-copy from ${TBBROOT}/lib"
      ${MKDIR} -p ${HERE}/lib/${TBBPLTFRM}
      for DIR in ${TBBLIBDIR}; do
        if [ -e ${TBBROOT}/${DIR}/libtbb.so ]; then
          ${CP} -L ${TBBROOT}/${DIR}/libtbb* ${HERE}/lib/${TBBPLTFRM}
          break
        else
          TBBRTDIR=$(${LS} -1 ${TBBROOT}/${DIR} 2>/dev/null | ${SORT} -V | ${TAIL} -n1)
          if [ -e ${TBBROOT}/${DIR}/${TBBRTDIR}/libtbb.so ]; then
            ${CP} -L ${TBBROOT}/${DIR}/${TBBRTDIR}/libtbb* ${HERE}/lib/${TBBPLTFRM}
            break
          fi
        fi
      done
      if [ -e ${HERE}/lib/${TBBPLTFRM}/libtbb.so ]; then
        ${SED} -e "s/\(.*\)-L\(..*\)intel64/\1-L\2${TBBPLTFRM}/" ${HERE}/ya.make.template > ${HERE}/ya.make
        echo "Successfully completed."
      else
        echo "Error: cannot locate libraries!"
        exit 1
      fi
    else
      echo "No action performed."
      exit 0
    fi
  else
    echo "Error: cannot find Intel TBB!"
    exit 1
  fi
else
  echo "Error: missing prerequisites!"
  exit 1
fi

