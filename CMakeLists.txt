cmake_minimum_required(VERSION 3.22)

project(ADRIEN_DO_IT_BETTER VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS : binaire universel (Apple Silicon + Intel), macOS 11 minimum
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    endif()
endif()

# JUCE : soit un dossier local (-DJUCE_DIR=/chemin/vers/JUCE), soit téléchargé automatiquement
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Formats : VST3 par défaut (ajoute AU ou Standalone avec -DADRIEN_PLUGIN_FORMATS="VST3;AU")
if(NOT ADRIEN_PLUGIN_FORMATS)
    set(ADRIEN_PLUGIN_FORMATS VST3)
endif()

juce_add_plugin(AdrienDoItBetter
    COMPANY_NAME "Julien Herve"
    COMPANY_CODE "JHrv"
    PLUGIN_MANUFACTURER_CODE "JHrv"
    PLUGIN_CODE "AdDB"
    PRODUCT_NAME "ADRIEN DO IT BETTER"
    BUNDLE_ID "com.julienherve.adriendoitbetter"
    FORMATS ${ADRIEN_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Fx EQ Distortion)

juce_generate_juce_header(AdrienDoItBetter)

juce_add_binary_data(AdrienAssets SOURCES
    Assets/adrien_body.png
    Assets/adrien_arm.png
    Assets/adrien_bald.png)

target_sources(AdrienDoItBetter PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/AdrienView.cpp
    Source/EqScreen.cpp)

target_compile_definitions(AdrienDoItBetter PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(AdrienDoItBetter
    PRIVATE
        AdrienAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
