"""Remaining native case failure/target edges, using real spell Usecode."""
import argparse,json,shutil,subprocess,sys,time,traceback
from pathlib import Path
import xml.etree.ElementTree as ET
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'test_cli'))
from driver import BASE,Driver
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--run',required=True);p.add_argument('--mod',default='',choices=('','Keyring'));p.add_argument('--checkpoint',type=Path,required=True);p.add_argument('--scale',type=int,choices=[1,2],default=2);a=p.parse_args();d=Driver();records=[];old=subprocess.Popen
def launch(args,**kw):
    if '--test-cli'in args:
        cfg=Path(args[args.index('-c')+1]);tree=ET.parse(cfg);v=tree.getroot().find('video')
        for path,val in {'display/width':640*a.scale,'display/height':400*a.scale,'game/width':640,'game/height':400,'ui/width':640,'ui/height':400,'ui/fill_mode':'Fit','ui/scale_method':'Point','ui/fill_scaler':'Point','scale':a.scale}.items():
            el=v
            for part in path.split('/'):
                child=el.find(part)
                if child is None:child=ET.SubElement(el,part)
                el=child
            el.text=str(val)
        tree.write(cfg,encoding='unicode')
    return old(args,**kw)
def do(cmd,**kw):
    c=dict(cmd=cmd,**kw);r=d.execute(c);records.append(dict(command=c,response=r));return r
def check(name,v):
    d.assertions+=1;d.failures+=not v;records.append(dict(check=name,passed=bool(v)));print(name,bool(v),flush=True)
    if not v:raise AssertionError(name)
def inv(ref=0):return do('inspect_inventory',ref=ref)['objects']
def obj(ref):return do('inspect_object',ref=ref)['object']
def find(ref=0):
    for o in inv(ref):
        if o['shape']==1601:return o['ref']
        if o['class_id']==6:
            r=find(o['ref'])
            if r:return r
def close():
    while d.state()['input_owner']=='gump':do('dismiss')
def show(ref):
    owner=obj(ref)['owner'];avatar=do('inspect_world',radius=2)['avatar']
    if owner and owner!=avatar:show(owner)
    do('interact_inventory',ref=ref)
def open_case():close();do('key',key='1');show(case);do('move_gump',ref=case,x=310,y=5)

def capture(name):
    do('screenshot',full=True);shutil.copy2(d.run/'screenshot.png',d.run/(name+'.png'))
def onto_table(ref,table,home):
    fp=table['footprint'];lift=table['tile']['z']+table['height']
    # Native dragging retains the picked opaque pixel's offset; try legal surface
    # pixels and verify the resulting real tile, never assign an object's position.
    for x in range(fp['x']-3,fp['x']+fp['w']+1):
        for y in range(fp['y']-3,fp['y']+fp['h']+1):
            try:
                do('drop_item',ref=ref,x=x,y=y,z=lift);actual=obj(ref)
                if actual['owner']==0:
                    at=actual['tile']
                    if at['z']==lift and fp['x']<=at['x']<fp['x']+fp['w'] and fp['y']<=at['y']<fp['y']+fp['h']:return True
                    do('transfer_item',ref=ref,destination=home)
            except Exception:pass
    return False

