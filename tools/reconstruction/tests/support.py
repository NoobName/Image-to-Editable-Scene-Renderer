from pathlib import Path
from PIL import Image, ImageDraw


def make_input(path: Path, size=(320, 240)) -> Path:
    """Asymmetric test image makes UV flipping/mirroring and aspect distortion visible."""
    width, height = size
    image = Image.new("RGB", size, (32, 38, 50))
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, width//2-1, height//2-1), fill=(220, 50, 40))
    draw.rectangle((width//2, 0, width-1, height//2-1), fill=(45, 180, 80))
    draw.rectangle((0, height//2, width//2-1, height-1), fill=(40, 95, 220))
    draw.rectangle((width//2, height//2, width-1, height-1), fill=(240, 200, 40))
    draw.text((12, 12), "TOP LEFT / RED", fill="white")
    draw.text((width//2+12, 12), "TOP RIGHT / GREEN", fill="white")
    draw.text((12, height-24), "BOTTOM LEFT / BLUE", fill="white")
    draw.text((width//2+12, height-24), "BOTTOM RIGHT", fill="black")
    radius = min(width, height)//5
    draw.ellipse((width//2-radius, height//2-radius, width//2+radius, height//2+radius), fill=(235, 235, 235), outline="black", width=3)
    draw.line((width//2, height//2+radius//2, width//2, height//2-radius//2), fill="black", width=5)
    draw.polygon(((width//2, height//2-radius//2-8), (width//2-9, height//2-radius//2+8),
                  (width//2+9, height//2-radius//2+8)), fill="black")
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        raise FileExistsError(f"Test input already exists: {path}")
    image.save(path, quality=95)
    return path
