cmake_minimum_required(VERSION 3.24)
project(DicomViewer LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
find_package(DCMTK 3.6.9 CONFIG REQUIRED)

add_library(dicom_core
    src/dicomviewer/dicomImage.cpp
    src/dicomviewer/renderedFrame.cpp
    src/dicomviewer/window.cpp
)
target_compile_features(dicom_core PUBLIC cxx_std_20)
target_include_directories(dicom_core PUBLIC src)
target_link_libraries(dicom_core
    PUBLIC DCMTK::dcmdata DCMTK::dcmimgle
    PRIVATE DCMTK::dcmjpeg
)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
