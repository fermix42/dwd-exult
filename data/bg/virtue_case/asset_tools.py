"""PNG / BG shape conversion. Python 3 standard library; no artwork generation.
CLI: python asset_tools.py --input incoming --output converted [--quantize]
PNG: 8-bit indexed, RGB or RGBA, noninterlaced. No resizing or dithering.
"""
import argparse, hashlib, json, struct, zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent
def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))

def write_png(path, w, h, pixels, palette=None):
    channels = 1 if palette is not None else 4
    header = struct.pack('>IIBBBBB', w, h, 8, 3 if palette is not None else 6, 0, 0, 0)
    data = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header)
    if palette is not None:
        data += chunk(b'PLTE', bytes(palette)) + chunk(b'tRNS', bytes([255]*255 + [0]))
    rows = b''.join(b'\0' + bytes(pixels[y*w*channels:(y+1)*w*channels]) for y in range(h))
    path = Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b''))

def read_png(path):
    data = Path(path).read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n': raise ValueError('Not PNG')
    pos = 8; packed = b''; palette = None; alpha = b''
    while pos < len(data):
        n = struct.unpack_from('>I', data, pos)[0]; kind = data[pos+4:pos+8]; body = data[pos+8:pos+8+n]
        if zlib.crc32(kind+body) != struct.unpack_from('>I', data, pos+8+n)[0]: raise ValueError('PNG CRC')
        pos += n+12
        if kind == b'IHDR': w,h,depth,mode,comp,filt,interlace = struct.unpack('>IIBBBBB',body)
        elif kind == b'PLTE': palette = body
        elif kind == b'tRNS': alpha = body
        elif kind == b'IDAT': packed += body
    if depth != 8 or mode not in (2,3,6) or interlace or comp or filt:
        raise ValueError('Require noninterlaced 8-bit indexed/RGB/RGBA PNG')
    if mode == 2 and alpha: raise ValueError('RGB tRNS unsupported: export RGBA instead')
    stride = {2:3,3:1,6:4}[mode]; rowbytes = w*stride; raw = zlib.decompress(packed)
    if len(raw) != h*(rowbytes+1): raise ValueError('PNG decompressed length')
    pixels = bytearray(); prev = bytearray(rowbytes)
    for y in range(h):
        off=y*(rowbytes+1); f=raw[off]; row=bytearray(raw[off+1:off+1+rowbytes])
        if f > 4: raise ValueError('PNG filter')
        for x in range(rowbytes):
            a=row[x-stride] if x>=stride else 0; b=prev[x]; c=prev[x-stride] if x>=stride else 0
            if f == 1: val=a
            elif f == 2: val=b
            elif f == 3: val=(a+b)//2
            elif f == 4:
                p=a+b-c; ds=[abs(p-a),abs(p-b),abs(p-c)]; val=(a,b,c)[ds.index(min(ds))]
            else: val=0
            row[x]=(row[x]+val)&255
        pixels.extend(row); prev=row
    return w,h,mode,pixels,palette,alpha

def indexed(path, bg, quantize=False):
    w,h,mode,pixels,pal,alpha=read_png(path); result=bytearray(); converted=0
    colors=[tuple(bg[i:i+3]) for i in range(0,768,3)]
    lookup={}
    for i in range(223,-1,-1): lookup[colors[i]]=i
    for k in range(w*h):
        if mode == 3:
            v=pixels[k]; opacity=alpha[v] if v<len(alpha) else 255; rgb=tuple(pal[v*3:v*3+3])
        else:
            step=4 if mode==6 else 3; rgb=tuple(pixels[k*step:k*step+3]); opacity=pixels[k*step+3] if step==4 else 255
        if opacity == 0: result.append(255); continue
        if opacity != 255: raise ValueError(f'{path}: partial alpha at pixel {k}')
        if mode == 3 and pal == bg:
            if v >= 224: raise ValueError(f'{path}: opaque reserved index {v}')
            result.append(v); continue
        if rgb in lookup: result.append(lookup[rgb]); continue
        if not quantize: raise ValueError(f'{path}: non-BG color {rgb}; index first, or explicitly --quantize')
        result.append(min(range(224), key=lambda i:sum((colors[i][c]-rgb[c])**2 for c in range(3)))); converted+=1
    return w,h,result,converted