try:
    keys=BASE/(a.run+'.keys');keys.parent.mkdir(parents=True,exist_ok=True);keys.write_text('1 INVENTORY 0\n')
    subprocess.Popen=launch;do('init',game='bg',mod=a.mod,run=a.run,keys_path=str(keys),checkpoint=str(a.checkpoint.resolve()));subprocess.Popen=old
    do('load',slot=5);case=find();before=obj(case);home=before['owner'];check('inventory uses original closed shape1601 frame0',before['shape']==1601 and before['frame']==0);check('inventory native sprite is14x10 origin7,9',before['sprite']==dict(width=14,height=10,xleft=7,yabove=9))
    do('key',key='1');show(home);do('move_gump',ref=home,x=30,y=220);avatar=do('inspect_world',radius=2)['avatar'];do('move_gump',ref=avatar,x=30,y=30);capture('backpack-native-sprite');do('save',slot=8);at=d.state();dropped=False
    for dx,dy in [(3,2),(4,2),(5,2),(-3,2),(3,4),(-4,4)]:
        try:
            dest=dict(x=at['x']+dx,y=at['y']+dy,z=at['z'])
            if do('inspect_tile',**dest)['blocked']:continue
            do('drop_item',ref=case,**dest)
            if obj(case)['owner']==0:dropped=True;break
        except Exception:pass
    check('native drag places the real reward in world',dropped);close();ground=obj(case);placed_tile=ground['tile'];check('world uses same shape frame dimensions and origin',ground['shape']==before['shape'] and ground['frame']==before['frame'] and ground['sprite']==before['sprite']);check('one-tile collision footprint remains intact',ground['footprint']['w']==1 and ground['footprint']['h']==1 and ground['height']==1);capture('world-native-sprite')
    do('interact_object',ref=case);check('world hit opens accepted case gump',any('Case_gump'in g['type']for g in d.state()['gumps']));close();do('save',slot=6);do('load',slot=6);ground=do('find_objects',shape=1601,radius=16)['objects'][0];case=ground['ref'];check('world position and sprite survive reload',ground['tile']==placed_tile and ground['sprite']==before['sprite']);check('contents and compartment notes remain intact',ground['case_labels']==before['case_labels']);home=next(o['ref']for o in inv()if o['shape']==801 and o['class_id']==6);do('key',key='1');avatar=do('inspect_world',radius=2)['avatar'];do('move_gump',ref=avatar,x=30,y=5);show(home);do('move_gump',ref=home,x=30,y=220);do('transfer_item',ref=case,destination=home);check('world reward can return to ordinary backpack',obj(case)['owner']==home and obj(case)['sprite']==before['sprite']and not inv(case));capture('picked-up-sprite')
    tables=do('find_objects',shape=633,radius=32)['objects'];table=next(t for t in tables if t['tile']==dict(x=1592,y=370,z=0));records.append(dict(studio_table=table))
    check('small real case rests on accepted studio support surface',onto_table(case,table,home))
    check('case spans at most half the table sprite width',obj(case)['sprite']['width']*2<=table['sprite']['width']);close();capture('small-case-on-studio-table');do('save',slot=7)
    do('interact_object',ref=case);check('small tabletop hit opens unchanged196x135 case',any('Case_gump'in g['type']for g in d.state()['gumps']));close();do('key',key='1');show(home);do('move_gump',ref=home,x=30,y=220);do('transfer_item',ref=case,destination=home);check('small tabletop object picks up into backpack',obj(case)['owner']==home)
    placed=False
    for t in do('find_objects',shape=890,radius=16)['objects']+do('find_objects',shape=333,radius=16)['objects']+tables:
        if t['ref']==table['ref']:continue
        try:
            close();fp=t['footprint'];plans=[]
            for x in range(fp['x']-3,fp['x']+fp['w']+3):
                for y in range(fp['y']-3,fp['y']+fp['h']+3):
                    if fp['x']<=x<fp['x']+fp['w'] and fp['y']<=y<fp['y']+fp['h']:continue
                    path=do('path_query',x=x,y=y,z=0)
                    if path['found']and not path['blocked_destination']:plans.append((path['steps'],x,y))
            if not plans:continue
            steps,x,y=min(plans);do('move_to',x=x,y=y,z=0,timeout=30);end=time.monotonic()+32
            while time.monotonic()<end and do('movement_status')['movement']=='moving':time.sleep(.05)
            if do('movement_status')['movement']!='reached':continue
            do('key',key='1');show(home);do('move_gump',ref=home,x=30,y=220)
            if onto_table(case,t,home):placed=True;break
        except Exception:pass
    check('ordinary drag places real case on another suitable surface',placed);close();capture('small-case-on-other-surface');do('interact_object',ref=case);check('other surface case opens normally',any('Case_gump'in g['type']for g in d.state()['gumps']));close();do('exit')
except Exception:
    d.failures+=1;records.append(dict(error=traceback.format_exc()));print(traceback.format_exc(),flush=True)
finally:
    subprocess.Popen=old
    if d.run:(d.run/'sprite-checks.json').write_text(json.dumps(records,indent=2))
    result=d.close();print(json.dumps(result),flush=True)
sys.exit(0 if result and result['result']=='PASS'else 1)
