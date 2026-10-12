cmake_minimum_required(VERSION 3.24)
project(PitchLab VERSION 0.1.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 91ad83ae34a81e0833b1a2b0866f54846370ae53)
FetchContent_Declare(rubberband GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git
    GIT_TAG 1d95888bec3ae0a17c0c4af791810d5a63f6bc35)
FetchContent_MakeAvailable(JUCE rubberband)
add_library(pitchlab_rubberband STATIC ${rubberband_SOURCE_DIR}/single/RubberBandSingle.cpp)
target_include_directories(pitchlab_rubberband PUBLIC ${rubberband_SOURCE_DIR})
target_compile_definitions(pitchlab_rubberband PRIVATE NOMINMAX _USE_MATH_DEFINES)
juce_add_gui_app(PitchLab PRODUCT_NAME "PitchLab" COMPANY_NAME "PitchLab Study" BUNDLE_ID "org.pitchlab.study"
    NEEDS_WEB_BROWSER FALSE NEEDS_CURL FALSE)
target_sources(PitchLab PRIVATE Source/Main.cpp Source/AudioEngine.h Source/PitchEngine.h Source/OutputGuard.h Source/Recorder.h)
target_compile_definitions(PitchLab PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_USE_FLAC=0 JUCE_USE_OGGVORBIS=0 JUCE_ASIO=0)
target_link_libraries(PitchLab PRIVATE juce::juce_audio_utils pitchlab_rubberband
    juce::juce_recommended_config_flags)
add_executable(PitchLabTests Tests/DspTests.cpp)
target_link_libraries(PitchLabTests PRIVATE pitchlab_rubberband)
if(MSVC)
    target_compile_options(PitchLab PRIVATE /utf-8 /O2)
    target_compile_options(PitchLabTests PRIVATE /utf-8 /O2)
endif()
enable_testing()
add_test(NAME PitchLabDSP COMMAND PitchLabTests)
