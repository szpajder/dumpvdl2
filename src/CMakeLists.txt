include(CheckCCompilerFlag)

# strdup
add_definitions(-D_XOPEN_SOURCE=600)

if(UNIX OR MINGW)
	add_definitions(-D_FILE_OFFSET_BITS=64)
endif()

find_library(LIBM m REQUIRED)

find_library(LIBPTHREAD pthread REQUIRED)
CHECK_C_COMPILER_FLAG(-pthread CC_HAS_PTHREAD)
if(CC_HAS_PTHREAD)
	add_definitions(-pthread)
endif()

CHECK_C_COMPILER_FLAG(-ffast-math CC_HAS_FFAST_MATH)
if(CC_HAS_FFAST_MATH)
	add_definitions(-ffast-math)
endif()

find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
list(APPEND dumpvdl2_extra_libs ${GLIB2_LIBRARIES})
list(APPEND dumpvdl2_include_dirs ${GLIB2_INCLUDE_DIRS})
list(APPEND link_dirs ${GLIB2_LIBRARY_DIRS})

pkg_check_modules(ACARS REQUIRED libacars)
list(APPEND dumpvdl2_extra_libs ${ACARS_LIBRARIES})
list(APPEND link_dirs ${ACARS_LIBRARY_DIRS})

option(RTLSDR "Enable RTL-SDR support" ON)
set(WITH_RTLSDR FALSE)

option(MIRISDR "Enable Mirics support" ON)
set(WITH_MIRISDR FALSE)

option(SDRPLAY "Enable SDRPlay support" ON)
set(WITH_SDRPLAY FALSE)

option(SOAPYSDR "Enable SoapySDR support" ON)
set(WITH_SOAPYSDR FALSE)

option(ETSY_STATSD "Enable Etsy StatsD support" ON)
set(USE_STATSD FALSE)

# TODO: LDFLAGS, INCLUDE_DIRS, CFLAGS, CFLAGS_OTHER
if(RTLSDR)
	pkg_check_modules(RTLSDR librtlsdr)
	if(RTLSDR_FOUND)
		set(WITH_RTLSDR TRUE)
		list(APPEND dumpvdl2_extra_sources rtl.c)
		list(APPEND dumpvdl2_extra_libs ${RTLSDR_LIBRARIES})
		list(APPEND dumpvdl2_include_dirs ${RTLSDR_INCLUDE_DIRS})
		list(APPEND link_dirs ${RTLSDR_LIBRARY_DIRS})
	endif()
endif()

if(MIRISDR)
	pkg_check_modules(MIRISDR libmirisdr)
	if(MIRISDR_FOUND)
		set(WITH_MIRISDR TRUE)
		list(APPEND dumpvdl2_extra_sources mirics.c)
		list(APPEND dumpvdl2_extra_libs ${MIRISDR_LIBRARIES})
		list(APPEND dumpvdl2_include_dirs ${MIRISDR_INCLUDE_DIRS})
		list(APPEND link_dirs ${MIRISDR_LIBRARY_DIRS})
	endif()
endif()

if(SDRPLAY)
	find_library(SDRPLAY_FOUND mirsdrapi-rsp)
	if(SDRPLAY_FOUND)
		set(WITH_SDRPLAY TRUE)
		list(APPEND dumpvdl2_extra_sources sdrplay.c)
		list(APPEND dumpvdl2_extra_libs mirsdrapi-rsp)
	endif()
endif()

if(SOAPYSDR)
	pkg_check_modules(SOAPYSDR SoapySDR)
	if(SOAPYSDR_FOUND)
		set(WITH_SOAPYSDR TRUE)
		list(APPEND dumpvdl2_extra_sources soapysdr.c)
		list(APPEND dumpvdl2_extra_libs ${SOAPYSDR_LIBRARIES})
		list(APPEND dumpvdl2_include_dirs ${SOAPYSDR_INCLUDE_DIRS})
		list(APPEND link_dirs ${SOAPYSDR_LIBRARY_DIRS})
	endif()
endif()

if(ETSY_STATSD)
	find_library(STATSD_FOUND statsdclient)
	if(STATSD_FOUND)
		set(USE_STATSD TRUE)
		list(APPEND dumpvdl2_extra_sources statsd.c)
		list(APPEND dumpvdl2_extra_libs statsdclient)
	endif()
endif()

message(STATUS "dumpvdl2 configuration summary:")
message(STATUS "- SDR drivers::")
message(STATUS "  - librtsdr:\trequested: ${RTLSDR}\tenabled: ${WITH_RTLSDR}")
message(STATUS "  - mirisdr:\t\trequested: ${MIRISDR}\tenabled: ${WITH_MIRISDR}")
message(STATUS "  - sdrplay:\t\trequested: ${SDRPLAY}\tenabled: ${WITH_SDRPLAY}")
message(STATUS "  - soapysdr:\trequested: ${SOAPYSDR}\tenabled: ${WITH_SOAPYSDR}")

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
	"${CMAKE_CURRENT_SOURCE_DIR}/config.h"
	@ONLY
)

add_subdirectory (asn1)
add_subdirectory (libfec)

add_library (dumpvdl2_base OBJECT
	acars.c
	asn1-format-common.c
	asn1-format-icao.c
	asn1-util.c
	avlc.c
	bitstream.c
	chebyshev.c
	clnp.c
	cotp.c
	crc.c
	decode.c
	demod.c
	dumpvdl2.c
	esis.c
	icao.c
	idrp.c
	output.c
	rs.c
	tlv.c
	util.c
	x25.c
	xid.c
)

include_directories (
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/asn1
)

target_include_directories (dumpvdl2_base PUBLIC
	${dumpvdl2_include_dirs}
)

# can't do this per target with cmake <3.13
link_directories(${link_dirs})

add_library (dumpvdl2_optional OBJECT
	${dumpvdl2_extra_sources}
)

set(dumpvdl2_obj_files
	$<TARGET_OBJECTS:dumpvdl2_base>
	$<TARGET_OBJECTS:dumpvdl2_optional>
	$<TARGET_OBJECTS:asn1>
	$<TARGET_OBJECTS:fec>
)

add_executable (dumpvdl2 ${dumpvdl2_obj_files})

target_link_libraries (dumpvdl2
	m
	pthread
	${dumpvdl2_extra_libs}
)

install(TARGETS dumpvdl2
	RUNTIME DESTINATION bin
)
