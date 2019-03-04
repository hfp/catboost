LIBRARY()



SRCS(
    index_range.cpp
)

IF (TBB)
    PEERDIR(contrib/libs/tbb)
ELSE()
    ADDINCL(GLOBAL contrib/libs)
ENDIF()

END()
