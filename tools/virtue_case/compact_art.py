"""Deterministic import of the owner's compact case image, no generated artwork.

Run from repository root: python tools/virtue_case/compact_art.py --source IMAGE
Original closed art is retained; source image is sampled at196x135 and BG indexed.
"""
import argparse
from collections import deque
import json
from pathlib import Path
import shutil
import sys

repo = Path(__file__).resolve().parents[2]
art = repo/'data/bg/virtue_case'
sys.path.insert(0, str(art))
from asset_tools import read_png, write_png, indexed, encode_shape, decode_shape

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', type=Path, required=True)
a = p.parse_args()
palette = (art/'bg_palette_rgb8.bin').read_bytes()
w,h,mode,pixels,_,_ = read_png(a.source)
if mode not in (2,6): raise ValueError('Source must be RGB/RGBA8 PNG')
channels = 3 if mode==2 else 4
width,height = 196,135
colors = [tuple(palette[i*3:i*3+3]) for i in range(224)]
sample = []
for y in range(height):
    for x in range(width):
        sx = min(w-1,int((x+.5)*w/width)); sy = min(h-1,int((y+.5)*h/height))
        off=(sy*w+sx)*channels; sample.append(tuple(pixels[off:off+3]))
# Only remove perimeter-connected near-black background; internal dark velvet
# and outlines remain opaque. No removal by palette index0 globally.
outside=set(); queue=deque()
for y in range(height): queue.extend([y*width,y*width+width-1])
queue.extend(range(width)); queue.extend(range((height-1)*width,height*width))
while queue:
    pos=queue.popleft()
    if pos in outside or max(sample[pos])>20: continue
    outside.add(pos); x=pos%width; y=pos//width
    if x: queue.append(pos-1)
    if x+1<width: queue.append(pos+1)
    if y: queue.append(pos-width)
    if y+1<height: queue.append(pos+width)
cache={}; bg=bytearray()
for i,rgb in enumerate(sample):
    if i in outside: bg.append(255); continue
    if rgb not in cache: cache[rgb]=min(range(224),key=lambda k:sum((colors[k][c]-rgb[c])**2 for c in range(3)))
    bg.append(cache[rgb])
write_png(art/'open_case.png',width,height,bg,palette)
source_path=art/'source_case_reference.png'
if a.source.resolve()!=source_path.resolve(): shutil.copyfile(a.source,source_path)
manifest=json.loads((art/'asset_manifest.json').read_text())
for asset in manifest['assets']:
    frame=asset['packed_frame']
    if frame==1: asset.update(width=196,height=135)
    elif 2<=frame<10:
        # Read original approved art from the previous production revision,
        # not an already downsized canvas, so repeated import stays lossless.
        import subprocess
        original=subprocess.check_output(['git','show','da74f5607:data/bg/virtue_case/'+asset['filename']],cwd=repo)
        # Decode the PNG in a workspace-owned temporary file.
        tmp=art/'.stone_import.png'; tmp.write_bytes(original)
        sw,sh,indices,_=indexed(tmp,palette); tmp.unlink()
        dest=bytearray(indices[min(sh-1,int((y+.5)*sh/15))*sw+min(sw-1,int((x+.5)*sw/13))] for y in range(15) for x in range(13))
        write_png(art/asset['filename'],13,15,dest,palette)
        i=frame-2; anchor=[44+71*(i//4),52+18*(i%4)]
        asset.update(width=13,height=15,rendering_origin=[6,7],position_relative_to_gump=anchor)
    elif frame>=10:
        x,y=(38,14) if frame<12 else (130,14)
        normal=bytearray()
        for row in range(12): normal.extend(bg[(y+row)*width+x:(y+row)*width+x+32])
        pressed=bytearray(normal)
        # One native pixel of depression with a dark top edge.
        if frame%2:
            pressed[32:]=normal[:-32]; pressed[:32]=bytes([147])*32
        write_png(art/asset['filename'],32,12,pressed,palette)
        asset.update(width=32,height=12,position_relative_to_gump=[x,y])
manifest.update(canvas=[196,135],status='Compact196x135 owner image integration; supersedes320x200 layout',lid=[14,8,168,22],wood_border=3,velvet=[26,41,148,75],compass_safe=[74,10,48,18],hinges=[[40,30,18,10],[141,30,18,10]],button_hits={'mark':[38,14,32,12],'recall':[130,14,32,12]},button_text={'mark_normal':[43,17],'mark_pressed':[43,18],'recall_normal':[133,17],'recall_pressed':[133,18]},closing_catch_and_hit=[86,118,28,12],source_commit='da74f56074b6ae4c1c509ec3ab76a924c3e261dc')
for i,s in enumerate(manifest['slots']):
    c=i//4; r=i%4
    s.update(compartment=[30+71*c,42+18*r,67,18],recess=[33+71*c,44+18*r,23,16],stone_canvas=[38+71*c,45+18*r,13,15],stone_hotspot=[6,7],stone_render_anchor=[44+71*c,52+18*r],label_rect=[59+71*c,46+18*r,36,12],label_text_origin=[61+71*c,49+18*r],label_text_safe_rect=[61+71*c,49+18*r,32,6],label_hit=[59+71*c,46+18*r,36,12])
manifest['label_rules'].update(font='TINY_BLACK_FONT, resource shape4, lead0',text_height=6,baseline=4,max_width=32,recommended_character_cap=6)
(art/'asset_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
frames=[]
for asset in manifest['assets']:
    sw,sh,indices,n=indexed(art/asset['filename'],palette)
    assert [sw,sh]==[asset['width'],asset['height']] and n==0
    frames.append((sw,sh,*asset['rendering_origin'],indices))
blob=encode_shape(frames)
for expected,decoded in zip(frames,decode_shape(blob)):
    assert expected==(decoded['width'],decoded['height'],decoded['xleft'],decoded['yabove'],decoded['pixels'])
(repo/'data/bg/virtue_case.shp').write_bytes(blob)
print('PASS14 frames:196x135 background,13x15 seated stones,32x12 buttons; exact RLE roundtrip')