def flex_entry(path,index):
    data=Path(path).read_bytes(); off,n=struct.unpack_from('<II',data,128+8*index)
    if not n: raise ValueError(f'Empty FLEX entry {index}')
    return data[off:off+n]

def decode_shape(data):
    size,first=struct.unpack_from('<II',data); assert size==len(data)
    count=(first-4)//4; offsets=struct.unpack_from('<'+'I'*count,data,4); frames=[]
    for off in offsets:
        right,left,above,below=struct.unpack_from('<hhhh',data,off); w=left+right+1; h=above+below+1
        pixels=bytearray([255])*(w*h); pos=off+8
        while True:
            scan=struct.unpack_from('<H',data,pos)[0]; pos+=2
            if scan==0: break
            x,y=struct.unpack_from('<hh',data,pos); pos+=4; n=scan>>1; line=bytearray()
            if not scan&1: line.extend(data[pos:pos+n]); pos+=n
            else:
                while len(line)<n:
                    run=data[pos]; pos+=1; size=run>>1
                    if run&1: line.extend([data[pos]]*size); pos+=1
                    else: line.extend(data[pos:pos+size]); pos+=size
            start=(y+above)*w+x+left
            assert 0<=start and start+len(line)<=len(pixels) and len(line)==n
            pixels[start:start+n]=line
        frames.append(dict(width=w,height=h,xleft=left,xright=right,yabove=above,ybelow=below,pixels=pixels))
    return frames

def encode_shape(frames):
    encoded=[]
    for w,h,left,above,pixels in frames:
        data=struct.pack('<hhhh',w-left-1,left,above,h-above-1)
        for y in range(h):
            x=0
            while x<w:
                while x<w and pixels[y*w+x]==255: x+=1
                start=x
                while x<w and pixels[y*w+x]!=255: x+=1
                if x>start: data+=struct.pack('<Hhh',(x-start)<<1,start-left,y-above)+bytes(pixels[y*w+start:y*w+x])
        encoded.append(data+b'\0\0')
    off=4+4*len(encoded); offsets=[]
    for data in encoded: offsets.append(off); off+=len(data)
    return struct.pack('<I',off)+struct.pack('<'+'I'*len(offsets),*offsets)+b''.join(encoded)

def main():
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('--input',type=Path,required=True); p.add_argument('--output',type=Path,required=True); p.add_argument('--quantize',action='store_true'); args=p.parse_args()
    manifest=json.loads((ROOT/'asset_manifest.json').read_text()); bg=(ROOT/'bg_palette_rgb8.bin').read_bytes(); frames=[]; report=[]
    for a in manifest['assets']:
        path=args.input/a['filename']; w,h,pixels,changed=indexed(path,bg,args.quantize)
        if [w,h]!=[a['width'],a['height']]: raise ValueError(f'{path}: wrong dimensions {w}x{h}')
        if 255 not in pixels: pass  # Fully opaque backgrounds are allowed.
        ox,oy=a['rendering_origin']; frames.append((w,h,ox,oy,pixels)); report.append({'asset':a['name'],'quantized_pixels':changed,'sha256_png':hashlib.sha256(path.read_bytes()).hexdigest()})
    blob=encode_shape(frames); decoded=decode_shape(blob)
    for frame,expected in zip(decoded,frames):
        assert (frame['width'],frame['height'],frame['xleft'],frame['yabove'],frame['pixels'])==expected
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'virtue_case.shp').write_bytes(blob)
    (args.output/'closed_case.shp').write_bytes(encode_shape(frames[:1]))
    (args.output/'conversion_report.json').write_text(json.dumps({'roundtrip':'PASS','frames':report},indent=2))
    print(f'PASS: {len(frames)} frames; exact dimensions/origins/pixels round-trip. Output: {args.output}')
if __name__=='__main__': main()
