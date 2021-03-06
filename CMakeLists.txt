############################################################################
# CMakeLists.txt
# Copyright (C) 2014  Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

cmake_minimum_required(VERSION 3.2.0 FATAL_ERROR) # 3.2.0 required for target_compile_features support on gcc 4.4+ (centos6)

project(flexisip C CXX)

set(FLEXISIP_MAJOR_VERSION "1")
set(FLEXISIP_MINOR_VERSION "0")
set(FLEXISIP_MICRO_VERSION "12")
set(FLEXISIP_VERSION "${FLEXISIP_MAJOR_VERSION}.${FLEXISIP_MINOR_VERSION}.${FLEXISIP_MICRO_VERSION}")

include(CMakePushCheckState)
include(CMakeDependentOption)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(FeatureSummary)
include(CheckCXXSourceCompiles)
include(GNUInstallDirs)

option(ENABLE_DATEHANDLER "Build DateHandler module" NO)
option(ENABLE_DOC "Build documentation" YES)
option(ENABLE_MONOTONIC_CLOCK_REGISTRATIONS "Enable monotonic clock for registrations" NO)
option(ENABLE_ODBC "Build ODBC support for database connection" YES)
option(ENABLE_PRESENCE "Build presence support" NO)
option(ENABLE_PROTOBUF "Build with protobuf support" NO)
option(ENABLE_PUSHNOTIFICATION "Build with remote push notification support" YES)
option(ENABLE_REDIS "Build with Redis support" NO)
option(ENABLE_SNMP "Build with SNMP support" NO)
option(ENABLE_SOCI "Build with SOCI support" YES)
option(ENABLE_STATIC "Build static library (default is shared library)." NO)
option(ENABLE_TRANSCODER "Build transcoder support" YES)
cmake_dependent_option(ENABLE_SPECIFIC_FEATURES "Enable mediarelay specific features" OFF "ENABLE_TRANSCODER" OFF)


if(APPLE)
set(XSDCXX_DEFAULT_ROOT_PATH "/usr/local")
else ()
set(XSDCXX_DEFAULT_ROOT_PATH "/usr")
endif()

set(XSDCXX_ROOT_PATH ${XSDCXX_DEFAULT_ROOT_PATH}  CACHE STRING "Path of where the bin/xsdcxx executable will be found. Comes from http://www.codesynthesis.com/products/xsd/download.xhtml. On mac use 'brew install xsd'")
set(SYSCONFIGROOT "" CACHE STRING "Config directory, the place where flexisip expects its flexisip.conf file to reside" )


set(CONFIG_DIR ${SYSCONFIGROOT}/etc)
message(STATUS "Config dir: ${CONFIG_DIR}")

function(FIND_PROGRAM_REQUIRED varname progname)
	find_program(${varname} NAMES "${progname}")
	if(NOT ${varname})
		message(FATAL_ERROR "Program '${progname}' is required but could not be found")
	endif()
endfunction()


list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Required packages
find_package(SofiaSipUa 1.13.11 REQUIRED) #find_package only allow numbers
if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	include("${EP_ortp_CONFIG_DIR}/ORTPConfig.cmake")
else()
	find_package(ORTP 0.21 REQUIRED)
endif()

find_package(BcToolbox 0.0.3 REQUIRED)

find_program(XSDCXX_PROG NAMES "xsdcxx" "xsd"
	HINTS ${XSDCXX_ROOT_PATH}/bin
)
if(XSDCXX_PROG)
	set(ENABLE_XSD 1)
	message(STATUS "XSD found at ${XSDCXX_PROG}, enabling XSD")
	# TODO: check XSD is the correct executable
	find_library(XERCES_LIBS NAMES xerces-c)
	if(NOT XERCES_LIBS)
		message(FATAL_ERROR "Failed to find the Xerces library.")
	endif()
	find_path(XERCES_INCLUDE_DIRS NAMES xercesc/util/XercesDefs.hpp)
	if(NOT XERCES_INCLUDE_DIRS)
		message(FATAL_ERROR "Failed to find the Xerces includes.")
	endif()
else()
	set(ENABLE_XSD 0)
	message(STATUS "Program 'xsdcxx' could not be found in ${XSDCXX_ROOT_PATH}/bin, disabling XSD features")
endif()

check_function_exists(arc4random HAVE_ARC4RANDOM)
find_file(HAVE_SYS_PRCTL_H NAMES sys/prctl.h)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.7)
		message(STATUS "GCC version < 4.7: use MONOTONIC clock")
		add_definitions(-DUSE_MONOTONIC_CLOCK)
	else()
		message(STATUS "GCC version ${CMAKE_CXX_COMPILER_VERSION} should have steady_clock")
	endif()
endif()

set(CMAKE_REQUIRED_LIBRARIES)

