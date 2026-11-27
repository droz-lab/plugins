cmake_minimum_required(VERSION 3.25)

# Mac : binaire universel (Apple Silicon + Intel), a partir de macOS 11.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
# Windows : runtime C++ lie en statique, le plugin ne demande aucun redistribuable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(DrozLab VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
if(MSVC)
  add_compile_options(/utf-8) # sources en UTF-8 : les accents passent partout
endif()

option(DROZ_BUILD_PLUGINS "Compiler les plugins (OFF = seulement libs/ et ses tests, tres rapide)" ON)
option(DROZ_INSTALLER "Copier chaque plugin dans le dossier VST3 du systeme apres compilation" OFF)
set(DROZ_PLUGINS "" CACHE STRING "Dossiers de plugins a compiler, separes par ';'. Vide = tous")
set(DROZ_FORMATS "VST3" CACHE STRING "Formats JUCE a produire (VST3;AU;Standalone)")

enable_testing()
add_subdirectory(libs/droz_dsp)

if(DROZ_BUILD_PLUGINS)
  include(FetchContent)
  # JUCE pris a une version precise (regle commune : code tiers fige).
  # Pour reutiliser un JUCE deja telecharge : -DFETCHCONTENT_SOURCE_DIR_JUCE=/chemin/vers/JUCE
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.6
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)

  include(cmake/DrozPlugin.cmake)
  file(GLOB droz_metas CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/plugins/*/plugin.json")
  foreach(meta IN LISTS droz_metas)
    get_filename_component(dir "${meta}" DIRECTORY)
    get_filename_component(dossier "${dir}" NAME)
    if(DROZ_PLUGINS STREQUAL "" OR dossier IN_LIST DROZ_PLUGINS)
      droz_add_plugin("${dir}")
    endif()
  endforeach()
endif()
