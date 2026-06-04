#!/usr/bin/env python3
# Generate *_fr.qtts from *_en.qtts using en_fr_glossary.json (or built-in fallbacks).
# Run from src/gui: python3 tools/generate_fr_qtts.py

import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parent.parent
GLOSSARY_PATH = Path(__file__).resolve().parent / "en_fr_glossary.json"

# Whole-string overrides (IME UI; checked before phrase replacement).
WHOLE_STRING = {
    "OK": "OK",
    "&OK": "&OK",
    "Cancel": "Annuler",
    "Apply": "Appliquer",
    "Close": "Fermer",
    "Add": "Ajouter",
    "Delete": "Supprimer",
    "Edit": "Modifier",
    "Yes": "Oui",
    "No": "Non",
    "Properties": "Propriétés",
    "Dictionary": "Dictionnaire",
    "Keymap": "Table de touches",
    "Basics": "Bases",
    "Advanced": "Avancé",
    "Privacy": "Confidentialité",
    "Appearance": "Apparence",
    "Input mode": "Mode de saisie",
    "Keymap style": "Style de table de touches",
    "Customize...": "Personnaliser…",
    "Romaji table": "Table romaji",
    "About [ProductName]": "À propos de [ProductName]",
    "[ProductName] Settings": "Paramètres [ProductName]",
    "Dictionary Tool": "Outil dictionnaire",
    "Add Word": "Ajouter un mot",
    "Add a word": "Ajouter un mot",
    "Edit user dictionary...": "Modifier le dictionnaire utilisateur…",
    "product forum": "forum du produit",
    "marinaMoji": "marinaMoji",
    "Mozc": "Mozc",
    "Google Japanese Input": "Google Japanese Input",
    "Administration": "Administration",
    "A fatal error occurred.": "Une erreur fatale s'est produite.",
    "Do you want to enable [ProductName]?": "Voulez-vous activer [ProductName] ?",
}

# Longest-first phrase replacements within strings.
PHRASES = [
    ("Candidate window font size", "Taille de police de la fenêtre de candidats"),
    ("Adjust conversion based on previous input", "Ajuster la conversion selon la saisie précédente"),
    ("Always use Japanese keyboard layout for Japanese input",
     "Toujours utiliser la disposition clavier japonaise pour la saisie japonaise"),
    ("Always allocate conversion dictionary into physical memory",
     "Toujours allouer le dictionnaire de conversion en mémoire physique"),
    ("Emoticon conversion", "Conversion d'émoticônes"),
    ("Katakana to English conversion", "Conversion katakana vers anglais"),
    ("Show toolbar", "Afficher la barre d'outils"),
    ("Hide toolbar", "Masquer la barre d'outils"),
    ("Traditional kanji (Kyūjitai)", "Kanji traditionnels (kyūjitai)"),
    ("Odoriji (iteration marks)", "Odoriji (marques d'itération)"),
    ("Privacy mode", "Mode confidentialité"),
    ("Direct input", "Saisie directe"),
    ("Half width katakana", "Katakana demi-chasse"),
    ("Wide Latin", "Alphanumérique pleine chasse"),
    ("Input Mode", "Mode de saisie"),
    ("Half-width Katakana", "Katakana demi-chasse"),
    ("Full-width Roman", "Romain pleine chasse"),
    ("Half-width Roman", "Romain demi-chasse"),
    ("Direct Input", "Saisie directe"),
    ("open source software", "logiciels open source"),
    ("website", "site Web"),
    ("Settings", "Paramètres"),
    ("Preferences", "Préférences"),
    ("Toolbar", "Barre d'outils"),
    ("Hiragana", "Hiragana"),
    ("Katakana", "Katakana"),
    ("Latin", "Alphanumérique"),
    ("Alphanumeric", "Alphanumérique"),
    ("Alphabets", "Alphabets"),
    ("Composition", "Composition"),
    ("Conversion", "Conversion"),
    ("Dictionary", "Dictionnaire"),
    ("Import", "Importer"),
    ("Export", "Exporter"),
    ("Reading", "Lecture"),
    ("Category", "Catégorie"),
    ("Comment", "Commentaire"),
    ("Word", "Mot"),
    ("Words", "Mots"),
    ("User dictionary", "Dictionnaire utilisateur"),
    ("General", "Général"),
    ("Enable", "Activer"),
    ("Disable", "Désactiver"),
    ("Learn", "Apprendre"),
    ("Suggestion", "Suggestion"),
    ("Suggestions", "Suggestions"),
    ("Symbol", "Symbole"),
    ("Symbols", "Symboles"),
    ("Shortcut", "Raccourci"),
    ("Shortcuts", "Raccourcis"),
    ("Key", "Touche"),
    ("Keys", "Touches"),
    ("Function", "Fonction"),
    ("Script", "Script"),
    ("Privacy", "Confidentialité"),
    ("Usage statistics", "Statistiques d'utilisation"),
    ("crash reports", "rapports de plantage"),
    ("About", "À propos de"),
    ("Version", "Version"),
    ("Help", "Aide"),
    ("Tools", "Outils"),
    ("Mode", "Mode"),
    ("Reset", "Réinitialiser"),
    ("Default", "Par défaut"),
    ("Custom", "Personnalisé"),
    ("Customize", "Personnaliser"),
    ("Edit", "Modifier"),
    ("Delete", "Supprimer"),
    ("Add", "Ajouter"),
    ("Remove", "Retirer"),
    ("Save", "Enregistrer"),
    ("Load", "Charger"),
    ("Search", "Rechercher"),
    ("Type", "Type"),
    ("Size", "Taille"),
    ("Font", "Police"),
    ("Window", "Fenêtre"),
    ("Memory", "Mémoire"),
    ("Keyboard", "Clavier"),
    ("Japanese", "japonais"),
    ("English", "anglais"),
    ("Roman", "Romain"),
    ("roman", "romain"),
    ("input", "saisie"),
    ("Input", "Saisie"),
    ("conversion", "conversion"),
    ("dictionary", "dictionnaire"),
    ("toolbar", "barre d'outils"),
    ("candidate", "candidat"),
    ("preedit", "préédition"),
    ("learning", "apprentissage"),
    ("history", "historique"),
    ("privacy", "confidentialité"),
    ("automatically", "automatiquement"),
    ("automatic", "automatique"),
]
PHRASES.sort(key=lambda x: -len(x[0]))


