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
    PRIVATE DCMTK::dcmimage DCMTK::dcmjpeg DCMTK::dcmjpls
)

include(CTest)
option(DICOMVIEWER_BUILD_APP "Build the Qt Widgets viewer" ON)
if(DICOMVIEWER_BUILD_APP)
    find_package(Qt6 CONFIG REQUIRED COMPONENTS Widgets Concurrent)
    add_library(dicom_viewer_ui STATIC
        src/viewer/mainWindow.cpp
        src/viewer/imageViewport.cpp
        src/viewer/imageLoader.cpp
        src/viewer/metadataPanel.cpp
        src/viewer/contrastSpinBox.cpp
    )
    target_link_libraries(dicom_viewer_ui PUBLIC dicom_core Qt6::Widgets PRIVATE Qt6::Concurrent)
    add_executable(dicom_viewer src/viewer/main.cpp)
    target_link_libraries(dicom_viewer PRIVATE dicom_viewer_ui)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
