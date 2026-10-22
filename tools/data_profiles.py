"""Plattformabhaengige Datenprofile fuer Pakete und App-Bundles.

Jedes Profil nennt die Teilbaeume von data/, die nicht ausgeliefert werden, und ob die
Runtime einen Mod-Ordner liest. Ausgeschlossen werden nur Quellen, die die Runtime nicht
liest (Belege im Kommentar); alles andere bleibt unveraendert.
"""
from pathlib import Path
import shutil

# files/MOVIE: Original-PSS, die Runtime spielt nur movies/*.rvv (native/movie.cpp:18).
# archives/RDX_LNK: Raumarchiv, ungelesen sobald rooms/ existiert (native/room.cpp:65).
_COOKED_SOURCES = ('files/MOVIE', 'archives/RDX_LNK')

PROFILES = {
    'desktop': {'exclude': (), 'mods': True},
    'ios': {'exclude': _COOKED_SOURCES, 'mods': True},
    'tvos': {'exclude': _COOKED_SOURCES, 'mods': False},
}
TARGET_PROFILE = {'macos': 'desktop', 'windows': 'desktop', 'windows-arm64': 'desktop', 'linux': 'desktop',
                  'ios': 'ios', 'ios-simulator': 'ios'}


def profile(target: str) -> dict:
    return PROFILES[TARGET_PROFILE.get(target, target)]


def copy_data(source: Path, destination: Path, target: str) -> None:
    excluded = {source / part for part in profile(target)['exclude']}
    shutil.copytree(source, destination, ignore=lambda folder, names: [n for n in names if Path(folder) / n in excluded])
