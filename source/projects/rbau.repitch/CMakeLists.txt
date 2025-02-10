# Copyright 2018 The Min-DevKit Authors. All rights reserved.
# Use of this source code is governed by the MIT License found in the License.md file.

cmake_minimum_required(VERSION 3.19)

set(C74_MIN_API_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../min-api)
include(${C74_MIN_API_DIR}/script/min-pretarget.cmake)

set(CMTK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../cmtk)



#############################################################
# MAX EXTERNAL
#############################################################


include_directories( 
	"${C74_INCLUDES}"
	"${CMTK_DIR}/src"
)

set( SOURCE_FILES
	${CMTK_DIR}/src/Chord.cpp
	${CMTK_DIR}/src/Interval.cpp
	${CMTK_DIR}/src/Note.cpp
	${CMTK_DIR}/src/Scale.cpp
	${PROJECT_NAME}.cpp
)

add_library( 
	${PROJECT_NAME} 
	MODULE
	${SOURCE_FILES}
)


include(${C74_MIN_API_DIR}/script/min-posttarget.cmake)


#############################################################
# UNIT TEST
#############################################################

include(${C74_MIN_API_DIR}/test/min-object-unittest.cmake)
