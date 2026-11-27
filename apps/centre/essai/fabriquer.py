#!/usr/bin/env python3
"""Fabrique de quoi essayer le Droz Software Center sans rien publier. Python 3.9+, aucune dependance.

  python apps/centre/essai/fabriquer.py <sortie> --v1 <Setup.exe> --v2 <Setup.exe> [--nom "Droz Modele"]
      Ecrit <sortie>/a.json (version 0.0.1) et <sortie>/b.json (version 0.0.2), deux catalogues qui pointent
      vers deux vrais installeurs. C'est ce que lance le controle Verdict sous Windows avec --essai.

  python apps/centre/essai/fabriquer.py <sortie> --faux
      Sans Windows : memes catalogues avec de faux installeurs (scripts qui posent un paquet vide),
      plus un catalogue d'exemple et un dossier VST3 d'exemple pour les captures de l'interface.

Ensuite :
  DrozSoftwareCenter --essai --catalogue <sortie>/a.json --suite <sortie>/b.json --dossier <sortie>/vst3 --donnees <sortie>/donnees
  DrozSoftwareCenter --capture <sortie>/captures --catalogue <sortie>/exemple.json --dossier <sortie>/vst3-exemple
"""
from __future__ import annotations

import argparse
import json
import os
import stat
from pathlib import Path

FAUX = """#!/bin/sh
# Faux installeur d'essai : pose un paquet vide dans le dossier donne par /DIR=, comme tools/installeur.iss.
for a in "$@"; do case "$a" in /DIR=*) dir="${a#/DIR=}" ;; esac; done
[ -n "$dir" ] || exit 3
paquet="$dir/@NOM@.vst3"
mkdir -p "$paquet/Contents/Resources" "$paquet/Contents/x86_64-linux" || exit 4
: > "$paquet/Contents/x86_64-linux/@NOM@.so"
printf '[droz]\\nversion=@VERSION@\\nbuild=@BUILD@\\n' > "$paquet/Contents/Resources/droz.ini"
"""


def catalogue(plugins: list[dict], atelier: dict | None = None, centre: str = "0.1.0") -> dict:
    return {"format": 1, "genere": "essai", "centre": {"version": centre, "installeur": "https://github.com/droz-lab/plugins/releases/tag/dernier"},
            "plugins": plugins, "atelier": atelier or {"pr": [], "fiches_a_ecrire": [], "message": "", "liens": []}}


def plugin(nom: str, version: str, build: str, installeur: str, **plus) -> dict:
    base = {"dossier": nom.lower().replace("droz ", "").replace(" ", "-"), "nom": nom, "auteur": "equipe",
            "description": "Plugin d'essai du Droz Software Center.", "version": version, "build": build, "etat": "publie",
            "installeur": installeur, "page": ""}
    base.update(plus)
    return base


def ecrire(chemin: Path, contenu: dict) -> None:
    chemin.write_text(json.dumps(contenu, indent=1, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")


def faux_installeur(chemin: Path, nom: str, version: str, build: str) -> str:
    chemin.parent.mkdir(parents=True, exist_ok=True)
    chemin.write_text(FAUX.replace("@NOM@", nom).replace("@VERSION@", version).replace("@BUILD@", build), encoding="utf-8", newline="\n")
    chemin.chmod(chemin.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    return str(chemin.resolve())


def paquet_exemple(dossier: Path, nom: str, version: str, build: str = "") -> None:
    ressources = dossier / f"{nom}.vst3" / "Contents" / "Resources"
    ressources.mkdir(parents=True, exist_ok=True)
    (ressources / "droz.ini").write_text(f"[droz]\nversion={version}\nbuild={build}\n", encoding="utf-8", newline="\n")


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("sortie")
    p.add_argument("--v1", default="")
    p.add_argument("--v2", default="")
    p.add_argument("--nom", default="Droz Modele")
    p.add_argument("--faux", action="store_true")
    a = p.parse_args()
    sortie = Path(a.sortie)
    sortie.mkdir(parents=True, exist_ok=True)

    v1, v2 = a.v1, a.v2
    if a.faux and os.name != "nt":
        v1 = faux_installeur(sortie / "v1" / "installeur.sh", a.nom, "0.0.1", "essai01")
        v2 = faux_installeur(sortie / "v2" / "installeur.sh", a.nom, "0.0.2", "essai02")
    if v1 and v2:
        ecrire(sortie / "a.json", catalogue([plugin(a.nom, "0.0.1", "essai01", str(Path(v1).resolve()))]))
        ecrire(sortie / "b.json", catalogue([plugin(a.nom, "0.0.2", "essai02", str(Path(v2).resolve()))]))
        print(f"ecrit : {sortie / 'a.json'} et {sortie / 'b.json'}")
    elif not a.faux:
        p.error("donner --v1 et --v2, ou --faux")

    if a.faux:
        # Catalogue et dossier VST3 d'exemple : tous les etats d'une ligne sur une seule capture.
        depot = "https://github.com/droz-lab/plugins"
        exemple = catalogue(
            [plugin("Droz Exemple Un", "1.2.0", "", "absent.exe", auteur="Fx", description="Exemple : plugin publié, installé et à jour."),
             plugin("Droz Exemple Deux", "0.4.0", "", "absent.exe", auteur="Zomban", description="Exemple : plugin publié, une version plus récente existe."),
             plugin("Droz Exemple Trois", "0.2.0", "", "absent.exe", auteur="YoY", description="Exemple : plugin publié, pas encore installé."),
             plugin("Droz Exemple Quatre", "0.1.0", "a1b2c3d", "absent.exe", auteur="YoY", etat="essai", description="Exemple : chantier à l'essai, installé, build plus récent."),
             plugin("Droz Exemple Cinq", "0.1.0", "e4f5a6b", "absent.exe", auteur="Zomban", etat="essai", description="Exemple : chantier à l'essai, pas encore installé.")],
            {"pr": [{"numero": 21, "titre": "exemple : nouveau plugin", "auteur": "membre-un", "verdict": "vert", "brouillon": False, "url": f"{depot}/pulls"},
                    {"numero": 22, "titre": "exemple : correction d'un clic", "auteur": "membre-deux", "verdict": "rouge", "brouillon": True, "url": f"{depot}/pulls"}],
             "fiches_a_ecrire": ["Droz Exemple Cinq"],
             "message": "Message d'exemple. Je suis <fx | yoy | zomban>.",
             "liens": [{"nom": "Méthode", "url": f"{depot}/blob/main/docs/METHODE.md"},
                       {"nom": "Habillage", "url": f"{depot}/blob/main/docs/HABILLAGE.md"},
                       {"nom": "Pièges", "url": f"{depot}/blob/main/docs/PIEGES.md"},
                       {"nom": "Idée de plugin", "url": f"{depot}/issues/new?template=idee-de-plugin.yml"},
                       {"nom": "Page de l'atelier", "url": f"{depot}/releases/tag/dernier"}]},
            centre="9.9.9")
        ecrire(sortie / "exemple.json", exemple)
        vst3 = sortie / "vst3-exemple"
        paquet_exemple(vst3, "Droz Exemple Un", "1.2.0")
        paquet_exemple(vst3, "Droz Exemple Deux", "0.3.1")
        paquet_exemple(vst3, "Droz Exemple Quatre", "0.1.0", "0000000")
        paquet_exemple(vst3, "Droz Ancien Plugin", "0.1.4")
        print(f"ecrit : {sortie / 'exemple.json'} et {vst3}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
