#!/usr/bin/env python3
"""Outil du depot droz-lab. Python 3.9+, aucune dependance.

  python tools/droz.py bilan [--prenom fx]   A LANCER EN DEBUT DE SESSION : ce qui est pret, ce qui manque,
                                           et ce que l'assistant doit demander a la personne
  python tools/droz.py nouveau <dossier> --nom "Mon Plugin" [--type effet|instrument] [--auteur fx]
  python tools/droz.py verifier            registre des plugins (codes uniques, champs) et fiches (FICHE.md)
  python tools/droz.py palier <dossier>    palier du plugin : chantier (0.x) ou pro (1.0.0 et plus)
  python tools/droz.py niveau "<Nom>"      niveau pluginval exige pour ce plugin (5 ou 10)
  python tools/droz.py fiche-params <dossier> <params.txt>   les reglages de la fiche existent dans le plugin
  python tools/droz.py liste               tableau des plugins
  python tools/droz.py centre [--essais essais.json]      ecrit la page de l'atelier (plugins, chantiers, guide)
  python tools/droz.py accueil [--essais essais.json]     ecrit le README : le centre, puis tous les plugins et les chantiers
  python tools/droz.py attendus [--essais essais.json]    fichiers qui ont leur place sur la page "dernier" (menage)
  python tools/droz.py catalogue [--essais essais.json]   ecrit catalogue.json, lu par le Droz Software Center
  python tools/droz.py essais <commit> <branche> <dossier> <essais.json>   range les installeurs d'essai d'un run vert
  python tools/droz.py centre-touche <base>   "oui" si le Droz Software Center doit etre recompile depuis <base>
  python tools/droz.py modifies <base>     plugins touches depuis <base> (pour la compilation auto)
  python tools/droz.py depassees <fichier> parmi les etiquettes publiees du fichier, celles a retirer
"""
from __future__ import annotations

import argparse
import json
import os
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
# Le Droz Software Center (apps/centre) : recompile et reessaye quand un de ces chemins change.
CENTRE = RACINE / "apps" / "centre"
CENTRE_DEPEND = ("apps/", "libs/droz_ui/", "CMakeLists.txt", "cmake/", "tools/centre.iss", "tools/installeur.iss", ".github/workflows/")
# Noms de dossier pris par autre chose qu'un plugin (etiquettes centre-v<version>, page "dernier", essais).
RESERVES = ("centre", "dernier", "essais")
# Cibles de plugins retires du depot : une vieille branche qui les contient encore ne les fait pas revenir
# dans les chantiers ni dans les essais. (DrozPremierJet : le plugin de test "Test v0", retire le 27/11/2026.)
RETIRES = ("DrozPremierJet",)


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


def fiches_absentes(bloquantes: bool, dossiers=None) -> list[str]:
    """Plugins sans FICHE.md. Au palier pro c'est bloquant ; en chantier (0.x) c'est une alerte :
    un plugin ne d'avant la methode ne doit pas faire rougir la PR de son auteur."""
    out = []
    for dossier, m in metas().items():
        if dossier.startswith("_") or (dossiers is not None and dossier not in dossiers):
            continue
        if not (PLUGINS / dossier / "FICHE.md").exists() and (palier(m) == "pro") == bloquantes:
            out.append(dossier)
    return out


def erreurs_fiches(dossiers=None) -> list[str]:
    err = []
    for dossier, m in metas().items():
        if dossier.startswith("_") or (dossiers is not None and dossier not in dossiers):
            continue
        if not (PLUGINS / dossier / "FICHE.md").exists() and palier(m) != "pro":
            continue  # alerte seulement, voir fiches_absentes
        fiche = PLUGINS / dossier / "FICHE.md"
        aide = ("mets d'abord ta branche a jour avec `git fetch origin main && git merge origin/main`, "
                "puis lis docs/METHODE.md et lance `python tools/droz.py bilan` ; modele : plugins/_modele/FICHE.md")
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
    if a.dossier in RESERVES:
        raise SystemExit(f"dossier : '{a.dossier}' est reserve, en choisir un autre")
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
        if os.environ.get("GITHUB_ACTIONS"):  # visible en tete du run et sur la PR
            print(f"::error title=Fiche de plugin::{e}")
    if not fiches:
        print("fiches : aucune fiche incomplete")
    err += fiches
    for dossier in fiches_absentes(False):
        message = (f"{dossier} n'a pas encore sa FICHE.md. Rien n'est bloque en version 0.x. Pour l'ecrire : "
                   "`python tools/droz.py bilan` et suivre ce qu'il dit (docs/METHODE.md). Obligatoire pour une 1.0.0.")
        print("A FAIRE fiche :", message)
        if os.environ.get("GITHUB_ACTIONS"):
            print(f"::warning title=Fiche de plugin a ecrire::{message}")
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


DEPOT = "https://github.com/droz-lab/plugins"
BLOB = f"{DEPOT}/blob/main"
INSTALLEUR_CENTRE = "Droz-Software-Center-Setup.exe"
AUTEURS = {"yoy": "YoY", "zomban": "Zomban", "fx": "Fx", "equipe": "equipe"}


