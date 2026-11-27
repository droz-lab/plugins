# Habillage commun des interfaces (couleurs, polices, controles, en-tete). Depend de JUCE : en-tetes seulement.
# Regle complete : docs/HABILLAGE.md.

# Polices embarquees (SIL OFL, licences a cote des fichiers). Chemins absolus : juce_add_binary_data l'exige.
juce_add_binary_data(droz_ui_polices
  NAMESPACE DrozPolices
  HEADER_NAME DrozPolices.h
  SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/polices/Michroma-Regular.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/polices/IBMPlexMono-Regular.ttf")
set_target_properties(droz_ui_polices PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER "libs")

add_library(droz_ui INTERFACE)
add_library(droz::ui ALIAS droz_ui)
target_include_directories(droz_ui INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(droz_ui INTERFACE droz_ui_polices)

# Planche de tous les controles, pour regler l'habillage sans ouvrir de DAW :
#   cmake -S . -B build -DDROZ_PLANCHE=ON && cmake --build build --target droz_ui_planche
#   xvfb-run -a build/.../droz_ui_planche docs/habillage.png     (xvfb-run : Linux seulement)
option(DROZ_PLANCHE "Compiler la planche de l'habillage (droz_ui_planche)" OFF)
if(DROZ_PLANCHE)
  juce_add_console_app(droz_ui_planche PRODUCT_NAME "droz_ui_planche")
  target_sources(droz_ui_planche PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/planche/planche.cpp")
  target_compile_definitions(droz_ui_planche PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(droz_ui_planche PRIVATE droz::ui juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags)
  set_target_properties(droz_ui_planche PROPERTIES FOLDER "libs")
endif()
