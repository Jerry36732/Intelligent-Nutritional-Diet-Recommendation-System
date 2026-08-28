from pathlib import Path
import pypdfium2 as pdfium
from PIL import Image, ImageDraw

root = Path(r"C:\Users\ROG\Documents\System\食谱数据\rendered_word")
pdf_path = root / "食谱数据手册.pdf"
pages_dir = root / "pages"
contacts_dir = root / "contacts"
pages_dir.mkdir(parents=True, exist_ok=True)
contacts_dir.mkdir(parents=True, exist_ok=True)

doc = pdfium.PdfDocument(str(pdf_path))
thumbs = []
for i, page in enumerate(doc):
    bitmap = page.render(scale=1.35)
    rendered = bitmap.to_pil().convert("RGB")
    out = pages_dir / f"page-{i+1:03d}.png"
    rendered.save(out)
    image = rendered.copy()
    image.thumbnail((255, 330))
    thumbs.append((i + 1, image.copy()))

for group_start in range(0, len(thumbs), 12):
    group = thumbs[group_start:group_start + 12]
    sheet = Image.new("RGB", (4 * 275, 3 * 360), "#d8dde5")
    draw = ImageDraw.Draw(sheet)
    for idx, (page_no, thumb) in enumerate(group):
        x = (idx % 4) * 275 + 10
        y = (idx // 4) * 360 + 22
        sheet.paste(thumb, (x, y))
        draw.text((x, 4 + (idx // 4) * 360), f"Page {page_no}", fill="black")
    sheet.save(contacts_dir / f"contact-{group_start//12+1:02d}.png")

print({"pages": len(doc), "contacts": (len(thumbs) + 11) // 12})
