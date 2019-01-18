

RESOURCES_LIBRARY()

IF (GOSTD_VERSION STREQUAL 1.11.4)
    IF (HOST_OS_LINUX)
        DECLARE_EXTERNAL_RESOURCE(GO_TOOLS sbr:776237383)
    ELSEIF (HOST_OS_DARWIN)
        DECLARE_EXTERNAL_RESOURCE(GO_TOOLS sbr:776239409)
    ELSEIF (HOST_OS_WINDOWS)
        DECLARE_EXTERNAL_RESOURCE(GO_TOOLS sbr:776240722)
    ELSE()
        MESSAGE(FATAL_ERROR Unsupported host platform)
    ENDIF()
ELSE()
    MESSAGE(FATAL_ERROR Unsupported version [$GOSTD] of Go Standard Library)
ENDIF()

END()
