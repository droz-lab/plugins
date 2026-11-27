# droz_add_plugin(<dossier>) : declare un plugin a partir de son plugin.json.
# Cree : <cible> (code commun), <cible>_VST3 (le plugin), <cible>_outil (tests, rendu WAV, capture).

function(_droz_json out meta cle defaut)
  string(JSON val ERROR_VARIABLE err GET "${meta}" "${cle}")
  if(err OR val STREQUAL "")
    set(val "${defaut}")
  endif()
  set(${out} "${val}" PARENT_SCOPE)
endfunction()

function(droz_add_plugin dir)
  file(READ "${dir}/plugin.json" meta)
  get_filename_component(dossier "${dir}" NAME)
  _droz_json(cible "${meta}" cible "")
  _droz_json(nom "${meta}" nom "")
  _droz_json(code "${meta}" code "")
  _droz_json(version "${meta}" version "0.1.0")
  _droz_json(type "${meta}" type "effet")
  _droz_json(fabricant "${meta}" fabricant "Droz")
  _droz_json(code_fabricant "${meta}" code_fabricant "Droz")
  _droz_json(description "${meta}" description "")
  if(cible STREQUAL "" OR nom STREQUAL "" OR code STREQUAL "")
    message(FATAL_ERROR "${dir}/plugin.json : 'cible', 'nom' et 'code' sont obligatoires")
  endif()

  if(type STREQUAL "instrument")
    set(synth TRUE)
    set(categories Instrument Synth)
  else()
    set(synth FALSE)
    set(categories Fx)
  endif()

  juce_add_plugin(${cible}
    PRODUCT_NAME "${nom}"
    VERSION "${version}"
    COMPANY_NAME "${fabricant}"
    PLUGIN_MANUFACTURER_CODE "${code_fabricant}"
    PLUGIN_CODE "${code}"
    DESCRIPTION "${description}"
    FORMATS ${DROZ_FORMATS}
    IS_SYNTH ${synth}
    NEEDS_MIDI_INPUT ${synth}
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES ${categories}
    COPY_PLUGIN_AFTER_BUILD ${DROZ_INSTALLER})

  file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${dir}/src/*.cpp" "${dir}/src/*.h")
  target_sources(${cible} PRIVATE ${sources})
  target_include_directories(${cible} PRIVATE "${dir}/src")
  target_compile_definitions(${cible} PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    DROZ_PLUGIN_DOSSIER="${dossier}")
  target_link_libraries(${cible}
    PRIVATE
      droz::dsp
      droz::ui
      juce::juce_audio_utils
      juce::juce_dsp
    PUBLIC
      juce::juce_recommended_config_flags
      juce::juce_recommended_warning_flags)
  set_target_properties(${cible} PROPERTIES FOLDER "plugins/${dossier}")

  # Reglages propres au plugin (ressources, fichiers fabriques) : plugins/<dossier>/plugin.cmake, facultatif.
  if(EXISTS "${dir}/plugin.cmake")
    include("${dir}/plugin.cmake")
  endif()

  # Outil sans DAW : memes sources que le plugin, lance en ligne de commande.
  add_executable(${cible}_outil "${CMAKE_SOURCE_DIR}/tools/outil/outil.cpp")
  target_link_libraries(${cible}_outil PRIVATE ${cible} droz::dsp)
  target_compile_definitions(${cible}_outil PRIVATE
    $<TARGET_PROPERTY:${cible},COMPILE_DEFINITIONS>)
  target_include_directories(${cible}_outil PRIVATE
    $<TARGET_PROPERTY:${cible},INCLUDE_DIRECTORIES>)
  set_target_properties(${cible}_outil PROPERTIES FOLDER "plugins/${dossier}")
  # Palier pro (version 1.0.0 et plus, docs/METHODE.md) : toute alerte des tests communs devient un echec.
  set(palier "")
  if(version VERSION_GREATER_EQUAL "1.0.0")
    set(palier "--pro")
  endif()
  add_test(NAME ${dossier}_tests COMMAND ${cible}_outil test ${palier})
endfunction()