def _git(*args: str) -> str:
    # utf-8 dit en clair : sous Windows (essais.yml) le defaut abimerait les accents des plugin.json.
    r = subprocess.run(["git", *args], cwd=RACINE, capture_output=True, text=True, encoding="utf-8", errors="replace")
    return r.stdout if r.returncode == 0 else ""


_CHANTIERS = None


def plugins_en_chantier() -> dict[str, tuple[str, dict]]:
    """Plugins presents sur une branche du depot mais pas encore dans main : dossier -> (branche, plugin.json)."""
    global _CHANTIERS
    if _CHANTIERS is not None:
        return _CHANTIERS
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
            if str(meta.get("cible")) in cibles_main or str(meta.get("cible")) in RETIRES:
                continue
            # La branche au nom d'une personne passe avant une branche de travail automatique.
            if dossier not in trouves or (trouves[dossier][0].startswith("claude/") and not branche.startswith("claude/")):
                trouves[dossier] = (branche, meta)
    _CHANTIERS = trouves
    return trouves


def lien_installeur(nom: str) -> str:
    return f"{DEPOT}/releases/download/dernier/{nom.replace(' ', '-')}-Setup.exe"


def lien_essai(fichier: str) -> str:
    # Les essais sont sur la meme page que le reste : tout l'atelier tient sur "dernier".
    return f"{DEPOT}/releases/download/dernier/{fichier}"


def resume_fiche(texte: str) -> dict:
    """Ce que le Droz Software Center et la page d'accueil montrent d'une FICHE.md : l'intention, en clair,
    et le nom des reglages en facade. Fiche absente ou pas remplie : champs vides."""
    sections = sections_fiche(texte or "")
    intention = " ".join(sections.get("Intention", "").split())
    if "A REMPLIR" in intention:
        intention = ""
    if len(intention) > 420:
        intention = intention[:420].rsplit(". ", 1)[0].rstrip(".") + "."
    reglages = []
    for ligne in sections.get("Reglages en facade", "").splitlines():
        cases = [c.strip().strip("`") for c in ligne.strip().strip("|").split("|")]
        if not ligne.strip().startswith("|") or len(cases) < 2 or cases[0].lower() == "id" or set(cases[0]) <= set("-: "):
            continue
        if cases[1]:
            reglages.append(cases[1])
    return {"fiche": intention, "reglages": reglages[:8]}


def fiche_de(dossier: str) -> dict:
    try:
        return resume_fiche((PLUGINS / dossier / "FICHE.md").read_text(encoding="utf-8"))
    except OSError:
        return resume_fiche("")


def _qui(m: dict) -> str:
    return AUTEURS.get(str(m.get("auteur", "-")).lower(), str(m.get("auteur", "-")))


def _quoi(m: dict, limite: int = 90) -> str:
    d = re.sub(r"\s*\(migre[^)]*\)", "", str(m.get("description", ""))).replace("|", "/").strip() or "-"
    return d if len(d) <= limite else d.split(" : ")[0].split(", ")[0].rstrip(".")


def meta_centre() -> dict:
    """Identite du Droz Software Center (apps/centre/app.json). Vide tant qu'il n'est pas dans le depot."""
    try:
        return json.loads((CENTRE / "app.json").read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}


def _api(chemin: str):
    """Lecture de l'API GitHub par `gh`. Sans `gh` ou sans reseau : None, et la page se passe de l'info."""
    if not shutil.which("gh"):
        return None
    code, sortie = _commande("gh", "api", chemin, delai=40)
    if code != 0:
        return None
    try:
        return json.loads(sortie)
    except json.JSONDecodeError:
        return None


# --- Essais : l'installeur d'un plugin en chantier, publie a chaque Verdict vert de sa branche (essais.yml). ---
# Ils vivent sur la page fixe "dernier", avec tout le reste : un fichier par plugin, et essais.json qui dit de
# quel commit vient chacun.

def lire_essais(chemin) -> dict[str, dict]:
    """essais.json : {"plugins": {dossier: {nom, auteur, description, fiche, reglages, version, build, branche,
    fichier, date}}}."""
    try:
        contenu = json.loads(Path(chemin).read_text(encoding="utf-8")) if chemin else {}
    except (OSError, json.JSONDecodeError):
        contenu = {}
    plugins = contenu.get("plugins") if isinstance(contenu, dict) else None
    return {d: e for d, e in (plugins or {}).items() if isinstance(e, dict) and e.get("fichier")}


def essais_en_cours(chemin) -> dict[str, dict]:
    """Essais encore utiles : le plugin est toujours sur une branche, et pas encore dans main."""
    chantiers = plugins_en_chantier()
    return {d: e for d, e in lire_essais(chemin).items() if d in chantiers}