_PLACEHOLDER_RE = re.compile(
    r"\[(?:ProductName|ProductUrl|ForumUrl|ForumName)\]"
)


def translate_to_fr(text: str, glossary: dict) -> str:
    if not text:
        return text
    if text in glossary:
        return glossary[text]
    if text in WHOLE_STRING:
        return WHOLE_STRING[text]
    if text in ("0", "1", "2", "!", "?", "0.0.0.0") or re.match(r"^[A-Z0-9 \\-]+$", text):
        return text

    placeholders = {}

    def _stash(m):
        key = f"__PH_{len(placeholders)}__"
        placeholders[key] = m.group(0)
        return key

    protected = _PLACEHOLDER_RE.sub(_stash, text)
    out = protected
    for en, fr in PHRASES:
        if en in out:
            out = out.replace(en, fr)
    for key, val in placeholders.items():
        out = out.replace(key, val)
    return out


def process_file(en_path: Path, glossary: dict) -> None:
    fr_path = en_path.with_name(en_path.name.replace("_en.qtts", "_fr.qtts"))
    tree = ET.parse(en_path)
    root = tree.getroot()
    root.set("language", "fr")
    for msg in root.iter("message"):
        src_el = msg.find("source")
        if src_el is None or not src_el.text:
            continue
        src = src_el.text
        tr_el = msg.find("translation")
        if tr_el is None:
            tr_el = ET.SubElement(msg, "translation")
        fr = translate_to_fr(src, glossary)
        tr_el.text = fr
        if "type" in tr_el.attrib:
            del tr_el.attrib["type"]
    tree.write(fr_path, encoding="utf-8", xml_declaration=True)
    print("wrote", fr_path)


def main():
    glossary = {}
    if GLOSSARY_PATH.exists():
        glossary = json.loads(GLOSSARY_PATH.read_text(encoding="utf-8"))
    for en_path in sorted(GUI_ROOT.rglob("*_en.qtts")):
        process_file(en_path, glossary)
    # Also handle tr_en if named differently
    tr_en = GUI_ROOT / "base" / "tr_en.qtts"
    if tr_en.exists():
        process_file(tr_en, glossary)


if __name__ == "__main__":
    main()
