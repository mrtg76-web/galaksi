cmake_minimum_required(VERSION 3.22)
project(GALAKSI VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(GK_BUILD_PLUGIN "Build the JUCE plugin" ON)
option(GK_BUILD_TESTS "Build the headless core tests" ON)
option(GK_BUILD_SNAPSHOT "Build the UI snapshot tool (development)" OFF)
set(GK_JUCE_DIR "" CACHE PATH "Local JUCE checkout (fetched automatically when empty)")
set(GK_RUBBERBAND_DIR "" CACHE PATH "Local Rubber Band checkout (fetched automatically when empty)")

include(FetchContent)

# ---------------------------------------------------------------- Rubber Band (GPL, single-file build)
if(NOT GK_RUBBERBAND_DIR)
    FetchContent_Declare(rubberband
        GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git
        GIT_TAG v4.0.0
        GIT_SHALLOW ON)
    FetchContent_GetProperties(rubberband)
    if(NOT rubberband_POPULATED)
        FetchContent_Populate(rubberband)
    endif()
    set(GK_RUBBERBAND_DIR ${rubberband_SOURCE_DIR})
endif()

add_library(gk_rubberband STATIC ${GK_RUBBERBAND_DIR}/single/RubberBandSingle.cpp)
target_include_directories(gk_rubberband PUBLIC ${GK_RUBBERBAND_DIR})
if(MSVC)
    target_compile_definitions(gk_rubberband PRIVATE _USE_MATH_DEFINES NOMINMAX)
    target_compile_options(gk_rubberband PRIVATE /w)
else()
    target_compile_options(gk_rubberband PRIVATE -w)
endif()
if(APPLE)
    target_link_libraries(gk_rubberband PUBLIC "-framework Accelerate")
endif()

# ---------------------------------------------------------------- core (framework independent)
add_library(gk_core STATIC core/Analysis.cpp core/Audio.cpp core/Percussion.cpp core/Adg.cpp)
target_include_directories(gk_core PUBLIC core PRIVATE third_party/minimp3)
target_link_libraries(gk_core PUBLIC gk_rubberband)

if(GK_BUILD_TESTS)
    enable_testing()
    add_executable(gk_tests tests/test_core.cpp)
    target_link_libraries(gk_tests PRIVATE gk_core)
    target_compile_definitions(gk_tests PRIVATE GK_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME gk_tests COMMAND gk_tests)
    add_executable(gk_perc_tests tests/test_perc.cpp)
    target_link_libraries(gk_perc_tests PRIVATE gk_core)
    add_test(NAME gk_perc_tests COMMAND gk_perc_tests)
endif()

# ---------------------------------------------------------------- plugin
if(GK_BUILD_PLUGIN)
    if(GK_JUCE_DIR)
        add_subdirectory(${GK_JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW ON)
        FetchContent_MakeAvailable(JUCE)
    endif()

    if(APPLE)
        set(GK_FORMATS VST3 AU Standalone)
    elseif(WIN32)
        set(GK_FORMATS VST3 Standalone)
    else()
        set(GK_FORMATS VST3)
    endif()

    juce_add_plugin(Galaksi
        COMPANY_NAME "Galaksi"
        BUNDLE_ID "com.galaksi.radiosampler"
        PLUGIN_MANUFACTURER_CODE Glks
        PLUGIN_CODE Gks1
        FORMATS ${GK_FORMATS}
        PRODUCT_NAME "Galaksi"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Instrument Sampler
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        PLIST_TO_MERGE "<plist><dict><key>NSAppTransportSecurity</key><dict><key>NSAllowsArbitraryLoads</key><true/></dict></dict></plist>")

    target_sources(Galaksi PRIVATE
        plugin/PluginProcessor.cpp
        plugin/PluginEditor.cpp
        plugin/RadioNet.cpp
        plugin/FreesoundNet.cpp
        plugin/SlotEngine.cpp)

    target_compile_definitions(Galaksi PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    if(UNIX AND NOT APPLE)
        target_compile_definitions(Galaksi PUBLIC JUCE_ALSA=0 JUCE_JACK=0)
    endif()

    target_link_libraries(Galaksi
        PRIVATE
            gk_core
            juce::juce_audio_processors
            juce::juce_audio_formats
            juce::juce_audio_devices
            juce::juce_audio_utils
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

if(GK_BUILD_PLUGIN AND GK_BUILD_SNAPSHOT)
    add_executable(gk_snapshot tests/snapshot.cpp)
    target_include_directories(gk_snapshot PRIVATE plugin $<TARGET_PROPERTY:Galaksi,INCLUDE_DIRECTORIES>)
    target_compile_definitions(gk_snapshot PRIVATE $<TARGET_PROPERTY:Galaksi,COMPILE_DEFINITIONS>)
    target_link_libraries(gk_snapshot PRIVATE Galaksi)
endif()
