"""Keep world and inventory pixels equal in the private human review profiles.

The accepted closed sprite is already shared, 28x20 at origin14,19. The old
review's 320x200 world and 640x400 inventory layer displayed it at different
screen scales. This adjusts only explicitly selected disposable review configs.
"""
import argparse
from pathlib import Path
import xml.etree.ElementTree as ET
from xml.sax.saxutils import escape

def configure(root):
    root=Path(root).resolve()
    for profile in ['bg','keyring']:
        path=root/('exult-'+profile+'.cfg')
        tree=ET.parse(path);video=tree.getroot().find('video')
        for axis in ['width','height']:
            video.find('game/'+axis).text=video.find('ui/'+axis).text.strip()
        # Preserve the engine's native multiline configuration format.
        def lines(element):
            result=['<'+element.tag+'>']
            if element.text and element.text.strip():result.append(escape(element.text.strip()))
            for child in element:result.extend(lines(child))
            result.append('</'+element.tag+'>')
            return result
        path.write_text('\n'.join(lines(tree.getroot()))+'\n',encoding='utf-8')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True)
    configure(p.parse_args().root)
