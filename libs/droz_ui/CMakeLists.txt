# Habillage commun des interfaces (couleurs, potards, en-tete). Depend de JUCE : en-tetes seulement.
add_library(droz_ui INTERFACE)
add_library(droz::ui ALIAS droz_ui)
target_include_directories(droz_ui INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
