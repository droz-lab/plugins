#!/usr/bin/env python3
"""Outil du depot droz-lab. Python 3.9+, aucune dependance.

  python tools/droz.py nouveau <dossier> --nom "Mon Plugin" [--type effet|instrument] [--auteur fx]
  python tools/droz.py verifier            registre des plugins : codes uniques, champs presents
  python tools/droz.py liste               tableau des plugins
  python tools/droz.py vitrine             refait le tableau des plugins du README (publies et en chantier)
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
    # Non bloquant : un oubli ne doit pas faire rougir la PR de quelqu'un d'autre.
    for e in ecarts_habillage():
        print("A FAIRE habillage (docs/HABILLAGE.md) :", e)
    return 1 if err else 0


def cmd_liste(_) -> int:
    print("| Dossier | Nom | Type | Version | Code | Responsable |")
    print("|---|---|---|---|---|---|")
    for dossier, m in metas().items():
        print(f"| {dossier} | {m.get('nom')} | {m.get('type')} | {m.get('version')} | {m.get('code')} | {m.get('auteur')} |")
    return 0


# Signes d'un habillage fait a la main : couleur ou police ecrite en dur, ou editeur sans droz::ui.
HORS_CHARTE = re.compile(r"juce::Colour\s*\(\s*0x|juce::Colours::(?!transparent)|juce::FontOptions\s*\(")


def ecarts_habillage() -> list[str]:
    out = []
    for dossier in metas():
        sources = [f for f in sorted((PLUGINS / dossier / "src").glob("**/*")) if f.suffix in (".cpp", ".h")]
        textes = {f: f.read_text(encoding="utf-8", errors="replace") for f in sources}
        editeurs = [f for f in sources if "Editor" in f.name]
        if editeurs and not any("droz/ui.h" in textes[f] for f in editeurs):
            out.append(f"{dossier} : l'editeur n'utilise pas droz::ui")
        for f, texte in textes.items():
            n = len(HORS_CHARTE.findall(texte))
            if n:
                out.append(f"{dossier} : {n} couleur(s) ou police(s) en dur dans {f.name}")
    return out


README = RACINE / "README.md"
DEBUT, FIN = "<!-- vitrine:debut -->", "<!-- vitrine:fin -->"
DEPOT = "https://github.com/droz-lab/plugins"


def _git(*args: str) -> str:
    r = subprocess.run(["git", *args], cwd=RACINE, capture_output=True, text=True)
    return r.stdout if r.returncode == 0 else ""


def plugins_en_chantier() -> dict[str, tuple[str, dict]]:
    """Plugins presents sur une branche du depot mais pas encore dans main : dossier -> (branche, plugin.json)."""
    _git("fetch", "--quiet", "--depth", "1", "origin", "+refs/heads/*:refs/remotes/origin/*")
    dans_main = set(metas())
    cibles_main = {str(m.get("cible")) for m in metas().values()}  # un plugin renomme garde sa cible
    trouves: dict[str, tuple[str, dict]] = {}
    for ref in sorted(_git("for-each-ref", "--format=%(refname:short)", "refs/remotes/origin").split()):
        branche = ref.removeprefix("origin/")
        if branche in ("HEAD", "main") or ref == "origin":
            continue
        for fichier in _git("ls-tree", "-r", "--name-only", ref, "--", "plugins").split("\n"):
            morceaux = fichier.split("/")
            if len(morceaux) != 3 or morceaux[2] != "plugin.json":
                continue
            dossier = morceaux[1]
            if dossier.startswith("_") or dossier in dans_main:
                continue
            try:
                meta = json.loads(_git("show", f"{ref}:{fichier}"))
            except json.JSONDecodeError:
                continue
            if str(meta.get("cible")) in cibles_main:
                continue
            # La branche au nom d'une personne passe avant une branche de travail automatique.
            if dossier not in trouves or (trouves[dossier][0].startswith("claude/") and not branche.startswith("claude/")):
                trouves[dossier] = (branche, meta)
    return trouves


def etiquettes() -> set[str]:
    distantes = {l.split("refs/tags/")[-1] for l in _git("ls-remote", "--tags", "origin").splitlines() if "refs/tags/" in l}
    return distantes or set(_git("tag").split())


def _ligne(nom: str, lien: str, meta: dict, etat: str, telecharger: str) -> str:
    description = re.sub(r"\s*\(migre[^)]*\)", "", str(meta.get("description", ""))).replace("|", "/").strip() or "-"
    auteur = {"yoy": "YoY", "zomban": "Zomban", "fx": "Fx", "equipe": "equipe"}.get(str(meta.get("auteur", "-")).lower(), meta.get("auteur", "-"))
    return f"| [{nom}]({lien}) | {description} | {auteur} | {etat} | {telecharger} |"


def tableau_vitrine() -> str:
    lignes = ["| Plugin | Ce qu'il fait | Par | Etat | Telecharger |", "|---|---|---|---|---|"]
    publies = etiquettes()
    for dossier, m in metas().items():
        if dossier.startswith("_"):
            continue
        nom = str(m.get("nom"))
        # Deja publie au moins une fois : le lien fixe "dernier" sert toujours la derniere version.
        if any(e.startswith(f"{dossier}-v") for e in publies):
            lien = f"{DEPOT}/releases/download/dernier/{nom.replace(' ', '-')}-Setup.exe"
            lignes.append(_ligne(nom, f"plugins/{dossier}/NOTES.md", m, "publie", f"[Installer (Windows)]({lien})"))
        else:
            lignes.append(_ligne(nom, f"plugins/{dossier}/NOTES.md", m, "pas encore publie", "-"))
    for dossier, (branche, m) in sorted(plugins_en_chantier().items(), key=lambda x: str(x[1][1].get("nom")).lower()):
        lignes.append(_ligne(str(m.get("nom")), f"{DEPOT}/tree/{branche}/plugins/{dossier}", m, "en chantier", "-"))
    return "\n".join(lignes)


def cmd_vitrine(_) -> int:
    texte = README.read_text(encoding="utf-8")
    if DEBUT not in texte or FIN not in texte:
        print(f"ERREUR README.md : reperes {DEBUT} et {FIN} introuvables")
        return 1
    avant, reste = texte.split(DEBUT, 1)
    nouveau = avant + DEBUT + "\n" + tableau_vitrine() + "\n" + FIN + reste.split(FIN, 1)[1]
    if nouveau != texte:
        README.write_text(nouveau, encoding="utf-8", newline="\n")
        print("README.md : tableau des plugins refait")
    else:
        print("README.md : tableau des plugins a jour")
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
    sub.add_parser("vitrine").set_defaults(f=cmd_vitrine)
    m = sub.add_parser("modifies")
    m.add_argument("base")
    m.set_defaults(f=cmd_modifies)
    a = p.parse_args()
    return a.f(a)


if __name__ == "__main__":
    sys.exit(main())
