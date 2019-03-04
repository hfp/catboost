LIBRARY()



SRCS(
    index_range.cpp
)

IF (TBB)
    PEERDIR(contrib/libs/tbb)
ENDIF()

END()
