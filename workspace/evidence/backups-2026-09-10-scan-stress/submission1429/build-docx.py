"""Build a simple three-page Word document using standard OOXML."""
from pathlib import Path
from xml.sax.saxutils import escape
from zipfile import ZipFile, ZIP_DEFLATED
import xml.etree.ElementTree as ET

D = Path(__file__).resolve().parent
paragraphs = []
for line in (D / 'project-introduction.md').read_text().splitlines():
    if not line.strip():
        continue
    if line == '---PAGE---':
        paragraphs.append('<w:p><w:r><w:br w:type="page"/></w:r></w:p>')
        continue
    heading = line.startswith('#')
    size = '34' if line.startswith('# ') else ('26' if heading else '21')
    text = line.lstrip('# ') if heading else line
    bold = '<w:b/>' if heading else ''
    paragraphs.append(
        '<w:p><w:pPr><w:spacing w:after="130" w:line="290" w:lineRule="auto"/>'
        + ('<w:keepNext/>' if heading else '') + '</w:pPr><w:r><w:rPr>'
        '<w:rFonts w:ascii="Calibri" w:eastAsia="Microsoft YaHei"/>'
        + bold + '<w:sz w:val="' + size + '"/></w:rPr><w:t xml:space="preserve">'
        + escape(text) + '</w:t></w:r></w:p>')
ns = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
parts = {
    '[Content_Types].xml': '<?xml version="1.0" encoding="UTF-8"?>'
    '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
    '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
    '<Default Extension="xml" ContentType="application/xml"/>'
    '<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>'
    '</Types>',
    '_rels/.rels': '<?xml version="1.0" encoding="UTF-8"?>'
    '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
    '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>'
    '</Relationships>',
    'word/document.xml': '<?xml version="1.0" encoding="UTF-8"?>'
    '<w:document xmlns:w="' + ns + '"><w:body>' + ''.join(paragraphs)
    + '<w:sectPr><w:pgSz w:w="11906" w:h="16838"/>'
    '<w:pgMar w:top="1000" w:right="1100" w:bottom="1000" w:left="1100"/>'
    '</w:sectPr></w:body></w:document>'
}
output = D / 'ESP32-S31-openvela-project-introduction-draft.docx'
with ZipFile(output, 'w', ZIP_DEFLATED) as archive:
    for name, xml in parts.items():
        ET.fromstring(xml)
        archive.writestr(name, xml)
with ZipFile(output) as archive:
    assert archive.testzip() is None
    root = ET.fromstring(archive.read('word/document.xml'))
    texts = root.findall('.//{' + ns + '}t')
    assert len(texts) == len([l for l in (D / 'project-introduction.md').read_text().splitlines()
                              if l.strip() and l != '---PAGE---'])
print('DOCX ZIP/XML and paragraph content checks PASS; visual layout not yet reviewed')
