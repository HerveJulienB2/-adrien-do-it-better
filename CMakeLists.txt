cmake_minimum_required(VERSION 3.22)

project(ADRIEN_DO_IT_BETTER_V2 VERSION 2.1.0)

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

# Formats : VST3 par défaut. Pro Tools : -DADRIEN_PLUGIN_FORMATS="VST3;AAX" (le SDK AAX est
# fourni avec JUCE 8). Pour s'ouvrir dans Pro Tools, l'AAX doit ensuite être signé avec PACE.
if(NOT ADRIEN_PLUGIN_FORMATS)
    set(ADRIEN_PLUGIN_FORMATS VST3)
endif()

# copie dans le dossier des plugins après la compilation (désactivée en CI : -DADRIEN_COPY_AFTER_BUILD=OFF)
if(NOT DEFINED ADRIEN_COPY_AFTER_BUILD)
    set(ADRIEN_COPY_AFTER_BUILD TRUE)
endif()

# V2 : un nouveau plugin (nouveaux codes), signé Méchant Humain.
juce_add_plugin(AdrienDoItBetter
    COMPANY_NAME "Méchant Humain"
    COMPANY_CODE "MHmn"
    PLUGIN_MANUFACTURER_CODE "MHmn"
    PLUGIN_CODE "AdB2"
    PRODUCT_NAME "ADRIEN DO IT BETTER V2"
    BUNDLE_ID "com.mechanthumain.adriendoitbetter2"
    AAX_CATEGORY EQ Harmonic
    FORMATS ${ADRIEN_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${ADRIEN_COPY_AFTER_BUILD}
    VST3_CATEGORIES Fx EQ Distortion)

juce_generate_juce_header(AdrienDoItBetter)

juce_add_binary_data(AdrienAssets SOURCES
    Assets/adrien_body.png
    Assets/adrien_arm.png)

target_sources(AdrienDoItBetter PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/AdrienView.cpp)

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

# Tests (Linux/macOS, sans DAW) : -DADRIEN_BUILD_TESTS=ON
if(ADRIEN_BUILD_TESTS)
    juce_add_console_app(AdrienTests PRODUCT_NAME "AdrienTests")
    target_sources(AdrienTests PRIVATE
        Tests/Tests.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/AdrienView.cpp)
    target_include_directories(AdrienTests PRIVATE Source)
    target_compile_definitions(AdrienTests PRIVATE
        JucePlugin_Name="ADRIEN DO IT BETTER V2"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(AdrienTests PRIVATE
        AdrienAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    juce_generate_juce_header(AdrienTests)
endif()