def cmd_essais(a) -> int:
    """Apres un run vert de la branche <branche> au commit <commit> : met a jour essais.json et dit, en JSON,
    quels installeurs poser sur la page "dernier" et lesquels en retirer. N'execute rien de la branche :
    seuls ses plugin.json et ses FICHE.md sont lus."""
    import datetime
    recu, fichier = Path(a.dossier), Path(a.fichier)
    essais = lire_essais(fichier)
    dans_main = set(metas())
    cibles_main = {str(m.get("cible")) for m in metas().values()}
    fichiers_main = {str(m.get("nom")).replace(" ", "-") + "-Setup.exe" for m in metas().values()} | {INSTALLEUR_CENTRE}
    poser, poses = [], set()
    for chemin in _git("ls-tree", "-r", "--name-only", a.commit, "--", "plugins").split("\n"):
        morceaux = chemin.split("/")
        if len(morceaux) != 3 or morceaux[2] != "plugin.json":
            continue
        dossier = morceaux[1]
        if dossier.startswith("_") or dossier in dans_main or dossier in RESERVES:
            continue
        try:
            meta = json.loads(_git("show", f"{a.commit}:{chemin}"))
        except json.JSONDecodeError:
            continue
        nom, version = str(meta.get("nom", "")), str(meta.get("version", ""))
        # Le nom devient un nom de fichier public : on n'accepte que des caracteres simples.
        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9 ._-]{0,60}", nom) or not re.fullmatch(r"\d+\.\d+\.\d+", version):
            continue
        if str(meta.get("cible")) in cibles_main or str(meta.get("cible")) in RETIRES:
            continue
        nom_fichier = nom.replace(" ", "-") + "-Setup.exe"
        if nom_fichier in fichiers_main:
            continue  # un essai ne remplace jamais l'installeur d'un plugin publie
        trouves = sorted(recu.rglob(nom_fichier)) if recu.is_dir() else []
        if not trouves:
            continue  # pas compile par ce run
        essais[dossier] = {
            "nom": nom, "auteur": _qui(meta), "description": _quoi(meta, 200),
            **resume_fiche(_git("show", f"{a.commit}:plugins/{dossier}/FICHE.md")), "version": version,
            "build": a.commit[:7], "branche": a.branche, "fichier": nom_fichier,
            "date": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%MZ"),
        }
        poser.append(str(trouves[0]))
        poses.add(dossier)
    # Menage : le plugin est arrive dans main, ou sa branche a disparu.
    chantiers = plugins_en_chantier()
    retirer = []
    for dossier in sorted(essais):
        if dossier not in poses and (dossier in dans_main or dossier not in chantiers):
            retirer.append(essais.pop(dossier)["fichier"])
    gardes = {e["fichier"] for e in essais.values()}
    fichier.write_text(json.dumps({"plugins": essais}, indent=1, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"poser": poser, "retirer": [f for f in retirer if f not in gardes]}))
    return 0


# --- Etat de l'atelier : commun a la page "dernier" et au catalogue du Droz Software Center. ---

def etat_atelier(essais: dict[str, dict]) -> dict:
    prs = {p["head"]["ref"]: p for p in (_api("repos/droz-lab/plugins/pulls?state=open&per_page=100") or [])}
    vus: dict[str, str] = {}

    def verdict(sha: str) -> str:
        if not sha:
            return "?"
        if sha not in vus:
            vus[sha] = "pas encore lance"
            for r in (_api(f"repos/droz-lab/plugins/commits/{sha}/check-runs?per_page=100") or {}).get("check_runs", []):
                if r.get("name") == "Verdict":
                    vus[sha] = "en cours" if r.get("status") != "completed" else ("vert" if r.get("conclusion") == "success" else "rouge")
                    break
        return vus[sha]

    chantiers = []
    for dossier, (branche, m) in sorted(plugins_en_chantier().items(), key=lambda x: str(x[1][1].get("nom")).lower()):
        chantiers.append({
            "dossier": dossier, "branche": branche, "meta": m, "pr": prs.get(branche),
            "fiche": bool(_git("ls-tree", "--name-only", f"origin/{branche}", f"plugins/{dossier}/FICHE.md").strip()),
            "verdict": verdict(_git("rev-parse", f"origin/{branche}").strip()),
            "essai": essais.get(dossier),
        })
    demandes = [{"numero": pr["number"], "titre": str(pr.get("title", "")), "auteur": pr["user"]["login"], "branche": branche,
                 "verdict": verdict(pr["head"]["sha"]), "brouillon": bool(pr.get("draft")), "url": f"{DEPOT}/pull/{pr['number']}"}
                for branche, pr in sorted(prs.items(), key=lambda x: x[1]["number"])]
    return {"chantiers": chantiers, "demandes": demandes,
            "fiches_a_ecrire": [str(c["meta"].get("nom")) for c in chantiers if not c["fiche"]]}


def message_a_coller() -> str:
    """Le message unique de docs/ATELIER.md, que le Droz Software Center copie dans le presse-papiers."""
    try:
        texte = (RACINE / "docs" / "ATELIER.md").read_text(encoding="utf-8")
    except OSError:
        return ""
    m = re.search(r"## Commencer ici.*?```\n(.*?)```", texte, flags=re.S)
    return m.group(1).strip() if m else ""


