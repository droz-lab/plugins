#!/bin/bash
# Installe sur Mac la derniere version publiee d'un plugin du depot droz-lab/plugins.
#   curl -fsSL https://raw.githubusercontent.com/droz-lab/plugins/main/tools/installer.sh | bash
#   ... | bash -s -- <dossier>        (par defaut : premier-jet)
set -euo pipefail
plugin="${1:-premier-jet}"
dest="$HOME/Library/Audio/Plug-Ins/VST3"
url=$(curl -fsSL "https://api.github.com/repos/droz-lab/plugins/releases?per_page=100" \
  | grep -o "https://[^\"]*/releases/download/${plugin}-v[^/\"]*/[^\"]*-Mac\.zip" | head -n 1 || true)
if [ -z "$url" ]; then
  echo "Aucune version publiee pour '$plugin'. Voir https://github.com/droz-lab/plugins/releases"
  exit 1
fi
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
echo "Telechargement : $url"
curl -fsSL -o "$tmp/plugin.zip" "$url"
mkdir -p "$tmp/contenu" "$dest"
ditto -x -k "$tmp/plugin.zip" "$tmp/contenu"
for vst in "$tmp/contenu"/*.vst3; do
  nom=$(basename "$vst")
  rm -rf "$dest/$nom"
  ditto "$vst" "$dest/$nom"
  xattr -dr com.apple.quarantine "$dest/$nom" 2>/dev/null || true
  echo "Installe : $dest/$nom"
done
echo "Ferme puis rouvre ton DAW, ou relance le scan des plugins."
