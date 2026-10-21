"""Locate FFmpeg tools on PATH or in the local Windows build-tools directory."""
from pathlib import Path
import platform
import shutil

ROOT = Path(__file__).resolve().parents[1]


def find(name, root=ROOT):
    """Keep explicit paths and PATH precedence; only default names get a fallback."""
    if located := shutil.which(name):
        return located
    if platform.system() == 'Windows' and name in ('ffmpeg', 'ffprobe', 'ffmpeg.exe', 'ffprobe.exe'):
        filename = name if name.endswith('.exe') else name + '.exe'
        candidate = Path(root) / 'build-tools' / filename
        if candidate.is_file():
            return str(candidate.resolve())
    return None
