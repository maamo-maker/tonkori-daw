cmake_minimum_required(VERSION 3.22)

# macOS 13 (Ventura) is the oldest system this app must run on.
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")

project(TonkoriDAW VERSION 0.4.0 LANGUAGES C CXX)

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
    ICON_BIG                       "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png"
    VERSION                        ${PROJECT_VERSION}
    MICROPHONE_PERMISSION_ENABLED  TRUE
    MICROPHONE_PERMISSION_TEXT     "Tonkori DAW needs audio input access to record."
    NEEDS_CURL                     FALSE
    NEEDS_WEB_BROWSER              FALSE
    PLUGINHOST_AU                  TRUE)

juce_generate_juce_header(TonkoriDAW)

set(TONKORI_ENGINE_SOURCES
    src/dsd/DsdDecoder.cpp
    src/model/FadeSpec.cpp
    src/engine/Loudness.cpp
    src/video/Video.cpp)

# Static FFmpeg (scripts/build-ffmpeg.sh), Rubber Band built from source by Tracktion.
set(FFMPEG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/ffmpeg-install)
if (NOT EXISTS ${FFMPEG_DIR}/lib/libavformat.a)
    message(FATAL_ERROR "FFmpeg missing: run scripts/build-ffmpeg.sh (CI does this automatically)")
endif()

function(tonkori_configure target)
    target_include_directories(${target} PRIVATE src ${FFMPEG_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/libs)
    target_compile_definitions(${target} PRIVATE
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1
        TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH=1
        TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND=1
        TRACKTION_BUILD_RUBBERBAND=1)
    target_link_libraries(${target} PRIVATE
        tracktion::tracktion_core
        tracktion::tracktion_engine
        tracktion::tracktion_graph
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        ${FFMPEG_DIR}/lib/libavformat.a
        ${FFMPEG_DIR}/lib/libavcodec.a
        ${FFMPEG_DIR}/lib/libswscale.a
        ${FFMPEG_DIR}/lib/libswresample.a
        ${FFMPEG_DIR}/lib/libavutil.a
        z)
    if (APPLE)
        target_compile_options(${target} PRIVATE "-fno-aligned-allocation")
        target_link_libraries(${target} PRIVATE
            "-framework VideoToolbox" "-framework CoreMedia" "-framework CoreVideo"
            "-framework CoreFoundation" "-framework CoreServices" "-framework AudioToolbox"
            "-framework Security" "-framework Accelerate")
    endif()
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(${target} PRIVATE "-latomic" m pthread)
    endif()
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_compile_options(${target} PRIVATE -Wno-unknown-pragmas)
    endif()
endfunction()

target_sources(TonkoriDAW PRIVATE
    src/Main.cpp              # includes MainComponent.cpp (single TU)
    ${TONKORI_ENGINE_SOURCES}
    src/ui/ArrangeView.cpp
    src/ui/Inspector.cpp
    src/ui/Mixer.cpp)

tonkori_configure(TonkoriDAW)
target_include_directories(TonkoriDAW PRIVATE src/vendor/tracktion_examples)
target_compile_definitions(TonkoriDAW PRIVATE JUCE_PLUGINHOST_AU=1 JUCE_PLUGINHOST_VST3=1)

set_target_properties(TonkoriDAW PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden)

#==============================================================================
# Headless engine self-test. Runs in CI on macOS; the release only ships if it passes.
option(TONKORI_BUILD_TESTS "Build the engine self-test" ON)

if (TONKORI_BUILD_TESTS)
    juce_add_console_app(TonkoriSelfTest PRODUCT_NAME "TonkoriSelfTest")
    juce_generate_juce_header(TonkoriSelfTest)
    target_sources(TonkoriSelfTest PRIVATE tests/EngineSelfTest.cpp ${TONKORI_ENGINE_SOURCES})
    tonkori_configure(TonkoriSelfTest)
    target_compile_definitions(TonkoriSelfTest PRIVATE JUCE_PLUGINHOST_AU=0 JUCE_PLUGINHOST_VST3=0)
endif()
