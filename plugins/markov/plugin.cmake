# Reglages propres a Markov (inclus par cmake/DrozPlugin.cmake, variables cible / dir / dossier).

# Generateur MIDI : le VST3 doit declarer une sortie MIDI (le modele commun la laisse a FALSE).
# JUCE lit cette propriete au moment de generer : la changer ici suffit.
set_target_properties(${cible} PROPERTIES JUCE_NEEDS_MIDI_OUTPUT TRUE)

# Tests du moteur, en C++ pur (sans JUCE) : rapides, lances par ctest.
add_executable(${cible}_moteur_tests "${dir}/tests/engine_tests.cpp")
target_include_directories(${cible}_moteur_tests PRIVATE "${dir}/src")
target_compile_features(${cible}_moteur_tests PRIVATE cxx_std_20)
set_target_properties(${cible}_moteur_tests PROPERTIES FOLDER "plugins/${dossier}")
add_test(NAME ${dossier}_moteur COMMAND ${cible}_moteur_tests)

# Tests du plugin entier (synchro hote, note-off, reproductibilite), avec JUCE.
add_executable(${cible}_plugin_tests "${dir}/tests/plugin_tests.cpp")
target_link_libraries(${cible}_plugin_tests PRIVATE ${cible} droz::dsp)
target_compile_definitions(${cible}_plugin_tests PRIVATE $<TARGET_PROPERTY:${cible},COMPILE_DEFINITIONS>)
target_include_directories(${cible}_plugin_tests PRIVATE $<TARGET_PROPERTY:${cible},INCLUDE_DIRECTORIES>)
set_target_properties(${cible}_plugin_tests PROPERTIES FOLDER "plugins/${dossier}")
add_test(NAME ${dossier}_plugin COMMAND ${cible}_plugin_tests)
