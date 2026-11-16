find_package(GTest CONFIG REQUIRED)

add_executable(dicom_unit_tests window_test.cpp)
target_link_libraries(dicom_unit_tests PRIVATE dicom_core GTest::gtest_main)
add_test(NAME dicom.unit COMMAND dicom_unit_tests)

add_executable(dicom_integration_tests dicom_image_test.cpp)
target_link_libraries(dicom_integration_tests PRIVATE dicom_core GTest::gtest_main
    DCMTK::dcmdata DCMTK::dcmjpeg)
add_test(NAME dicom.integration COMMAND dicom_integration_tests)
