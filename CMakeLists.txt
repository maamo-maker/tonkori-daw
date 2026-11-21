cmake_minimum_required(VERSION 3.22)

# macOS 13 (Ventura) is the oldest system this app must run on.
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")

project(TonkoriDAW VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/tracktion_engine)
if (NOT EXISTS ${TE_DIR}/modules/juce/CMakeLists.txt)
    message(FATAL_ERROR "Dependencies missing: run scripts/fetch-deps.sh (CI does this automatically)")
endif()

add_subdirectory(${TE_DIR}/modules/juce   ${CMAKE_BINARY_DIR}/juce)
add_subdirectory(${TE_DIR}/modules        ${CMAKE_BINARY_DIR}/tracktion)

juce_add_gui_app(TonkoriDAW
    PRODUCT_NAME                   "Tonkori DAW"
    COMPANY_NAME                   "Tonkori Recording Studio"
    BUNDLE_ID                      "studio.tonkori.daw"
    VERSION                        ${PROJECT_VERSION}
    MICROPHONE_PERMISSION_ENABLED  TRUE
    MICROPHONE_PERMISSION_TEXT     "Tonkori DAW needs audio input access to record."
    NEEDS_CURL                     FALSE
    NEEDS_WEB_BROWSER              FALSE
    PLUGINHOST_AU                  TRUE)

juce_generate_juce_header(TonkoriDAW)

target_sources(TonkoriDAW PRIVATE
    src/Main.cpp              # includes MainComponent.cpp (single TU)
    src/dsd/DsdDecoder.cpp)

target_include_directories(TonkoriDAW PRIVATE src src/vendor/tracktion_examples)

target_compile_definitions(TonkoriDAW PRIVATE
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1)

target_link_libraries(TonkoriDAW PRIVATE
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_recommended_config_flags)

set_target_properties(TonkoriDAW PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden)

if (APPLE)
    target_compile_options(TonkoriDAW PRIVATE "-fno-aligned-allocation")
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(TonkoriDAW PRIVATE "-latomic")
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(TonkoriDAW PRIVATE -Wno-unknown-pragmas)
endif()

#==============================================================================
# Headless engine self-test: null test + DSD accuracy. Runs in CI on macOS.
option(TONKORI_BUILD_TESTS "Build the engine self-test" ON)

if (TONKORI_BUILD_TESTS)
    juce_add_console_app(TonkoriSelfTest PRODUCT_NAME "TonkoriSelfTest")
    juce_generate_juce_header(TonkoriSelfTest)
    target_sources(TonkoriSelfTest PRIVATE tests/EngineSelfTest.cpp src/dsd/DsdDecoder.cpp)
    target_include_directories(TonkoriSelfTest PRIVATE src)
    target_compile_definitions(TonkoriSelfTest PRIVATE
        JUCE_PLUGINHOST_AU=0 JUCE_PLUGINHOST_VST3=0 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
        JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_STRICT_REFCOUNTEDPOINTER=1)
    target_link_libraries(TonkoriSelfTest PRIVATE
        tracktion::tracktion_core tracktion::tracktion_engine tracktion::tracktion_graph
        juce::juce_audio_devices juce::juce_audio_processors juce::juce_audio_utils
        juce::juce_recommended_config_flags)
    if (APPLE)
        target_compile_options(TonkoriSelfTest PRIVATE "-fno-aligned-allocation")
    endif()
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(TonkoriSelfTest PRIVATE "-latomic")
    endif()
endif()