def publies() -> list[tuple[str, dict]]:
    """Plugins de main dont l'installeur est sur la page "dernier". Sans acces a GitHub : tous ceux de main."""
    page = _api("repos/droz-lab/plugins/releases/tags/dernier")
    poses = {f.get("name") for f in page.get("assets", [])} if isinstance(page, dict) and "assets" in page else None
    out = []
    for dossier, m in metas().items():
        if dossier.startswith("_"):
            continue
        if poses is None or str(m.get("nom")).replace(" ", "-") + "-Setup.exe" in poses:
            out.append((dossier, m))
    return out


def cmd_catalogue(a) -> int:
    """catalogue.json, lu par le Droz Software Center : plugins publies, plugins a l'essai, etat de l'atelier.
    Ecrit sur la sortie standard. `centre.yml` le pose sur la page fixe "dernier"."""
    import datetime
    essais = essais_en_cours(a.essais)
    etat = etat_atelier(essais)
    plugins = []
    for dossier, m in publies():
        plugins.append({
            "dossier": dossier, "nom": str(m.get("nom")), "auteur": _qui(m), "description": _quoi(m, 200),
            **fiche_de(dossier), "version": str(m.get("version")), "build": "", "etat": "publie",
            "installeur": lien_installeur(str(m.get("nom"))),
            "page": f"{BLOB}/plugins/{dossier}/{'FICHE.md' if (PLUGINS / dossier / 'FICHE.md').exists() else 'NOTES.md'}",
        })
    noms = {p["nom"] for p in plugins}
    for dossier, e in sorted(essais.items(), key=lambda x: str(x[1].get("nom")).lower()):
        if e.get("nom") in noms:
            continue
        noms.add(e.get("nom"))
        plugins.append({
            "dossier": dossier, "nom": str(e.get("nom")), "auteur": str(e.get("auteur", "-")), "description": str(e.get("description", "")),
            "fiche": str(e.get("fiche", "")), "reglages": list(e.get("reglages") or []),
            "version": str(e.get("version")), "build": str(e.get("build", "")), "etat": "essai",
            "installeur": lien_essai(str(e.get("fichier"))), "branche": str(e.get("branche", "")),
            "page": f"{DEPOT}/tree/{e.get('branche')}/plugins/{dossier}",
        })
    centre = meta_centre()
    catalogue = {
        "format": 1,
        "genere": datetime.datetime.now(datetime.timezone.utc).strftime("%d/%m/%Y %H:%M UTC"),
        "centre": {"version": str(centre.get("version", "")), "installeur": f"{DEPOT}/releases/download/dernier/{INSTALLEUR_CENTRE}",
                   "page": f"{DEPOT}/releases/tag/dernier"},
        "plugins": plugins,
        "atelier": {
            "pr": [{k: d[k] for k in ("numero", "titre", "auteur", "verdict", "brouillon", "url")} for d in etat["demandes"]],
            "fiches_a_ecrire": etat["fiches_a_ecrire"],
            "message": message_a_coller(),
            "liens": [{"nom": "Méthode", "url": f"{BLOB}/docs/METHODE.md"},
                      {"nom": "Habillage", "url": f"{BLOB}/docs/HABILLAGE.md"},
                      {"nom": "Pièges", "url": f"{BLOB}/docs/PIEGES.md"},
                      {"nom": "Idée de plugin", "url": f"{DEPOT}/issues/new?template=idee-de-plugin.yml"},
                      {"nom": "Page de l'atelier", "url": f"{DEPOT}/releases/tag/dernier"}],
        },
    }
    print(json.dumps(catalogue, indent=1, ensure_ascii=False))
    return 0


