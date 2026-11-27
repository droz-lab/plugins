# Propre a ce plugin : fabrique Equipe.h a partir de equipe/*.txt (un fichier par membre, une ligne).
# Un fichier par personne : personne ne modifie la meme ligne, donc aucun conflit de fusion.
file(GLOB membres CONFIGURE_DEPENDS "${dir}/equipe/*.txt")
list(SORT membres)
set(lignes "")
foreach(fichier IN LISTS membres)
  file(STRINGS "${fichier}" contenu LIMIT_COUNT 1 ENCODING UTF-8)
  string(REGEX REPLACE "[\\\\\";]" "" contenu "${contenu}")
  string(STRIP "${contenu}" contenu)
  string(LENGTH "${contenu}" longueur)
  if(longueur GREATER 80)
    message(FATAL_ERROR "${fichier} : ligne trop longue (80 caracteres maximum)")
  endif()
  if(NOT contenu STREQUAL "")
    string(APPEND lignes "    \"${contenu}\",\n")
  endif()
endforeach()
set(sortie "${CMAKE_BINARY_DIR}/${cible}_gen")
file(WRITE "${sortie}/Equipe.h.tmp"
"// Fichier fabrique par plugin.cmake a partir de equipe/*.txt. Ne pas modifier.
#pragma once
namespace equipe
{
inline const char* const lignes[] = {
${lignes}    nullptr
};
}
")
file(COPY_FILE "${sortie}/Equipe.h.tmp" "${sortie}/Equipe.h" ONLY_IF_DIFFERENT)
target_include_directories(${cible} PRIVATE "${sortie}")
