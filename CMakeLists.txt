cmake_minimum_required(VERSION 3.22)

# macOS universal binaries
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
    endif()
endif()

project(Soundstave VERSION 0.2.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# -----------------------------------------------------------------------
# JUCE
# -----------------------------------------------------------------------
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE submodule missing. Run:\n"
        "  git submodule update --init --recursive\n"
        "or copy/clone JUCE into libs/JUCE/")
endif()

add_subdirectory(libs/JUCE EXCLUDE_FROM_ALL)

# -----------------------------------------------------------------------
# Source files
# -----------------------------------------------------------------------
set(SOUNDSTAVE_SOURCES
    # Application
    src/Main.cpp
    src/MainWindow.cpp
    src/MainWindow.h

    # Audio engine
    src/audio/AudioEngine.cpp
    src/audio/AudioEngine.h

    # Spectral analysis
    src/analysis/SpectralAnalyzer.cpp
    src/analysis/SpectralAnalyzer.h
    src/analysis/PitchDetector.cpp
    src/analysis/PitchDetector.h
    src/analysis/TempoEstimator.cpp
    src/analysis/TempoEstimator.h

    # Note model
    src/model/NoteModel.cpp
    src/model/NoteModel.h
    src/model/NoteEditCommands.cpp
    src/model/NoteEditCommands.h

    # UI components
    src/ui/MainComponent.cpp
    src/ui/MainComponent.h
    src/ui/WaveformDisplay.cpp
    src/ui/WaveformDisplay.h
    src/ui/SpectrogramDisplay.cpp
    src/ui/SpectrogramDisplay.h
    src/ui/NoteOverlay.cpp
    src/ui/NoteOverlay.h
    src/ui/PianoRoll.cpp
    src/ui/PianoRoll.h
    src/ui/PianoKeyboard.cpp
    src/ui/PianoKeyboard.h
    src/ui/Toolbar.cpp
    src/ui/Toolbar.h
    src/ui/TransportBar.cpp
    src/ui/TransportBar.h
    src/ui/TimelineRuler.cpp
    src/ui/TimelineRuler.h
    src/ui/LookAndFeel.cpp
    src/ui/LookAndFeel.h
    src/ui/AnalysisProgressOverlay.cpp
    src/ui/AnalysisProgressOverlay.h
    src/ui/SettingsDialog.cpp
    src/ui/SettingsDialog.h
    src/ui/ExportDialog.cpp
    src/ui/ExportDialog.h

    # Export
    src/export/MidiExporter.cpp
    src/export/MidiExporter.h
    src/export/MusicXmlExporter.cpp
    src/export/MusicXmlExporter.h
    src/export/QuantizationEngine.cpp
    src/export/QuantizationEngine.h

    # Project
    src/project/ProjectManager.cpp
    src/project/ProjectManager.h
)

# -----------------------------------------------------------------------
# Standalone application target
# -----------------------------------------------------------------------
juce_add_gui_app(Soundstave
    VERSION             "${PROJECT_VERSION}"
    PRODUCT_NAME        "Soundstave"
    COMPANY_NAME        "Soundstave Contributors"
    BUNDLE_ID           "org.soundstave.app"
    ICON_BIG            "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_512.png"
    ICON_SMALL          "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_512.png"
    NEEDS_CURL          FALSE
    NEEDS_WEB_BROWSER   FALSE
)

target_sources(Soundstave PRIVATE ${SOUNDSTAVE_SOURCES})

target_compile_definitions(Soundstave PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Soundstave,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Soundstave,JUCE_VERSION>"
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)

target_include_directories(Soundstave PRIVATE src)

# Sources contain UTF-8 text (Danish UI); MSVC must not read them as ANSI
if(MSVC)
    target_compile_options(Soundstave PRIVATE /utf-8)
endif()

target_link_libraries(Soundstave PRIVATE
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------
# Headless tests: synthetic signals through analysis + export
#   cmake --build build --target SoundstaveTests && ./build/SoundstaveTests_artefacts/<cfg>/SoundstaveTests
# -----------------------------------------------------------------------
option(SOUNDSTAVE_BUILD_TESTS "Build the SoundstaveTests console app" ON)
if(SOUNDSTAVE_BUILD_TESTS)
    juce_add_console_app(SoundstaveTests PRODUCT_NAME "SoundstaveTests")
    target_sources(SoundstaveTests PRIVATE
        tests/AnalysisTests.cpp
        src/analysis/SpectralAnalyzer.cpp
        src/analysis/PitchDetector.cpp
        src/analysis/TempoEstimator.cpp
        src/model/NoteModel.cpp
        src/export/QuantizationEngine.cpp
        src/export/MusicXmlExporter.cpp
        src/export/MidiExporter.cpp
        src/audio/AudioEngine.cpp)
    target_include_directories(SoundstaveTests PRIVATE src)
    if(MSVC)
        target_compile_options(SoundstaveTests PRIVATE /utf-8)
    endif()
    target_compile_definitions(SoundstaveTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SoundstaveTests PRIVATE
        juce::juce_core juce::juce_events juce::juce_data_structures
        juce::juce_audio_basics juce::juce_audio_formats juce::juce_audio_devices juce::juce_dsp
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
