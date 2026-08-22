cmake_minimum_required(VERSION 3.15)

################################
# Determine the library version from TinyEXIF.h (the single source of
# truth -- see the comment above TINYEXIF_MAJOR_VERSION in that file).
# TinyEXIF.h must stay self-contained since it is vendored directly by
# some consumers, so the version lives there and is read back out here
# rather than the header including a CMake-generated file.
#
# The three regexes below are anchored to the start of a line -- CMake's
# REGEX MATCH returns the first match anywhere in the file, so an unanchored
# pattern is satisfied by any mention of the macro in a comment or an #if 0
# block and reports a version the compiler never sees. The captured digits
# are therefore CMAKE_MATCH_2, after the leading-newline group.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/TinyEXIF.h" _tinyexif_h_content)

string(REGEX MATCH "(^|\n)[ \t]*#define[ \t]+TINYEXIF_MAJOR_VERSION[ \t]+([0-9]+)" _tinyexif_major_match "${_tinyexif_h_content}")
if(NOT _tinyexif_major_match)
	message(FATAL_ERROR "TinyEXIF.h: could not find '#define TINYEXIF_MAJOR_VERSION <n>'")
endif()
set(_tinyexif_major "${CMAKE_MATCH_2}")

string(REGEX MATCH "(^|\n)[ \t]*#define[ \t]+TINYEXIF_MINOR_VERSION[ \t]+([0-9]+)" _tinyexif_minor_match "${_tinyexif_h_content}")
if(NOT _tinyexif_minor_match)
	message(FATAL_ERROR "TinyEXIF.h: could not find '#define TINYEXIF_MINOR_VERSION <n>'")
endif()
set(_tinyexif_minor "${CMAKE_MATCH_2}")

string(REGEX MATCH "(^|\n)[ \t]*#define[ \t]+TINYEXIF_PATCH_VERSION[ \t]+([0-9]+)" _tinyexif_patch_match "${_tinyexif_h_content}")
if(NOT _tinyexif_patch_match)
	message(FATAL_ERROR "TinyEXIF.h: could not find '#define TINYEXIF_PATCH_VERSION <n>'")
endif()
set(_tinyexif_patch "${CMAKE_MATCH_2}")

set(TinyEXIF_VERSION "${_tinyexif_major}.${_tinyexif_minor}.${_tinyexif_patch}")

# vcpkg.json must stay a static file (vcpkg reads it directly from the
# repo, not through CMake), so cross-check it against TinyEXIF.h instead
# of generating it.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json" _tinyexif_vcpkg_json_content)
string(REGEX MATCH "\"version-string\"[ \t]*:[ \t]*\"([0-9]+\\.[0-9]+\\.[0-9]+)\"" _tinyexif_vcpkg_match "${_tinyexif_vcpkg_json_content}")
if(NOT _tinyexif_vcpkg_match)
	message(FATAL_ERROR "vcpkg.json: could not find '\"version-string\": \"<major.minor.patch>\"'")
endif()
set(_tinyexif_vcpkg_version "${CMAKE_MATCH_1}")

if(NOT _tinyexif_vcpkg_version STREQUAL TinyEXIF_VERSION)
	message(FATAL_ERROR
		"Version mismatch: TinyEXIF.h declares version ${TinyEXIF_VERSION} "
		"but vcpkg.json declares \"version-string\": \"${_tinyexif_vcpkg_version}\". "
		"Update vcpkg.json's version-string to match TinyEXIF.h.")
endif()

message(STATUS "TinyEXIF version (from TinyEXIF.h): ${TinyEXIF_VERSION}")

################################
# lib version is set in TinyEXIF.h; see above
project(TinyEXIF VERSION ${TinyEXIF_VERSION})
set(GENERIC_LIB_SOVERSION "1")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(tinyxml2 CONFIG REQUIRED)

################################
# Add definitions
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

################################
# Add targets
# By Default shared libray is being built
# User can choose to build static library by using cmake -DBUILD_SHARED_LIBS:BOOL=OFF
# To build the demo binary, use cmake . -DBUILD_DEMO:BOOL=ON
option(BUILD_SHARED_LIBS "build as shared library" ON)
option(LINK_CRT_STATIC_LIBS "link CRT static library" OFF)
option(BUILD_DEMO "build demo binary" ON)
# Off by default: only meant for local/CI fuzzing, and only buildable with a
# compiler that supports -fsanitize=fuzzer (Clang). See fuzz/README.md.
option(BUILD_FUZZER "build the libFuzzer target for EXIFInfo::parseFrom (requires Clang)" OFF)

# Set MSVC runtime library globally so all targets use consistent /MT or /MD.
# This avoids LNK2038 mismatches between targets compiled with different CRT variants.
if(MSVC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<NOT:$<BOOL:${LINK_CRT_STATIC_LIBS}>>:DLL>")
	if(MSVC_VERSION GREATER 1300)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4251") # needs to have dll-interface
	endif()
endif()

add_library(TinyEXIF TinyEXIF.cpp TinyEXIF.h)
add_library(TinyEXIF::TinyEXIF ALIAS TinyEXIF)

target_link_libraries(TinyEXIF tinyxml2::tinyxml2)
set_target_properties(TinyEXIF PROPERTIES
	DEFINE_SYMBOL "TINYEXIF_EXPORT"
	VERSION "${TinyEXIF_VERSION}"
	SOVERSION "${GENERIC_LIB_SOVERSION}"
	MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}"
)

target_include_directories(TinyEXIF PUBLIC
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
	"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_compile_definitions(TinyEXIF
	INTERFACE $<$<BOOL:${BUILD_SHARED_LIBS}>:TINYEXIF_IMPORT>
	PRIVATE $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
)

if(BUILD_DEMO)
	add_executable(TinyEXIFdemo main.cpp)
	set_target_properties(TinyEXIFdemo PROPERTIES MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}")
	target_link_libraries(TinyEXIFdemo TinyEXIF)
endif()

if(BUILD_FUZZER)
	# -fsanitize=fuzzer is a Clang feature (both upstream Clang and AppleClang);
	# fail clearly here instead of letting an unrelated flags/link error surface
	# on MSVC or GCC for an option nobody asked the default build to enable.
	if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		message(FATAL_ERROR "BUILD_FUZZER requires Clang (needs -fsanitize=fuzzer support); CMAKE_CXX_COMPILER_ID is '${CMAKE_CXX_COMPILER_ID}'")
	endif()
	add_executable(TinyEXIFfuzzer fuzz/fuzz_exif.cpp)
	target_link_libraries(TinyEXIFfuzzer TinyEXIF)
	target_compile_options(TinyEXIFfuzzer PRIVATE -fsanitize=fuzzer)
	target_link_options(TinyEXIFfuzzer PRIVATE -fsanitize=fuzzer)
endif()

################################
# Install targets
install(TARGETS TinyEXIF
	EXPORT TinyEXIFTargets
	RUNTIME	DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES TinyEXIF.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT TinyEXIFTargets
	FILE TinyEXIFTargets.cmake
	NAMESPACE TinyEXIF::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TinyEXIF
)

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/TinyEXIFConfig.cmake"
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TinyEXIF
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/TinyEXIFConfig.cmake"
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TinyEXIF
)
