LIBRARY()

LICENSE(
    APACHE2
)

NO_UTIL()
NO_RUNTIME()
NO_JOIN_SRC()
NO_COMPILER_WARNINGS()

ADDINCL(
    GLOBAL contrib/libs/tbb/include
)

CFLAGS(
    -D__TBB
)

LDFLAGS(
    -Lcontrib/libs/tbb/lib/intel64/gcc4.7
)

EXTRALIBS(
    -ltbb
)

END()
