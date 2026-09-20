"""Render selected source-PDF pages read-only using the existing Windows PDFium."""
from pathlib import Path
import sys
import pypdfium2 as pdfium

output = Path(r'\\wsl.localhost\Ubuntu-22.04\home\regex\work\esp32s31-openvela\backups\2026-09-10-scan-stress\pdf-review')
output.mkdir(exist_ok=True)
source = Path(r'D:\Users\tttgu\Downloads\Documents\esp32-s31-wroom-3_datasheet_cn.pdf')
prefix = 'module'
numbers = (2, 3, 39, 40, 54, 55)
if len(sys.argv) > 1 and sys.argv[1] == 'chip':
    source = Path(r'D:\Users\tttgu\Downloads\Documents\esp32-s31_datasheet_cn.pdf')
    prefix = 'chip'
    numbers = (2, 3)
with pdfium.PdfDocument(source) as document:
    for number in numbers:
        target = output / f'{prefix}-page-{number}.png'
        if target.exists():
            raise FileExistsError(target)
        page = document[number - 1]
        bitmap = page.render(scale=2)
        bitmap.to_pil().save(target)
        bitmap.close()
        page.close()
        print(target)
