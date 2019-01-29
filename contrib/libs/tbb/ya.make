LIBRARY()

LICENSE(
    APACHE2
)

when ($TBB == "yes") {
    NO_COMPILER_WARNINGS()
    NO_JOIN_SRC()
    NO_RUNTIME()
    NO_UTIL()

    ADDINCL(
        GLOBAL contrib/libs/tbb/include
    )

    CFLAGS(
        GLOBAL -D__TBB
    )

    LDFLAGS(
        -Lcontrib/libs/tbb/lib/intel64/gcc4.7
    )

    EXTRALIBS(
        -ltbb
    )
}

END()