# Options
if(ENABLE_SNMP)
	# todo: Not quite ready
	FIND_PROGRAM_REQUIRED(NET_SNMP_PROG net-snmp-config)
	find_path(NET_SNMP_INCLUDE_DIRS NAMES net-snmp/net-snmp-config.h)
	if(NOT NET_SNMP_INCLUDE_DIRS)
		message(FATAL_ERROR "SNMP header files not found")
	endif()

	execute_process(COMMAND "${NET_SNMP_PROG}" "--agent-libs" OUTPUT_VARIABLE NET_SNMP_LIBRARIES OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

if(ENABLE_ODBC)
	FIND_PROGRAM_REQUIRED(ODBC_PROG odbc_config)
	execute_process(COMMAND "${ODBC_PROG}" "--libs" OUTPUT_VARIABLE ODBC_LIBRARIES OUTPUT_STRIP_TRAILING_WHITESPACE)
	execute_process(COMMAND "${ODBC_PROG}" "--cflags" OUTPUT_VARIABLE ODBC_CFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
	execute_process(COMMAND "${ODBC_PROG}" "--include-prefix" OUTPUT_VARIABLE ODBC_INCLUDE_DIRS OUTPUT_STRIP_TRAILING_WHITESPACE)
	execute_process(COMMAND "${ODBC_PROG}" "--lib-prefix" OUTPUT_VARIABLE ODBC_LIBRARY_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
	message(STATUS "ODBC libraries: ${ODBC_LIBRARIES}")
	message(STATUS "ODBC headers: ${ODBC_CFLAGS}")

	# check SQL headers
	find_path(ODBC_SQL_HEADERS NAMES sql.h sqlext.h sqltypes.h mysql/mysql.h PATHS ODBC_INCLUDE_DIRS)
	if(NOT ODBC_SQL_HEADERS)
		message(FATAL_ERROR "ODBC Mysql headers not found.")
	else()
		message(STATUS "ODBC Mysql ${ODBC_SQL_HEADERS}")
	endif()

	# check that odbc_config gives us a correct library path
	find_library(ODBC_LIB_FOUND NAMES odbc PATHS ODBC_LIBRARY_DIR)
	if(NOT ODBC_LIB_FOUND)
		message(FATAL_ERROR "ODBC library not found.")
	endif()
endif()

if(ENABLE_TRANSCODER)
	find_package(Mediastreamer2 2.9.0 REQUIRED)
endif()

if(ENABLE_SPECIFIC_FEATURES)
	set(MEDIARELAY_SPECIFIC_FEATURES_ENABLED ON)
endif()

if(ENABLE_MONOTONIC_CLOCK_REGISTRATIONS)
	set(MONOTONIC_CLOCK_REGISTRATIONS ON)
endif()

if(ENABLE_DATEHANDLER)
	set(HAVE_DATEHANDLER ON)
endif()

if(ENABLE_REDIS)
	find_path(HIREDIS_INCLUDE_DIRS NAMES hiredis/hiredis.h)
	find_library(HIREDIS_LIBRARIES NAMES hiredis)
	if(NOT HIREDIS_INCLUDE_DIRS)
		message(FATAL_ERROR "hiredis headers not found")
	endif()
	if(NOT HIREDIS_LIBRARIES)
		message(FATAL_ERROR "hiredis library not found")
	endif()

	# check that the async mode is supported
	cmake_push_check_state(RESET)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${HIREDIS_INCLUDE_DIRS})
	list(APPEND CMAKE_REQUIRED_LIBRARIES ${HIREDIS_LIBRARIES})
	check_symbol_exists("redisAsyncCommand" "hiredis/async.h" HIREDIS_ASYNC_ENABLED)
	cmake_pop_check_state()
	if(NOT HIREDIS_ASYNC_ENABLED)
		message(FATAL_ERROR "Hiredis needs support for Async commands.")
	endif()
endif()

if(ENABLE_PROTOBUF)
	find_package(protobuf REQUIRED)
	# package finder for protobuf does not exit on REQUIRED..
	if(NOT PROTOBUF_FOUND)
		message(FATAL_ERROR "Protobuf not found and is required")
	endif()
	if(NOT PROTOBUF_PROTOC_EXECUTABLE)
		message(FATAL_ERROR "Protobuf 'protoc' executable not found and is required")
	endif()
endif()

if(ENABLE_DOC)
	FIND_PROGRAM_REQUIRED(PDFLATEX_PROG pdflatex)
endif()

if(ENABLE_PRESENCE)
		# disable Presence on gcc < 4.7 because c++11 is not supported well before
		if (CMAKE_COMPILER_IS_GNUCXX AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.7)
				message(WARNING "Disable Presence because GCC is too old")
				set(ENABLE_PRESENCE OFF)
		endif()

	if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
		include("${EP_bellesip_CONFIG_DIR}/BelleSIPConfig.cmake")
	else()
		find_package(BelleSIP 1.2.4 REQUIRED)
	endif()
endif()

if(ENABLE_SOCI)
	find_package(Soci REQUIRED COMPONENTS mysql)
	find_path(SOCI_MYSQL_INCLUDES NAMES mysql.h PATH_SUFFIXES mysql)
endif()


find_path(MSGPACK_INCLUDE_DIRS NAMES msgpack.hpp HINTS /usr/local/include)
if(MSGPACK_INCLUDE_DIRS)
	message(STATUS "MSGPACK found")
	add_definitions("-DENABLE_MSGPACK")
	set(ENABLE_MSGPACK 1)
else()
	message(STATUS "MSGPACK not found")
endif()


find_package(OpenSSL 0.9.8 REQUIRED)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

include_directories(
	src/
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_BINARY_DIR}/src/
	${SOFIASIPUA_INCLUDE_DIRS}
	${ORTP_INCLUDE_DIRS}
)

configure_file(${PROJECT_SOURCE_DIR}/cmake/flexisip-config.h.in ${PROJECT_BINARY_DIR}/flexisip-config.h)
set_source_files_properties(${PROJECT_BINARY_DIR}/flexisip-config.h PROPERTIES GENERATED ON)
add_definitions("-DHAVE_CONFIG_H")

add_subdirectory(src)

# Packaging

set(CPACK_GENERATOR TGZ;DEB;RPM)
set(CPACK_PACKAGE_VERSION_MAJOR ${FLEXISIP_MAJOR_VERSION})
set(CPACK_PACKAGE_VERSION_MINOR ${FLEXISIP_MINOR_VERSION})
set(CPACK_PACKAGE_VERSION_PATCH ${FLEXISIP_MICRO_VERSION})
set(CPACK_PACKAGE_VERSION ${FLEXISIP_VERSION})
set(CPACK_PACKAGE_CONTACT info@belledonne-communications.com)

include(CPack)
