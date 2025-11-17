cmake_minimum_required(VERSION 3.15)

################################
# set lib version here
project(TinyEXIF VERSION 1.0.4)
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
