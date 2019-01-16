cmake_minimum_required(VERSION 3.1)

project(TinyEXIF)
include(GNUInstallDirs)

find_package(tinyxml2 REQUIRED)

#CMAKE_BUILD_TOOL

################################
# set lib version here

set(GENERIC_LIB_VERSION "1.0.1")
set(GENERIC_LIB_SOVERSION "1")

################################
# Add definitions

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

################################
# Add targets
# By Default shared libray is being built
# To build static libs also - Do cmake . -DBUILD_STATIC_LIBS:BOOL=ON
# User can choose not to build shared library by using cmake -DBUILD_SHARED_LIBS:BOOL=OFF
# To build only static libs use cmake . -DBUILD_SHARED_LIBS:BOOL=OFF -DBUILD_STATIC_LIBS:BOOL=ON
# To build the demo binary, use cmake . -DBUILD_DEMO:BOOL=ON

option(BUILD_SHARED_LIBS "build as shared library" ON)
option(BUILD_STATIC_LIBS "build as static library" OFF)
option(LINK_CRT_STATIC_LIBS "link CRT static library" OFF)
option(BUILD_DEMO "build demo binary" ON)

# set MSVC runtime linkage to static or dynamic
# as in: https://stackoverflow.com/questions/10113017/setting-the-msvc-runtime-in-cmake
macro(configure_runtime CRT_RUNTIME)
	if(MSVC)
		# Default to statically-linked runtime.
		if("${CRT_RUNTIME}" STREQUAL "")
			set(CRT_RUNTIME "static")
		endif()
		# Set compiler options.
		set(variables
			CMAKE_C_FLAGS_DEBUG
			CMAKE_C_FLAGS_MINSIZEREL
			CMAKE_C_FLAGS_RELEASE
			CMAKE_C_FLAGS_RELWITHDEBINFO
			CMAKE_CXX_FLAGS_DEBUG
			CMAKE_CXX_FLAGS_MINSIZEREL
			CMAKE_CXX_FLAGS_RELEASE
			CMAKE_CXX_FLAGS_RELWITHDEBINFO
		)
		if(${CRT_RUNTIME} STREQUAL "static")
			message(STATUS "MSVC -> forcing use of statically-linked runtime.")
			foreach(variable ${variables})
				if(${variable} MATCHES "/MD")
					string(REGEX REPLACE "/MD" "/MT" ${variable} "${${variable}}")
				endif()
			endforeach()
		else()
			message(STATUS "MSVC -> forcing use of dynamically-linked runtime.")
			foreach(variable ${variables})
				if(${variable} MATCHES "/MT")
					string(REGEX REPLACE "/MT" "/MD" ${variable} "${${variable}}")
				endif()
			endforeach()
		endif()
	endif()
endmacro()

if(LINK_CRT_STATIC_LIBS)
	# set MSVC runtime linkage to static
	configure_runtime("static")
endif()

# to distinguish between debug and release lib
set(CMAKE_DEBUG_POSTFIX "d")

if(BUILD_SHARED_LIBS)
	add_library(TinyEXIF SHARED TinyEXIF.cpp TinyEXIF.h)

	if(MSVC_VERSION GREATER 1300)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4251") # needs to have dll-interface
	endif()

	target_link_libraries(TinyEXIF tinyxml2::tinyxml2)
	set_target_properties(TinyEXIF PROPERTIES
		COMPILE_DEFINITIONS "TINYEXIF_EXPORT"
		VERSION "${GENERIC_LIB_VERSION}"
		SOVERSION "${GENERIC_LIB_SOVERSION}")


	if(DEFINED CMAKE_VERSION AND NOT "${CMAKE_VERSION}" VERSION_LESS "2.8.11")
		target_include_directories(TinyEXIF PUBLIC 
								$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
								$<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>)

		if(MSVC)
			target_compile_definitions(TinyEXIF PUBLIC _CRT_SECURE_NO_WARNINGS)
		endif()
	else()
		include_directories(${PROJECT_SOURCE_DIR})

		if(MSVC)
			add_definitions(-D_CRT_SECURE_NO_WARNINGS)
		endif()
	endif()

	# export targets for find_package config mode
	export(TARGETS TinyEXIF
			FILE ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}Targets.cmake)

	install(TARGETS TinyEXIF
			EXPORT ${CMAKE_PROJECT_NAME}Targets
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
			LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

if(BUILD_STATIC_LIBS)
	add_library(TinyEXIFstatic STATIC TinyEXIF.cpp TinyEXIF.h)
	
	target_link_libraries(TinyEXIFstatic tinyxml2::tinyxml2)
	set_target_properties(TinyEXIFstatic PROPERTIES
			OUTPUT_NAME TinyEXIF
			VERSION "${GENERIC_LIB_VERSION}"
			SOVERSION "${GENERIC_LIB_SOVERSION}")

	if(DEFINED CMAKE_VERSION AND NOT "${CMAKE_VERSION}" VERSION_LESS "2.8.11")
		target_include_directories(TinyEXIFstatic PUBLIC 
								$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
								$<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>)

		if(MSVC)
			target_compile_definitions(TinyEXIFstatic PUBLIC _CRT_SECURE_NO_WARNINGS)
		endif()
	else()
		include_directories(${PROJECT_SOURCE_DIR})

		if(MSVC)
			add_definitions(-D_CRT_SECURE_NO_WARNINGS)
		endif()
	endif()

	# export targets for find_package config mode
	export(TARGETS TinyEXIFstatic
			FILE ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}Targets.cmake)

	install(TARGETS TinyEXIFstatic
			EXPORT ${CMAKE_PROJECT_NAME}Targets
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
			LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

if(BUILD_DEMO)
	add_executable(TinyEXIFdemo main.cpp)
	if(BUILD_SHARED_LIBS)
		add_dependencies(TinyEXIFdemo TinyEXIF)
		target_link_libraries(TinyEXIFdemo TinyEXIF)
		target_compile_definitions(TinyEXIFdemo PRIVATE TINYEXIF_IMPORT)
	else(BUILD_STATIC_LIBS)
		add_dependencies(TinyEXIFdemo TinyEXIFstatic)
		target_link_libraries(TinyEXIFdemo TinyEXIFstatic tinyxml2::tinyxml2)
	endif()
endif()

install(FILES TinyEXIF.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

foreach(p LIB INCLUDE)
	set(var CMAKE_INSTALL_${p}DIR)
	if(NOT IS_ABSOLUTE "${${var}}")
		set(${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
	endif()
endforeach()

file(WRITE
	${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}Config.cmake
	"include(\${CMAKE_CURRENT_LIST_DIR}/${CMAKE_PROJECT_NAME}Targets.cmake)\n")

install(FILES
		${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}Config.cmake
		DESTINATION lib/cmake/${CMAKE_PROJECT_NAME})

install(EXPORT ${CMAKE_PROJECT_NAME}Targets
		DESTINATION lib/cmake/${CMAKE_PROJECT_NAME})
