execute_process(COMMAND git describe --always --tags --dirty
		OUTPUT_VARIABLE GIT_VERSION_
		ERROR_QUIET
		OUTPUT_STRIP_TRAILING_WHITESPACE)

if ("${GIT_VERSION_}" STREQUAL "")
	set(VERSION "char const * const DUMPVDL2_VERSION=\"${DUMPVDL2_VERSION}\";\n")
else()
	string(REGEX REPLACE ".*-g" "" GIT_VERSION ${GIT_VERSION_})
	set(VERSION "char const * const DUMPVDL2_VERSION=\"${DUMPVDL2_VERSION}-${GIT_VERSION}\";\n")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/version.c)
	file(READ ${CMAKE_CURRENT_SOURCE_DIR}/version.c VERSION_)
else()
	set(VERSION_ "")
endif()

if (NOT "${VERSION}" STREQUAL "${VERSION_}")
	file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/version.c "${VERSION}")
endif()
