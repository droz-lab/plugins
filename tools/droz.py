#!/usr/bin/env python3
"""Outil du depot droz-lab. Python 3.9+, aucune dependance.

  python tools/droz.py nouveau <dossier> --nom "Mon Plugin" [--type effet|instrument] [--auteur fx]
  python tools/droz.py verifier            registre des plugins : codes uniques, champs presents
  python tools/droz.py liste               tableau des plugins
  python tools/droz.py modifies <base>     plugins touches depuis <base> (pour la compilation auto)
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parent.parent
PLUGINS = RACINE / "plugins"
MODELE = PLUGINS / "_modele"
# Un changement ici touche tous les plugins : on recompile tout.
COMMUN = ("CMakeLists.txt", "cmake/", "libs/", "tools/outil/", ".github/workflows/")


def metas() -> dict[str, dict]:
    out = {}
    for f in sorted(PLUGINS.glob("*/plugin.json")):
        out[f.parent.name] = json.loads(f.read_text(encoding="utf-8"))
    return out


def erreurs_registre() -> list[str]:
    err = []
    vus_code, vus_cible, vus_nom = {}, {}, {}
    for dossier, m in metas().items():
        for champ in ("nom", "cible", "code", "type", "version", "auteur"):
            if not str(m.get(champ, "")).strip():
                err.append(f"{dossier} : champ '{champ}' manquant dans plugin.json")
        code, cible = m.get("code", ""), m.get("cible", "")
        if not re.fullmatch(r"[A-Z][A-Za-z0-9]{3}", code):
            err.append(f"{dossier} : code '{code}' invalide (4 caracteres, lettres ou chiffres, majuscule en premier)")
        if not re.fullmatch(r"[A-Za-z][A-Za-z0-9]*", cible):
            err.append(f"{dossier} : cible '{cible}' invalide (lettres et chiffres, sans espace)")
        if m.get("type") not in ("effet", "instrument"):
            err.append(f"{dossier} : type '{m.get('type')}' invalide (effet ou instrument)")
        if not re.fullmatch(r"\d+\.\d+\.\d+", str(m.get("version", ""))):
            err.append(f"{dossier} : version '{m.get('version')}' invalide (ex. 0.1.0)")
        # Le DAW reconnait un plugin par (code fabricant, code) : un doublon melange deux plugins.
        cle = (m.get("code_fabricant", "Droz"), code)
        for vus, valeur, quoi in ((vus_code, cle, "code"), (vus_cible, cible.lower(), "cible"), (vus_nom, m.get("nom", "").lower(), "nom")):
            if valeur in vus:
                err.append(f"{dossier} : meme {quoi} que {vus[valeur]} ({valeur})")
            vus[valeur] = dossier
    return err


def code_libre(nom: str) -> str:
    pris = {m.get("code") for m in metas().values()}
    lettres = re.sub(r"[^A-Za-z0-9]", "", nom) or "Plug"
    base = (lettres[0].upper() + lettres[1:3].lower()).ljust(3, "x")
    for suffixe in "123456789abcdefghijklmnopqrstuvwxyz":
        if base + suffixe not in pris:
            return base + suffixe
    raise SystemExit("plus de code libre pour ce nom, en choisir un a la main avec --code")


def cmd_nouveau(a) -> int:
    if not re.fullmatch(r"[a-z0-9][a-z0-9-]*", a.dossier):
        raise SystemExit("dossier : minuscules, chiffres et tirets (ex. spiralift)")
    cible_dir = PLUGINS / a.dossier
    if cible_dir.exists():
        raise SystemExit(f"{cible_dir} existe deja")
    cible = "".join(p.capitalize() for p in re.split(r"[^A-Za-z0-9]+", a.nom) if p)
    shutil.copytree(MODELE, cible_dir)
    meta = {
        "nom": a.nom,
        "cible": cible,
        "code": a.code or code_libre(a.nom),
        "type": a.type,
        "version": "0.1.0",
        "auteur": a.auteur,
        "description": a.description,
    }
    (cible_dir / "plugin.json").write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    (cible_dir / "NOTES.md").write_text(
        f"# {a.nom}\n\nResponsable : {a.auteur}\n\n## But\n\n{a.description or 'A ecrire : le son voulu, en deux phrases.'}\n\n"
        "## Etat\n\n- v0.1.0 : copie du modele (gain).\n\n## Decisions et lecons\n\n", encoding="utf-8")
    editeur = cible_dir / "src" / "PluginEditor.cpp"
    editeur.write_text(editeur.read_text(encoding="utf-8").replace("Droz Modèle", a.nom), encoding="utf-8")
    err = erreurs_registre()
    if err:
        shutil.rmtree(cible_dir)
        raise SystemExit("\n".join(err))
    print(f"cree : plugins/{a.dossier} (cible {cible}, code {meta['code']})")
    print(f"compiler : cmake --build build --target {cible}_VST3 {cible}_outil")
    return 0


def cmd_verifier(_) -> int:
    err = erreurs_registre()
    for e in err:
        print("ERREUR", e)
    if not err:
        print(f"registre : {len(metas())} plugin(s), aucun conflit")
    return 1 if err else 0


def cmd_liste(_) -> int:
    print("| Dossier | Nom | Type | Version | Code | Responsable |")
    print("|---|---|---|---|---|---|")
    for dossier, m in metas().items():
        print(f"| {dossier} | {m.get('nom')} | {m.get('type')} | {m.get('version')} | {m.get('code')} | {m.get('auteur')} |")
    return 0


def cmd_modifies(a) -> int:
    tous = sorted(metas())
    try:
        diff = subprocess.run(["git", "diff", "--name-only", f"{a.base}...HEAD"], cwd=RACINE,
                              capture_output=True, text=True, check=True).stdout.split()
    except subprocess.CalledProcessError:
        diff = None  # base introuvable : dans le doute, tout
    if diff is None or any(f.startswith(COMMUN) for f in diff):
        choisis = tous
    else:
        touches = {f.split("/")[1] for f in diff if f.startswith("plugins/") and f.count("/") >= 2}
        choisis = [d for d in tous if d in touches]
    print(json.dumps(choisis))
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="commande", required=True)
    n = sub.add_parser("nouveau")
    n.add_argument("dossier")
    n.add_argument("--nom", required=True)
    n.add_argument("--type", choices=("effet", "instrument"), default="effet")
    n.add_argument("--auteur", default="equipe")
    n.add_argument("--description", default="")
    n.add_argument("--code", default="")
    n.set_defaults(f=cmd_nouveau)
    sub.add_parser("verifier").set_defaults(f=cmd_verifier)
    sub.add_parser("liste").set_defaults(f=cmd_liste)
    m = sub.add_parser("modifies")
    m.add_argument("base")
    m.set_defaults(f=cmd_modifies)
    a = p.parse_args()
    return a.f(a)


if __name__ == "__main__":
    sys.exit(main())
