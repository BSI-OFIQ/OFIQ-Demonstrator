message(STATUS  "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_CXX_STANDARD 17)

# Configure built shared libraries in top-level lib directory
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/source)

set(OFIQ_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/OFIQ-Project")

# Define local include pathes
list(FILTER include_modules EXCLUDE REGEX "/src$")
include_directories (
	${SOURCE_DIR}/include
	${OFIQ_SRC_DIR}/OFIQlib/include/
	${OFIQ_SRC_DIR}/OFIQlib/modules/utils/
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build/conan")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build/conan")


# Add packages from conan
set(OpenCV_STATIC ON)
find_package(OpenCV REQUIRED COMPONENTS core calib3d imgcodecs imgproc highgui dnn ml)
set(wxWidgets_USE_STATIC ON)
find_package(wxWidgets REQUIRED gl core base)

add_library(onnxruntime SHARED IMPORTED)
set_target_properties(onnxruntime PROPERTIES
    IMPORTED_IMPLIB ${OFIQ_SRC_DIR}/extern/onnxruntime-win-x64-1.17.3/lib/onnxruntime.lib
    IMPORTED_LOCATION ${OFIQ_SRC_DIR}/extern/onnxruntime-win-x64-1.17.3/lib/onnxruntime.dll
    INTERFACE_INCLUDE_DIRECTORIES ${OFIQ_SRC_DIR}/extern/onnxruntime-win-x64-1.17.3/include
)
get_property(IMPORTED_LIB_LOCATION TARGET onnxruntime PROPERTY IMPORTED_LOCATION)
install(FILES "${IMPORTED_LIB_LOCATION}" DESTINATION ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})

add_library(ofiq_lib SHARED IMPORTED)
set_target_properties(ofiq_lib PROPERTIES
    IMPORTED_IMPLIB ${OFIQ_SRC_DIR}/build/build_win/${CMAKE_BUILD_TYPE}/ofiq_lib.lib
    IMPORTED_LOCATION ${OFIQ_SRC_DIR}/build/build_win/${CMAKE_BUILD_TYPE}/ofiq_lib.dll
    INTERFACE_INCLUDE_DIRECTORIES ${OFIQ_SRC_DIR}/OFIQlib/include
)
get_property(IMPORTED_LIB_LOCATION TARGET ofiq_lib PROPERTY IMPORTED_LOCATION)
install(FILES "${IMPORTED_LIB_LOCATION}" DESTINATION ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})

# Find all source files
list(APPEND PUBLIC_HEADER_LIST
	${SOURCE_DIR}/include/OFIQPictureFrame.h
)

#list(APPEND libImplementationSources
#	${SOURCE_DIR}/src/OFIQDemonstrator.cpp
#)

list(APPEND LINK_LIST 
	opencv::opencv
)

# add a test application
add_executable(OFIQDemonstrator WIN32 ${SOURCE_DIR}/src/OFIQDemonstrator.cpp)
target_link_libraries(OFIQDemonstrator PRIVATE ${wxWidgets_LIBRARIES} ofiq_lib onnxruntime ${LINK_LIST})