def cmd_centre(a) -> int:
    """La page de l'atelier, pour qui n'a pas le Droz Software Center : tout ce qui existe, comment l'installer,
    ou en est chaque chantier, par ou commencer. Ecrite sur la sortie standard. `centre.yml` la pose sur la
    page fixe "dernier", a cote des installeurs et de catalogue.json."""
    import datetime
    essais = essais_en_cours(a.essais)
    etat = etat_atelier(essais)
    out = ["# Droz Lab : l'atelier", "",
           f"Tout ce qui existe dans l'atelier, au meme endroit. Page refaite toute seule, derniere fois le "
           f"{datetime.datetime.now(datetime.timezone.utc).strftime('%d/%m/%Y a %H:%M')} UTC.", ""]
    if meta_centre():
        out += ["## Le plus simple : le Droz Software Center", "",
                f"**[Telecharger le Droz Software Center]({DEPOT}/releases/download/dernier/{INSTALLEUR_CENTRE})** (Windows). "
                "Une seule appli pour installer, mettre a jour et desinstaller tous les plugins Droz, chantiers compris. "
                "Windows signale un editeur inconnu : \"Informations complementaires\" puis \"Executer quand meme\".", ""]
    out += ["## Plugins", "",
            "Sans l'appli : clique sur \"Installer\", double-clique sur le fichier, choisis ton dossier VST3. "
            "Le lien donne toujours la derniere version : pour mettre a jour, reinstalle par-dessus.", "",
            "| Plugin | Version | Quoi | Par | Windows | En savoir plus |", "|---|---|---|---|---|---|"]
    lignes = 0
    for dossier, m in metas().items():
        if dossier.startswith("_"):
            continue
        lignes += 1
        fiche = f"[fiche]({BLOB}/plugins/{dossier}/FICHE.md), " if (PLUGINS / dossier / "FICHE.md").exists() else ""
        out.append(f"| **{m.get('nom')}** | {m.get('version')} | {_quoi(m)} | {_qui(m)} | [Installer]({lien_installeur(str(m.get('nom')))}) | "
                   f"{fiche}[notes]({BLOB}/plugins/{dossier}/NOTES.md) |")
    if not lignes:
        out.append("| - | - | aucun plugin publie pour l'instant : voir les chantiers ci-dessous | - | - | - |")
    out += ["", f"Mac et Linux : zips sur la page de chaque version ([Releases]({DEPOT}/releases)), marche a suivre dans le [README]({DEPOT}#readme).", "",
            "## En chantier", "",
            "Les plugins en cours, pas encore dans la version commune. A chaque push, des que le controle Verdict de la branche est vert, "
            "son plugin est installable \"a l'essai\", ici et dans le Droz Software Center.", "",
            "| Plugin | Par | Quoi | Ou ca en est | Fiche | Essayer | Suivre |", "|---|---|---|---|---|---|---|"]
    for c in etat["chantiers"]:
        m, pr, e = c["meta"], c["pr"], c["essai"]
        if pr:
            ou = f"PR {pr['number']}" + (" (brouillon)" if pr.get("draft") else "") + f", Verdict {c['verdict']}"
            suivre = f"[PR {pr['number']}]({DEPOT}/pull/{pr['number']})"
        else:
            ou = f"branche sans PR, Verdict {c['verdict']}"
            suivre = f"[branche]({DEPOT}/tree/{c['branche']}/plugins/{c['dossier']})"
        essayer = f"[Installer l'essai]({lien_essai(e['fichier'])}) ({e.get('version')}, {e.get('build')})" if e else "pas encore de Verdict vert"
        out.append(f"| **{m.get('nom')}** | {_qui(m)} | {_quoi(m)} | {ou} | {'oui' if c['fiche'] else 'a ecrire'} | {essayer} | {suivre} |")
    if not etat["chantiers"]:
        out.append("| - | - | aucun chantier en cours | - | - | - | - |")
    out += ["", "## Par ou commencer", "",
            f"- **Reprendre mon plugin, ou en creer un** : [ouvre le depot avec ton assistant et dis ce que tu veux]({BLOB}/docs/ATELIER.md#commencer-ici). Les regles se chargent toutes seules : il met ta branche a jour, te pose ses questions en une fois, ecrit la fiche et s'occupe du reste.",
            f"- **Proposer une idee de plugin** sans la faire toi-meme : [formulaire]({DEPOT}/issues/new?template=idee-de-plugin.yml).",
            f"- **Signaler un souci** (plugin qui plante, controle qui bloque a tort) : [formulaire]({DEPOT}/issues/new?template=souci.yml).",
            f"- **Comprendre comment on travaille** : [la methode]({BLOB}/docs/METHODE.md). Les trois premieres parties suffisent, la suite sert surtout a l'assistant.",
            f"- **Voir a quoi ressemble un plugin Droz** : [l'habillage]({BLOB}/docs/HABILLAGE.md).",
            f"- **Eviter les pieges deja rencontres** : [les lecons]({BLOB}/docs/PIEGES.md).", "",
            "## A faire en ce moment", ""]
    taches = []
    if etat["fiches_a_ecrire"]:
        taches.append(f"Fiche a ecrire pour : {', '.join(etat['fiches_a_ecrire'])}. L'assistant de son auteur s'en charge a la prochaine session. Rien n'est bloque en attendant.")
    for d in etat["demandes"]:
        if d["verdict"] == "rouge":
            taches.append(f"[PR {d['numero']}]({d['url']}) ({d['auteur']}) : Verdict rouge. A dire a son assistant : \"lis le journal du Verdict de ma PR et corrige\".")
        elif d["verdict"] == "vert" and not d["brouillon"]:
            taches.append(f"[PR {d['numero']}]({d['url']}) ({d['auteur']}) : Verdict vert. Lire les commentaires de la PR, puis son auteur peut la fusionner.")
    out += [f"- {t}" for t in taches] or ["- Rien en attente."]
    out += ["", "## Regles du jeu, en quatre lignes", "",
            "1. Chacun est chez lui dans son dossier de plugin. Ce qui est commun passe par une PR relue.",
            "2. Un plugin a sa fiche avant son code : le son voulu, les reglages, les cas limites.",
            "3. Version 0.x : on cherche, on publie vite, la methode guide sans bloquer. Version 1.0.0 : palier pro, tout est verifie.",
            "4. Le controle Verdict dit si c'est bon. Vert : on fusionne, et la version part toute seule.", ""]
    print("\n".join(out))
    return 0


def cmd_vitrine(_) -> int:
    # Commande gardee pour les branches et les habitudes d'avant : elle ne casse rien.
    print("Plus rien a faire : la page d'accueil (README), le catalogue et la page de l'atelier se refont tout seuls a chaque fusion.")
    return 0


