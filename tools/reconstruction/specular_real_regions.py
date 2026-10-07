"""Explicit manually inspected source-coordinate protection ROIs, not model semantic labels."""
import json
from pathlib import Path
from PIL import Image,ImageDraw
root=Path('generated/prompt25-fixtures')
regions={'mirror_glass_unknown':[255,120,314,169],'lamp_metal_unknown':[330,28,372,77],
         'screen_emission':[447,39,512,134],'painted_door_comparison':[321,116,377,211]}
image=Image.new('L',(512,341),0);draw=ImageDraw.Draw(image)
for name,rect in regions.items():
    if name!='painted_door_comparison':draw.rectangle((rect[0],rect[1],rect[2]-1,rect[3]-1),fill=255)
image.save(root/'real-protection.png')
(root/'real-regions.json').write_text(json.dumps(dict(source='generated/scene24-final-indoor',coordinate='canonical RGB8 pixel edges, manually selected',regions=regions),indent=2),encoding='utf-8')
