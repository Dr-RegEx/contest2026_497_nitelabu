"""Render the official board schematic using an already installed PDF reader."""
import importlib.util
from pathlib import Path
import urllib.request

available = {name: importlib.util.find_spec(name) is not None
             for name in ("fitz", "pypdf", "pypdfium2")}
print(available, flush=True)
if not available["pypdfium2"]:
    raise SystemExit("Existing PDFium unavailable; no dependencies installed")
import pypdfium2

base = Path(__file__).resolve().parent
target = base / "schematic331-audio.png"
if target.exists():
    raise SystemExit("Render already exists")
url = "https://dl.espressif.com/schematics/esp32-s31-function-coreboard-1-schematics.pdf"
with urllib.request.urlopen(url, timeout=30) as response:
    data = response.read(5 * 1024 * 1024)
document = pypdfium2.PdfDocument(data)
page = document[2]
page.render(scale=2).to_pil().save(str(target))
print(target, flush=True)
