project(glad VERSION 1.0.0 DESCRIPTION "OpenGL glad library")

file(GLOB_RECURSE SOURCE_FILES "*.c" "*.cpp" "*.h" "*.hpp")

add_library(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
)
