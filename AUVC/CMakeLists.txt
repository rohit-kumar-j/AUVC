cmake_minimum_required(VERSION 3.16)

add_subdirectory(apriltags)

# TODO : Test this
# find_package(OpenCV)
# include_directories(${OpenCV_INCLUDE_DIRS})

add_library(Controller SHARED controller.h  controller.cc )
set_property(TARGET Controller PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(Controller PROPERTIES COMPILE_FLAGS "-fvisibility=default")
target_link_libraries(Controller PUBLIC mujoco::mujoco) # TODO:Test and remove this