def cmd_accueil(a) -> int:
    """Le README, page d'accueil du depot : le Droz Software Center d'abord, puis chaque plugin avec ce qu'il
    fait, puis les chantiers. Ecrit sur la sortie standard. `centre.yml` le range dans main tout seul, par une
    PR du robot : ne pas le modifier a la main. Rien de ce qui bouge a chaque push n'y figure (ni build, ni
    etat du Verdict), pour que la page ne change que quand un plugin change."""
    essais = essais_en_cours(a.essais)
    out = ["<!-- Page ecrite par `python tools/droz.py accueil` et rangee toute seule (centre.yml). Ne pas modifier a la main. -->",
           "# DROZ LAB", "",
           "Plugins audio VST3 du collectif [Droz](https://drozrecordz.com).", "",
           "## Droz Software Center", "",
           f"**[Telecharger le Droz Software Center]({DEPOT}/releases/download/dernier/{INSTALLEUR_CENTRE})** (Windows)", "",
           "Une seule appli pour installer, mettre a jour et desinstaller tous les plugins ci-dessous, chantiers compris, d'un clic. "
           "Double-clique sur le fichier telecharge. Windows signale un editeur inconnu : \"Informations complementaires\" puis \"Executer quand meme\".", ""]

    def bloc(nom, version, auteur, resume, reglages, liens) -> list[str]:
        lignes = [f"### {nom}", "", f"`{version}` - par {auteur}", "", resume]
        if reglages:
            lignes += ["", "Reglages : " + ", ".join(reglages) + "."]
        return lignes + ["", " · ".join(liens), ""]

    out += ["## Plugins", ""]
    sortis = [(d, m) for d, m in metas().items() if not d.startswith("_")]
    for dossier, m in sortis:
        f = fiche_de(dossier)
        out += bloc(str(m.get("nom")), str(m.get("version")), _qui(m), f["fiche"] or _quoi(m, 400), f["reglages"],
                    [f"[Installer (Windows)]({lien_installeur(str(m.get('nom')))})",
                     f"[fiche complete](plugins/{dossier}/FICHE.md)" if (PLUGINS / dossier / "FICHE.md").exists()
                     else f"[notes](plugins/{dossier}/NOTES.md)"])
    if not sortis:
        out += ["Aucun plugin publie pour l'instant : les premiers sont en chantier, juste en dessous.", ""]

    out += ["## En chantier", "",
            "Les plugins en cours de fabrication. Chaque mise a jour d'une branche qui passe les controles devient installable \"a l'essai\", ici et dans le Droz Software Center.", ""]
    chantiers = sorted(plugins_en_chantier().items(), key=lambda x: str(x[1][1].get("nom")).lower())
    for dossier, (branche, m) in chantiers:
        e = essais.get(dossier)
        liens = [f"[Installer l'essai (Windows)]({lien_essai(e['fichier'])})"] if e else ["pas encore installable"]
        liens.append(f"[suivre le chantier]({DEPOT}/tree/{branche}/plugins/{dossier})")
        out += bloc(str(m.get("nom")), f"{e.get('version')} essai" if e else f"{m.get('version')} en chantier", _qui(m), (e or {}).get("fiche") or _quoi(m, 400),
                    (e or {}).get("reglages") or [], liens)
    if not chantiers:
        out += ["Aucun chantier en cours.", ""]

    out += ["## Participer", "",
            f"- Membres du collectif : [par ou commencer](docs/ATELIER.md). Ou en est chaque chantier : [la page de l'atelier]({DEPOT}/releases/tag/dernier).",
            f"- Une idee de plugin : [formulaire]({DEPOT}/issues/new?template=idee-de-plugin.yml). Un souci : [formulaire]({DEPOT}/issues/new?template=souci.yml).",
            "- Comment on travaille : [la methode](docs/METHODE.md), [l'habillage](docs/HABILLAGE.md).", "",
            "<details>", "<summary>Mac, Linux, zips</summary>", "",
            f"Zips de chaque version : [Releases]({DEPOT}/releases).", "",
            "- Mac : `curl -fsSL https://raw.githubusercontent.com/droz-lab/plugins/main/tools/installer.sh | bash -s -- <dossier>`",
            "- Mac a la main : copie le `.vst3` dans `~/Library/Audio/Plug-Ins/VST3/`, puis `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/\"<Nom>.vst3\"`.",
            "- Windows a la main : copie le dossier `.vst3` du zip dans ton dossier VST3.", "",
            "</details>"]
    print("\n".join(out))
    return 0


def cmd_attendus(a) -> int:
    """Les fichiers qui ont leur place sur la page "dernier", un par ligne : l'installeur de chaque plugin de
    main, ceux des essais en cours, l'appli, le catalogue. `sortie.yml` retire le reste."""
    noms = {str(m.get("nom")).replace(" ", "-") + "-Setup.exe" for d, m in metas().items() if not d.startswith("_")}
    noms |= {str(e.get("fichier")) for e in lire_essais(a.essais).values()}
    noms |= {INSTALLEUR_CENTRE, "catalogue.json", "essais.json"}
    print("\n".join(sorted(noms)))
    return 0


