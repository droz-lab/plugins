# Briques DSP communes, C++ pur, sans JUCE : utilisables et testables partout.
add_library(droz_dsp INTERFACE)
add_library(droz::dsp ALIAS droz_dsp)
target_include_directories(droz_dsp INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(droz_dsp INTERFACE cxx_std_20)

add_executable(droz_dsp_tests tests/tests.cpp)
target_link_libraries(droz_dsp_tests PRIVATE droz::dsp)
add_test(NAME droz_dsp_tests COMMAND droz_dsp_tests)
