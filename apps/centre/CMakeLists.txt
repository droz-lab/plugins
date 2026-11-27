# Droz Software Center : l'appli Windows qui installe, met a jour et desinstalle les plugins Droz.
# Spec : apps/centre/NOTES.md. Compilee seulement a la demande : -DDROZ_CENTRE=ON.
# Linux : elle compile aussi, pour les captures de l'interface et l'essai sans fenetre (pas de reseau https).

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/app.json" centre_meta)
string(JSON centre_version GET "${centre_meta}" version)

juce_add_gui_app(DrozSoftwareCenter
  PRODUCT_NAME "DrozSoftwareCenter"
  VERSION "${centre_version}"
  COMPANY_NAME "Droz")

target_sources(DrozSoftwareCenter PRIVATE
  src/Main.cpp
  src/Moteur.cpp
  src/Systeme.cpp
  src/Essai.cpp)
target_include_directories(DrozSoftwareCenter PRIVATE src)
target_compile_definitions(DrozSoftwareCenter PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0
  JUCE_APPLICATION_NAME_STRING="Droz Software Center"
  JUCE_APPLICATION_VERSION_STRING="${centre_version}"
  DROZ_CENTRE_VERSION="${centre_version}")
target_link_libraries(DrozSoftwareCenter
  PRIVATE
    droz::ui
    juce::juce_gui_basics
    juce::juce_data_structures
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
if(WIN32)
  target_link_libraries(DrozSoftwareCenter PRIVATE Rstrtmgr)
endif()
set_target_properties(DrozSoftwareCenter PROPERTIES FOLDER "apps")
