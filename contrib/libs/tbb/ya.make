LIBRARY()

LICENSE(
    APACHE2
)

NO_COMPILER_WARNINGS()
NO_JOIN_SRC()
NO_RUNTIME()
NO_UTIL()

ADDINCL(
    GLOBAL contrib/libs/tbb/include
)

IF (TBB)
    CFLAGS(
        GLOBAL -D__TBB
    )
    LDFLAGS(
        -Lcontrib/libs/tbb/lib/intel64/gcc4.7
    )
    EXTRALIBS(
        -ltbb
    )    
)
ENDIF()

END()
