project(application VERSION 1.0.0 DESCRIPTION "Nerine Engine")

file(GLOB_RECURSE SOURCE_FILES "*.cpp" "*.h")

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(${PROJECT_NAME} PRIVATE
	${PROJECT_SOURCE_DIR}/../Vendor/stb_image 
	${PROJECT_SOURCE_DIR}/../Vendor/imgui
	${PROJECT_SOURCE_DIR}/../Vendor/rapidjson/include
)



target_link_libraries(${PROJECT_NAME} PRIVATE
	Core
	RenderDescription
	glad
	glfw
	gli
)