def cmd_centre_touche(a) -> int:
    """ "oui" si le Droz Software Center doit etre recompile et reessaye depuis <base>. Dans le doute : oui."""
    try:
        diff = subprocess.run(["git", "diff", "--name-only", f"{a.base}...HEAD"], cwd=RACINE,
                              capture_output=True, text=True, check=True).stdout.split()
    except subprocess.CalledProcessError:
        diff = None
    if not meta_centre():
        print("non")
    else:
        print("oui" if diff is None or any(f.startswith(CENTRE_DEPEND) for f in diff) else "non")
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
        en_pro = palier(metas().get(a.dossier, {})) == "pro"
        print(f"{'ERREUR' if en_pro else 'A FAIRE'} fiche : {a.dossier} : pas de FICHE.md" + ("" if en_pro else " (pas bloquant en 0.x)"))
        return 1 if en_pro else 0
    reels = {l.split()[0] for l in Path(a.params).read_text(encoding="utf-8", errors="replace").splitlines()[1:] if l.strip()}
    absents = [i for i in ids_fiche(fiche.read_text(encoding="utf-8", errors="replace")) if i not in reels]
    if absents:
        print(f"ERREUR fiche : {a.dossier} : reglage(s) de FICHE.md absents du plugin : {', '.join(absents)}")
        print(f"  parametres du plugin : {', '.join(sorted(reels)) or 'aucun'}")
        print("  la fiche fait foi : corriger le plugin, ou la fiche si la decision a change (et le dire dans NOTES.md)")
        return 1
    print(f"fiche : {a.dossier} : les reglages de la fiche existent dans le plugin")
    return 0


MEMBRES = ("fx", "yoy", "zomban")
QUESTIONS_PROFIL = (
    "Prenom (celui de tes branches)",
    "Niveau en code : aucun, un peu, a l'aise",
    "DAW et systeme (ex. Ableton Live 12 sur Windows 11, Bitwig sur Mac)",
    "Dossier VST3 si ce n'est pas celui par defaut",
    "Comment tu ecoutes (enceintes, casque) et ou tu veux recevoir captures et rendus (telephone, ordinateur)",
    "Deux ou trois plugins que tu prends comme reference de qualite",
)


def _commande(*args: str, delai: int = 20) -> tuple[int, str]:
    try:
        r = subprocess.run(list(args), cwd=RACINE, capture_output=True, text=True, timeout=delai)
        return r.returncode, (r.stdout + r.stderr).strip()
    except (OSError, subprocess.TimeoutExpired) as e:
        return 1, str(e)


