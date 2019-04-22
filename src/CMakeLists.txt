include(CheckCCompilerFlag)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(TestBigEndian)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")

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

TEST_BIG_ENDIAN(IS_BIG_ENDIAN)

set(CMAKE_REQUIRED_FLAGS_ORIG ${CMAKE_REQUIRED_FLAGS})
if(CC_HAS_PTHREAD)
	set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -pthread")
endif()
CHECK_SYMBOL_EXISTS(pthread_barrier_wait pthread.h HAVE_PTHREAD_BARRIERS)
if(NOT HAVE_PTHREAD_BARRIERS)
	list(APPEND dumpvdl2_extra_sources pthread_barrier.c)
endif()
set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_ORIG})

set(CMAKE_REQUIRED_DEFINITIONS_ORIG ${CMAKE_REQUIRED_DEFINITIONS})
list(APPEND CMAKE_REQUIRED_DEFINITIONS "-D_GNU_SOURCE")
set(CMAKE_REQUIRED_LIBRARIES_ORIG ${CMAKE_REQUIRED_LIBRARIES})
list(APPEND CMAKE_REQUIRED_LIBRARIES m)
CHECK_SYMBOL_EXISTS(sincosf math.h HAVE_SINCOSF)
if(HAVE_SINCOSF)
	set(SINCOSF "sincosf")
else()
	CHECK_SYMBOL_EXISTS(__sincosf math.h HAVE___SINCOSF)
	if(HAVE___SINCOSF)
		set(SINCOSF "__sincosf")
	endif()
endif()
if(NOT HAVE_SINCOSF AND NOT HAVE___SINCOSF)
	message(FATAL_ERROR "Required function sincosf() is unavailable")
endif()
set(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS_ORIG})
set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES_ORIG})

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
set(WITH_STATSD FALSE)

if(RTLSDR)
	find_package(RTLSDR)
	if(RTLSDR_FOUND)
		list(APPEND dumpvdl2_extra_sources rtl.c)
		list(APPEND dumpvdl2_extra_libs ${RTLSDR_LIBRARIES})
		list(APPEND dumpvdl2_include_dirs ${RTLSDR_INCLUDE_DIRS})
		list(APPEND link_dirs ${RTLSDR_LIBRARY_DIRS})
		set(WITH_RTLSDR TRUE)
	endif()
endif()

if(MIRISDR)
	find_package(MiriSDR)
	if(MIRISDR_FOUND)
		set(WITH_MIRISDR TRUE)
		list(APPEND dumpvdl2_extra_sources mirics.c)
		list(APPEND dumpvdl2_extra_libs ${MIRISDR_LIBRARIES})
		list(APPEND dumpvdl2_include_dirs ${MIRISDR_INCLUDE_DIRS})
		list(APPEND link_dirs ${MIRISDR_LIBRARY_DIRS})
	endif()
endif()

if(SDRPLAY)
	find_package(LibSDRplay)
	if (LIBSDRPLAY_FOUND)
		list(APPEND dumpvdl2_extra_sources sdrplay.c)
		list(APPEND dumpvdl2_extra_libs ${LIBSDRPLAY_LIBRARIES})
		list(APPEND dumpvdl2_include_dirs ${LIBSDRPLAY_INCLUDE_DIRS})
		set(WITH_SDRPLAY TRUE)
	endif()
endif()

if(SOAPYSDR)
	message(STATUS "Checking for SoapySDR")
	find_package(SoapySDR NO_MODULE)
	if(SoapySDR_FOUND)
		list(APPEND dumpvdl2_extra_sources soapysdr.c)
		message(STATUS "  SoapySDR found, ${SoapySDR_INCLUDE_DIRS}, ${SoapySDR_LIBRARIES}")
		list(APPEND dumpvdl2_extra_libs ${SoapySDR_LIBRARIES})
		list(APPEND dumpvdl2_include_dirs ${SoapySDR_INCLUDE_DIRS})
		set(WITH_SOAPYSDR TRUE)
	else()
		message(STATUS "  SoapySDR not found")
	endif()
endif()

if(ETSY_STATSD)
	find_library(STATSD_FOUND statsdclient)
	if(STATSD_FOUND)
		list(APPEND dumpvdl2_extra_sources statsd.c)
		list(APPEND dumpvdl2_extra_libs statsdclient)
		set(WITH_STATSD TRUE)
	endif()
endif()

message(STATUS "dumpvdl2 configuration summary:")
message(STATUS "- SDR drivers:")
message(STATUS "  - librtsdr:\trequested: ${RTLSDR}\tenabled: ${WITH_RTLSDR}")
message(STATUS "  - mirisdr:\t\trequested: ${MIRISDR}\tenabled: ${WITH_MIRISDR}")
message(STATUS "  - sdrplay:\t\trequested: ${SDRPLAY}\tenabled: ${WITH_SDRPLAY}")
message(STATUS "  - soapysdr:\trequested: ${SOAPYSDR}\tenabled: ${WITH_SOAPYSDR}")
message(STATUS "- Other options:")
message(STATUS "  - Etsy StatsD:\trequested: ${ETSY_STATSD}\tenabled: ${WITH_STATSD}")

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
	"${CMAKE_CURRENT_SOURCE_DIR}/config.h"
	@ONLY
)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.c
		${CMAKE_CURRENT_BINARY_DIR}/_version.c
	COMMAND ${CMAKE_COMMAND} -DDUMPVDL2_VERSION=${DUMPVDL2_VERSION} -P ${CMAKE_CURRENT_SOURCE_DIR}/version.cmake
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
	${CMAKE_CURRENT_BINARY_DIR}/version.c
	${dumpvdl2_extra_sources}
)

target_include_directories (dumpvdl2_base PUBLIC
	${dumpvdl2_include_dirs}
)

# can't do this per target with cmake <3.13
link_directories(${link_dirs})

set(dumpvdl2_obj_files
	$<TARGET_OBJECTS:dumpvdl2_base>
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