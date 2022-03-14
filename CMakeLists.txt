cmake_minimum_required(VERSION 3.0.2)
project(enc_extraction)

find_package(catkin_simple REQUIRED)
catkin_python_setup()
catkin_simple(ALL_DEPS_REQUIRED)

find_library(gdal_LIBRARIES
  gdal
)

#############
# LIBRARIES #
#############
cs_add_library(${PROJECT_NAME}
  src/enc_extract_lib.cpp
)

#add_library(graph_builder src/graph_builder.cpp)
#target_compile_options(graph_builder PRIVATE ${OpenMP_FLAGS})
#add_dependencies(graph_builder ${catkin_EXPORTED_TARGETS})
#target_link_libraries(graph_builder ${catkin_LIBRARIES} ${OpenMP_LIBS})
#install(TARGETS graph_builder
#  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
#  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
#  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
#)

############
# BINARIES #
############

cs_add_executable(test_enc_extraction
  src/test_enc_extraction.cpp
)
target_link_libraries(test_enc_extraction ${PROJECT_NAME} ${gdal_LIBRARIES})

#cs_add_executable(sim_vessel_node
#  src/sim_vessel_node.cpp
#)
#target_link_libraries(sim_vessel_node ${PROJECT_NAME})


##########
# EXPORT #
##########
cs_install()
cs_export()

## Install python nodes
#set(py_nodes
#  los_controller_node
#)

foreach(py_node IN LISTS py_nodes)
  catkin_install_python(PROGRAMS scripts/${py_node}.py
    DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION})
endforeach()