def cmd_bilan(a) -> int:
    """Etat de la session : ne modifie rien. L'assistant lit la fin ("A REGLER", "A DEMANDER") et agit."""
    regler, demander, infos = [], [], []

    # 1. Branche et fraicheur des regles
    branche = _git("rev-parse", "--abbrev-ref", "HEAD").strip() or "?"
    infos.append(f"branche : {branche}")
    _commande("git", "fetch", "--quiet", "origin", "main")
    retard = _git("rev-list", "--count", "HEAD..origin/main").strip()
    if retard.isdigit() and int(retard) > 0:
        regler.append(f"ta branche a {retard} commit(s) de retard sur main : les regles que tu viens de lire sont peut-etre anciennes. "
                      "Lance `git merge origin/main` (ou repars de main), puis relis CLAUDE.md et docs/METHODE.md")
    elif not retard.isdigit():
        regler.append("impossible de comparer a origin/main (pas de reseau ou pas de depot distant) : verifie que tu travailles bien sur un clone de droz-lab/plugins")
    if branche in ("main", "HEAD"):
        regler.append("tu es sur main : cree une branche `<prenom>/<dossier>-<sujet>` avant toute modification")

    # 2. Qui est la personne
    prenom = (a.prenom or "").lower()
    if not prenom and "/" in branche and branche.split("/")[0] in MEMBRES:
        prenom = branche.split("/")[0]
    if not prenom:
        demander.append("Ton prenom (fx, yoy ou zomban) : il nomme tes branches et ton dossier de carnet")
    else:
        infos.append(f"personne : {prenom}")
        profil = RACINE / "carnet" / prenom / "PROFIL.md"
        if profil.exists():
            infos.append(f"profil : carnet/{prenom}/PROFIL.md (lis-le, ne repose pas ces questions)")
        else:
            demander.append(f"De quoi ecrire carnet/{prenom}/PROFIL.md (une fois pour toutes, le depot est public : rien de personnel) : "
                            + " ; ".join(QUESTIONS_PROFIL[1:]))

    # 3. Plugins de la personne et plugins touches par la branche
    tous = metas()
    touches = set()
    for f in _git("diff", "--name-only", "origin/main...HEAD").split():
        if f.startswith("plugins/") and f.count("/") >= 2:
            touches.add(f.split("/")[1])
    concernes = sorted(d for d, m in tous.items() if not d.startswith("_") and (d in touches or str(m.get("auteur", "")).lower() == prenom))
    for d in concernes:
        infos.append(f"plugin : {d} ({tous[d].get('nom')}, version {tous[d].get('version')}, palier {palier(tous[d])})")
    for d in (fiches_absentes(False, concernes) + fiches_absentes(True, concernes)) if concernes else []:
        regler.append(f"{d} n'a pas de FICHE.md : copie plugins/_modele/FICHE.md dans plugins/{d}/, remplis-la a partir de ce que le plugin "
                      "fait deja (lis son code et son NOTES.md), note ce que tu supposes `SUPPOSE : ...`, puis resume-la en cinq lignes a la personne")
    for e in erreurs_fiches(concernes) if concernes else []:
        regler.append("fiche, " + e.split(" (mets d'abord")[0])
    for d in concernes:
        if not (PLUGINS / d / "NOTES.md").exists():
            regler.append(f"{d} : pas de NOTES.md (memoire du plugin)")
        if not list((PLUGINS / d / "boucles").glob("*.wav")):
            demander.append(f"Une ou deux boucles courtes pour {d} (WAV, moins de 10 s, faites par toi, sans droits) : sans elles, pas de rendu a ecouter. Sinon je fabrique un signal de test")
    if not concernes:
        infos.append("aucun plugin a ton nom ni touche par cette branche : pour en creer un, `python tools/droz.py nouveau ...` puis la fiche")

    # 4. Acces : pousser, ouvrir une PR, lire les journaux
    code, sortie = _commande("git", "push", "--dry-run", "origin", "HEAD", delai=30)
    if code != 0:
        demander.append("Le droit de pousser sur droz-lab/plugins : je n'y arrive pas d'ici (" + (sortie.splitlines() or ["erreur"])[-1][:120] + "). "
                        "Verifie que tu as accepte l'invitation a l'organisation droz-lab et que ton assistant est relie a GitHub avec ce depot")
    else:
        infos.append("acces : je peux pousser une branche")
    if shutil.which("gh"):
        code, _ = _commande("gh", "api", "repos/droz-lab/plugins/pulls?per_page=1")
        if code != 0:
            demander.append("L'acces a GitHub pour ouvrir une PR et lire les journaux du Verdict : `gh` est installe mais pas connecte (`gh auth login`, a faire toi-meme, ne colle aucun jeton ici)")
        else:
            infos.append("acces : je peux ouvrir une PR et lire les journaux Actions")
    else:
        infos.append("pas de `gh` ici : si tu n'as pas d'autre moyen d'ouvrir une PR et de lire les journaux du Verdict, demande a la personne de te relier a GitHub")

    # 5. Compiler ici (facultatif, GitHub compile de toute facon)
    if shutil.which("cmake") and (shutil.which("g++") or shutil.which("clang++") or shutil.which("cl")):
        infos.append("compilation locale possible (cmake et compilateur presents) : plus rapide que d'attendre GitHub")
    else:
        infos.append("pas de compilation locale ici : GitHub compile a chaque PR (10 minutes), prevois-le")

    print("BILAN DE SESSION (docs/METHODE.md)\n")
    for i in infos:
        print("  -", i)
    print("\nA REGLER TOI-MEME, avant tout autre travail :")
    for r in regler or ["rien"]:
        print("  -", r)
    print("\nA DEMANDER A LA PERSONNE, en un seul message, a choix quand c'est possible :")
    for d in demander or ["rien"]:
        print("  -", d)
    print("\nS'il te manque autre chose pour bien faire (une reference d'ecoute, une decision, un acces, un fichier) : "
          "dis-le tout de suite et demande-le, ne devine pas en silence.")
    return 0


def cmd_depassees(a) -> int:
    """Etiquettes <dossier>-v<version> qui ne sont plus la version de main d'un plugin present.
    Tout le reste (page "dernier", etiquette d'une autre forme) n'est jamais propose."""
    courantes = {f"{d}-v{m.get('version')}" for d, m in metas().items() if not d.startswith("_")}
    if meta_centre():  # le Droz Software Center se publie comme un plugin : etiquette centre-v<version>
        courantes.add(f"centre-v{meta_centre().get('version')}")
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
    ce = sub.add_parser("centre")
    ce.add_argument("--essais", default="")
    ce.set_defaults(f=cmd_centre)
    for nom, fonction in (("accueil", cmd_accueil), ("attendus", cmd_attendus)):
        sp = sub.add_parser(nom)
        sp.add_argument("--essais", default="")
        sp.set_defaults(f=fonction)
    ca = sub.add_parser("catalogue")
    ca.add_argument("--essais", default="")
    ca.set_defaults(f=cmd_catalogue)
    es = sub.add_parser("essais")
    es.add_argument("commit")
    es.add_argument("branche")
    es.add_argument("dossier")
    es.add_argument("fichier")
    es.set_defaults(f=cmd_essais)
    ct = sub.add_parser("centre-touche")
    ct.add_argument("base")
    ct.set_defaults(f=cmd_centre_touche)
    m = sub.add_parser("modifies")
    m.add_argument("base")
    m.set_defaults(f=cmd_modifies)
    bi = sub.add_parser("bilan")
    bi.add_argument("--prenom", default="")
    bi.set_defaults(f=cmd_bilan)
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
