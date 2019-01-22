LIBRARY()

NO_PLATFORM()

ADDINCL(GLOBAL contrib/libs/tbb/include)

CFLAGS(
    -D__TBB
)

LDFLAGS(
    -Lcontrib/libs/tbb/lib/intel64/gcc4.7
    -ltbb
)

END()
