#!/usr/bin/env python3
"""Outil du depot droz-lab. Python 3.9+, aucune dependance.

  python tools/droz.py nouveau <dossier> --nom "Mon Plugin" [--type effet|instrument] [--auteur fx]
  python tools/droz.py verifier            registre des plugins (codes uniques, champs) et fiches (FICHE.md)
  python tools/droz.py palier <dossier>    palier du plugin : chantier (0.x) ou pro (1.0.0 et plus)
  python tools/droz.py niveau "<Nom>"      niveau pluginval exige pour ce plugin (5 ou 10)
  python tools/droz.py fiche-params <dossier> <params.txt>   les reglages de la fiche existent dans le plugin
  python tools/droz.py liste               tableau des plugins
  python tools/droz.py vitrine             refait le tableau des plugins du README (publies et en chantier)
  python tools/droz.py modifies <base>     plugins touches depuis <base> (pour la compilation auto)
  python tools/droz.py depassees <fichier> parmi les etiquettes publiees du fichier, celles a retirer
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


# --- Fiche de plugin (docs/METHODE.md) : ecrite avant le code, exigee par le controle Verdict. ---
SECTIONS_FICHE = ("Intention", "References d'ecoute", "Signal", "Reglages en facade", "Cas limites",
                  "Qualite attendue", "Comment on verifie", "Hors perimetre", "Questions ouvertes")


def palier(meta: dict) -> str:
    """0.x = chantier (on cherche, on publie vite). 1.0.0 et plus = pro (tous les controles bloquent)."""
    try:
        return "pro" if int(str(meta.get("version", "0")).split(".")[0]) >= 1 else "chantier"
    except ValueError:
        return "chantier"


def sections_fiche(texte: str) -> dict[str, str]:
    texte = re.sub(r"<!--.*?-->", "", texte, flags=re.S)
    out, titre = {}, None
    for ligne in texte.splitlines():
        m = re.match(r"##\s+(.*\S)\s*$", ligne)
        if m:
            titre = m.group(1)
            out[titre] = ""
        elif titre is not None:
            out[titre] += ligne + "\n"
    return out


def ids_fiche(texte: str) -> list[str]:
    """Identifiants de la premiere colonne du tableau "Reglages en facade"."""
    corps = sections_fiche(texte).get("Reglages en facade", "")
    ids = []
    for ligne in corps.splitlines():
        cases = [c.strip().strip("`") for c in ligne.strip().strip("|").split("|")]
        if not ligne.strip().startswith("|") or not cases or cases[0].lower() == "id" or set(cases[0]) <= set("-: "):
            continue
        if cases[0]:
            ids.append(cases[0])
    return ids


def erreurs_fiches(dossiers=None) -> list[str]:
    err = []
    for dossier in metas():
        if dossier.startswith("_") or (dossiers is not None and dossier not in dossiers):
            continue
        fiche = PLUGINS / dossier / "FICHE.md"
        aide = "voir docs/METHODE.md, modele : plugins/_modele/FICHE.md"
        if not fiche.exists():
            err.append(f"{dossier} : pas de FICHE.md ({aide})")
            continue
        texte = fiche.read_text(encoding="utf-8", errors="replace")
        sections = sections_fiche(texte)
        for titre in SECTIONS_FICHE:
            if titre not in sections:
                err.append(f"{dossier} : FICHE.md sans la section '{titre}' ({aide})")
            elif "A REMPLIR" in sections[titre] or not sections[titre].strip():
                err.append(f"{dossier} : FICHE.md, section '{titre}' pas remplie")
        if "Reglages en facade" in sections and "A REMPLIR" not in sections["Reglages en facade"]:
            n = len(ids_fiche(texte))
            if n == 0:
                err.append(f"{dossier} : FICHE.md, tableau 'Reglages en facade' vide (une ligne par reglage, id en premiere colonne)")
            elif n > 6:
                err.append(f"{dossier} : FICHE.md, {n} reglages en facade (6 au plus, le reste derriere un volet et hors du tableau)")
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
    fiche = cible_dir / "FICHE.md"
    fiche.write_text(fiche.read_text(encoding="utf-8").replace("# Fiche : Droz Modele", f"# Fiche : {a.nom}"), encoding="utf-8")
    err = erreurs_registre()
    if err:
        shutil.rmtree(cible_dir)
        raise SystemExit("\n".join(err))
    print(f"cree : plugins/{a.dossier} (cible {cible}, code {meta['code']})")
    print(f"AVANT le code : remplir plugins/{a.dossier}/FICHE.md (docs/METHODE.md). Le Verdict refuse une fiche incomplete.")
    print(f"compiler : cmake --build build --target {cible}_VST3 {cible}_outil")
    return 0


def cmd_verifier(_) -> int:
    err = erreurs_registre()
    for e in err:
        print("ERREUR", e)
    if not err:
        print(f"registre : {len(metas())} plugin(s), aucun conflit")
    fiches = erreurs_fiches()
    for e in fiches:
        print("ERREUR fiche :", e)
    if not fiches:
        print("fiches : toutes presentes et remplies")
    err += fiches
    # Non bloquant : un oubli ne doit pas faire rougir la PR de quelqu'un d'autre.
    lisezmoi = README.read_text(encoding="utf-8")
    for dossier, m in metas().items():
        if not dossier.startswith("_") and lien_installeur(str(m.get("nom"))) not in lisezmoi:
            print(f"A FAIRE vitrine : {m.get('nom')} manque dans le README, lancer `python tools/droz.py vitrine`")
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


def _ligne(nom: str, lien: str, meta: dict, telecharger: str) -> str:
    description = re.sub(r"\s*\(migre[^)]*\)", "", str(meta.get("description", ""))).replace("|", "/").strip() or "-"
    if len(description) > 70:  # court : le detail est sur la page du plugin
        description = description.split(" : ")[0].split(", ")[0].rstrip(".")
    auteur = {"yoy": "YoY", "zomban": "Zomban", "fx": "Fx", "equipe": "equipe"}.get(str(meta.get("auteur", "-")).lower(), meta.get("auteur", "-"))
    return f"| [{nom}]({lien}) | {description} | {auteur} | {telecharger} |"


def lien_installeur(nom: str) -> str:
    return f"{DEPOT}/releases/download/dernier/{nom.replace(' ', '-')}-Setup.exe"


def tableau_vitrine() -> str:
    lignes = ["| Plugin | Quoi | Par | Windows |", "|---|---|---|---|"]
    # Dans main = publie : sortie.yml publie toute version fusionnee. Le lien "dernier" sert toujours la derniere.
    for dossier, m in metas().items():
        if not dossier.startswith("_"):
            lignes.append(_ligne(str(m.get("nom")), f"plugins/{dossier}/NOTES.md", m, f"[Installer]({lien_installeur(str(m.get('nom')))})"))
    # Sur une branche = en chantier : les essais sont les fichiers des compilations de la branche (compte GitHub requis).
    for dossier, (branche, m) in sorted(plugins_en_chantier().items(), key=lambda x: str(x[1][1].get("nom")).lower()):
        essais = f"{DEPOT}/actions/workflows/compilation.yml?query=branch%3A{branche.replace('/', '%2F')}+is%3Asuccess"
        lignes.append(_ligne(str(m.get("nom")), f"{DEPOT}/tree/{branche}/plugins/{dossier}", m, f"en chantier, [essais]({essais})"))
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


def cmd_palier(a) -> int:
    m = metas().get(a.dossier)
    if m is None:
        raise SystemExit(f"pas de plugin dans plugins/{a.dossier}")
    print(palier(m))
    return 0


def cmd_niveau(a) -> int:
    """Niveau pluginval exige, a partir du nom du .vst3 : 5 en chantier, 10 au palier pro."""
    for m in metas().values():
        if str(m.get("nom")) == a.nom:
            print(10 if palier(m) == "pro" else 5)
            return 0
    print(5)
    return 0


def cmd_fiche_params(a) -> int:
    """Chaque reglage de la fiche doit exister dans le plugin (sortie de `<cible>_outil params`)."""
    fiche = PLUGINS / a.dossier / "FICHE.md"
    if not fiche.exists():
        print(f"ERREUR fiche : {a.dossier} : pas de FICHE.md")
        return 1
    reels = {l.split()[0] for l in Path(a.params).read_text(encoding="utf-8", errors="replace").splitlines()[1:] if l.strip()}
    absents = [i for i in ids_fiche(fiche.read_text(encoding="utf-8", errors="replace")) if i not in reels]
    if absents:
        print(f"ERREUR fiche : {a.dossier} : reglage(s) de FICHE.md absents du plugin : {', '.join(absents)}")
        print(f"  parametres du plugin : {', '.join(sorted(reels)) or 'aucun'}")
        print("  la fiche fait foi : corriger le plugin, ou la fiche si la decision a change (et le dire dans NOTES.md)")
        return 1
    print(f"fiche : {a.dossier} : les reglages de la fiche existent dans le plugin")
    return 0


def cmd_depassees(a) -> int:
    """Etiquettes <dossier>-v<version> qui ne sont plus la version de main d'un plugin present.
    Tout le reste (page "dernier", etiquette d'une autre forme) n'est jamais propose."""
    courantes = {f"{d}-v{m.get('version')}" for d, m in metas().items() if not d.startswith("_")}
    for tag in Path(a.fichier).read_text(encoding="utf-8").split():
        if re.fullmatch(r"[a-z0-9][a-z0-9-]*-v\d+\.\d+\.\d+", tag) and tag not in courantes:
            print(tag)
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
    de = sub.add_parser("depassees")
    de.add_argument("fichier")
    de.set_defaults(f=cmd_depassees)
    pa = sub.add_parser("palier")
    pa.add_argument("dossier")
    pa.set_defaults(f=cmd_palier)
    ni = sub.add_parser("niveau")
    ni.add_argument("nom")
    ni.set_defaults(f=cmd_niveau)
    fp = sub.add_parser("fiche-params")
    fp.add_argument("dossier")
    fp.add_argument("params")
    fp.set_defaults(f=cmd_fiche_params)
    a = p.parse_args()
    return a.f(a)


if __name__ == "__main__":
    sys.exit(main())
