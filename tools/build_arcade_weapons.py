#!/usr/bin/env python3
"""Reproducible, exterior-only arcade game props; no real weapon components.

Machine gun: original interpretation of the Crisis Zone cabinet silhouette.
Shotgun: original orange arcade-styled game prop, not an original Namco asset.
Handgun: selective colour variant of the existing attributed CC-BY Namco mesh.
No network, generators or third-party mesh libraries. Source assets stay intact.
"""
from __future__ import annotations
import argparse, colorsys, hashlib, json, math, struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PALETTE = {'machine_gun': (55,188,199), 'handgun': (237,107,166), 'shotgun': (255,138,41)}
DARK = (32,41,47)
METAL = (86,102,110)
PISTOL_SHA = '4a8231dcc6ba717d61ab5783156c3a0d0367572ddb54537b0b283415da12fab8'

def shade(col, factor):
    return tuple(max(0, min(255, round(c*factor))) for c in col)

def sub(a,b): return tuple(x-y for x,y in zip(a,b))
def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def unit(v):
    n=math.sqrt(sum(x*x for x in v))
    if n<1e-12: raise ValueError('Degenerate face')
    return tuple(x/n for x in v)

class Mesh:
    def __init__(self): self.vertices=[]; self.indices=[]
    def face(self, pts, colour, part=0, normals=None):
        normal=unit(cross(sub(pts[1],pts[0]),sub(pts[2],pts[0])))
        start=len(self.vertices)
        for k,p in enumerate(pts):
            self.vertices.append((*p,*(normals[k] if normals else normal),*(x/255 for x in colour),part))
        for k in range(1,len(pts)-1): self.indices.extend((start,start+k,start+k+1))
    def prism(self, poly, width, colour, bevel=.003, part=0, xcentre=0):
        """Bevelled convex side profile (z,y). Flat centres, distinct bevel normals."""
        poly=list(poly)
        area=sum(poly[i][0]*poly[(i+1)%len(poly)][1]-poly[(i+1)%len(poly)][0]*poly[i][1] for i in range(len(poly)))
        if area<0:poly.reverse()
        inset=[]
        for i,(z,y) in enumerate(poly):
            prev=poly[i-1];nxt=poly[(i+1)%len(poly)]
            d1=(z-prev[0],y-prev[1]);d2=(nxt[0]-z,nxt[1]-y)
            l1=math.hypot(*d1);l2=math.hypot(*d2)
            n1=(-d1[1]/l1,d1[0]/l1);n2=(-d2[1]/l2,d2[0]/l2)
            den=1+n1[0]*n2[0]+n1[1]*n2[1]
            if den<.05:raise ValueError('Acute bevel')
            inset.append((z+bevel*(n1[0]+n2[0])/den,y+bevel*(n1[1]+n2[1])/den))
        # Four rings: thin side faces, bevel strips, wide centre sidewalls.
        rings=[[(xcentre+x,y,z) for z,y in pp] for x,pp in [(-width/2,inset),(-width/2+bevel,poly),(width/2-bevel,poly),(width/2,inset)]]
        self.face(list(reversed(rings[0])),colour,part)
        self.face(rings[-1],colour,part)
        for a,b in zip(rings,rings[1:]):
            for i in range(len(poly)):
                j=(i+1)%len(poly)
                self.face([a[i],a[j],b[j],b[i]],colour,part)
    def box(self, centre, size, colour, bevel=.0015, part=0):
        x,y,z=centre;w,h,l=size
        self.prism([(z-l/2,y-h/2),(z+l/2,y-h/2),(z+l/2,y+h/2),(z-l/2,y+h/2)],w,colour,min(bevel,min(size)*.20),part,x)
    def rounded_box(self, centre, size, colour, radius=.006):
        """Smooth moulded shell; a rounded cube surface with analytic normals."""
        half=[x/2 for x in size];r=min(radius,min(half)*.8)
        grid=[[-h,-h+r*.25,-h+r*.65,-h+r,h-r,h-r*.65,h-r*.25,h] for h in half]
        core=[h-r for h in half]
        for axis in range(3):
            other=[i for i in range(3) if i!=axis]
            for sign in (-1,1):
                def point(a,b):
                    q=[0.,0.,0.];q[axis]=sign*half[axis];q[other[0]]=a;q[other[1]]=b
                    base=[max(-core[i],min(core[i],q[i])) for i in range(3)]
                    normal=unit(sub(q,base))
                    return tuple(centre[i]+base[i]+r*normal[i] for i in range(3)),normal
                for i in range(7):
                    for j in range(7):
                        vals=[point(grid[other[0]][i],grid[other[1]][j]),point(grid[other[0]][i+1],grid[other[1]][j]),
                              point(grid[other[0]][i+1],grid[other[1]][j+1]),point(grid[other[0]][i],grid[other[1]][j+1])]
                        pts=[v[0] for v in vals];normals=[v[1] for v in vals]
                        direction=cross(sub(pts[1],pts[0]),sub(pts[2],pts[0]))
                        if direction[axis]*sign<0:pts.reverse();normals.reverse()
                        self.face(pts,colour,normals=normals)
    def tube(self, start, end, radius, colour, inside=0, segments=24):
        """Closed visible prop barrel/optic. Optional dark inset, never internal parts."""
        direction=unit(sub(end,start));helper=(0,1,0) if abs(direction[1])<.9 else (1,0,0)
        u=unit(cross(direction,helper));v=cross(direction,u)
        rings=[];normals=[]
        for c in (start,end):
            ring=[]
            for i in range(segments):
                a=2*math.pi*i/segments;normal=tuple(u[j]*math.cos(a)+v[j]*math.sin(a) for j in range(3))
                ring.append(tuple(c[j]+radius*normal[j] for j in range(3)))
                if len(normals)<segments:normals.append(normal)
            rings.append(ring)
        self.face(list(reversed(rings[0])),colour)
        if not inside:self.face(rings[1],colour)
        else:
            inner=[tuple(end[j]+inside*normals[i][j] for j in range(3)) for i in range(segments)]
            back=[tuple(p[j]-direction[j]*.007 for j in range(3)) for p in inner]
            for i in range(segments):
                k=(i+1)%segments
                self.face([rings[1][i],rings[1][k],inner[k],inner[i]],colour)
                self.face([inner[i],inner[k],back[k],back[i]],DARK)
            self.face(back,(12,20,25))
        for i in range(segments):
            k=(i+1)%segments
            self.face([rings[0][i],rings[0][k],rings[1][k],rings[1][i]],colour,normals=[normals[i],normals[k],normals[k],normals[i]])
    def screw(self,x,y,z):
        s=1 if x>0 else -1
        self.tube((x,y,z),(x+s*.001,y,z),.0032,DARK,segments=12)
        self.box((x+s*.0011,y,z),(.0008,.0008,.004),METAL,.0001)
    def write(self,path,muzzle,label):
        if len(self.vertices)>100000 or len(self.indices)>300000:raise ValueError('Too large')
        if not all(math.isfinite(x) for v in self.vertices for x in v):raise ValueError('Nonfinite vertex')
        body=bytearray(struct.pack('<8sII3f',b'TCGUN002',len(self.vertices),len(self.indices),*muzzle))
        for v in self.vertices:body.extend(struct.pack('<10f',*v))
        body.extend(struct.pack('<%dI'%len(self.indices),*self.indices))
        path.write_bytes(body)
        meta={'name':label,'author':'Thatcher Productions','format':'TCGUN002','generator':'tools/build_arcade_weapons.py',
              'vertices':len(self.vertices),'triangles':len(self.indices)//3,'muzzle_m':muzzle,
              'bounds_m':[[min(v[i] for v in self.vertices) for i in range(3)],[max(v[i] for v in self.vertices) for i in range(3)]],
              'sha256':hashlib.sha256(body).hexdigest(),
              'description':'Original exterior-only game mesh. Not a scan, exact CAD replica, official Namco asset or real weapon design.'}
        path.with_suffix('.json').write_text(json.dumps(meta,indent=2)+'\n')
        return meta

def machine():
    m=Mesh();c=PALETTE['machine_gun'];trim=shade(c,.82);hi=shade(c,1.06)
    # Broad moulded upper receiver, short muzzle, top rail and stepped side panels.
    m.rounded_box((0,.044,-.102),(.074,.092,.350),c,.012)
    m.box((0,.091,-.094),(.044,.010,.326),hi,.002)
    m.box((0,.097,.045),(.041,.012,.026),c,.002)
    m.box((0,.097,-.250),(.033,.013,.025),c,.002)
    # Thin central mould line: no huge detached attachments or tactical rifle rails.
    m.box((0,.096,-.097),(.0011,.001,.264),trim,.0001)
    m.prism([(-.234,-.043),(-.213,-.058),(-.119,-.058),(-.083,-.025),(-.083,.008),(-.234,.008)],.074,c,.003)
    for s in (-1,1):
        m.prism([(-.249,.018),(-.247,.064),(-.199,.071),(-.082,.071),(-.071,.058),(-.071,.018)],.003,hi,.0006, xcentre=s*.0365)
        m.rounded_box((s*.036,.052,.025),(.007,.057,.087),c,.003)
        m.box((s*.0378,.047,-.064),(.0013,.007,.022),trim,.0002)
        # Recessed slots, moulded panel fasteners and small raised casing contours.
        for k in range(5):m.box((s*.039,.055,-.202+k*.009),(.001,.019,.0026),DARK,.0002)
        for k in range(3):m.box((s*.040,.075,.005+k*.010),(.001,.013,.0026),DARK,.0002)
        for y,z in [(.074,-.250),(.072,-.083),(.012,-.222),(.013,.060),(-.046,-.126)]:m.screw(s*.039,y,z)
        m.tube((s*.038,.011,-.026),(s*.041,.011,-.026),.010,trim,segments=20)
        m.box((s*.043,.013,-.023),(.004,.005,.023),c,.001)
    # Rear pistol grip with broad ribbed face and lower cap.
    m.prism([(-.027,.015),(.040,.015),(.044,-.049),(.028,-.145),(-.013,-.145),(-.019,-.060)],.050,c,.008)
    m.box((0,-.140,.007),(.054,.023,.046),hi,.004)
    for s in (-1,1):
        for i in range(10):
            y=-.052-i*.007;z=.009+(y+.052)*-.04
            m.box((s*.0255,y,z),(.0017,.002,.031),trim,.0002)
        m.screw(s*.026,-.129,.010)
    # Separate characteristic vertical foregrip, with rounded front and ribs.
    m.prism([(-.211,-.036),(-.173,-.036),(-.166,-.061),(-.170,-.110),(-.204,-.110),(-.216,-.076)],.042,c,.004)
    m.box((0,-.107,-.186),(.045,.014,.038),hi,.002)
    for s in (-1,1):
        for i in range(5):m.box((s*.0215,-.076-i*.006,-.188),(.0014,.0018,.029),trim,.0002)
        m.screw(s*.023,-.107,-.186)
    # Open trigger guard. No filled-in trigger hole.
    m.prism([(-.097,-.014),(-.086,-.014),(-.082,-.058),(-.092,-.058)],.025,c,.002)
    m.box((0,-.061,-.057),(.025,.009,.067),c,.002)
    m.prism([(-.036,-.007),(-.026,-.008),(-.034,-.035),(-.043,-.042),(-.045,-.034)],.012,DARK,.001,part=1)
    m.tube((0,.044,-.265),(0,.044,-.300),.020,c,inside=.0125)
    m.tube((0,.044,-.298),(0,.044,-.305),.017,trim,inside=.0125)
    return m,(0,.044,-.306)

def shotgun():
    m=Mesh();c=PALETTE['shotgun'];trim=shade(c,.78)
    # Short, exaggerated arcade prop: orange casing and pump, dark paired tubes.
    m.rounded_box((0,.038,-.045),(.065,.069,.204),c,.011)
    m.box((0,.074,-.047),(.028,.008,.186),trim,.0015)
    m.tube((0,.046,-.130),(0,.046,-.390),.0165,DARK,inside=.011)
    m.tube((0,.009,-.123),(0,.009,-.365),.013,DARK)
    m.tube((0,.046,-.385),(0,.046,-.400),.020,c,inside=.011)
    m.box((0,.067,-.368),(.007,.013,.010),c,.001)
    m.rounded_box((0,.005,-.253),(.061,.065,.139),c,.012)
    for s in (-1,1):
        for i in range(11):m.box((s*.029,.007,-.302+i*.009),(.003,.044,.003),trim,.0005)
        m.box((s*.033,.039,-.059),(.002,.025,.061),DARK,.0003)
        for y,z in [(.052,.034),(.026,-.127),(-.138,.029)]:m.screw(s*.034,y,z)
    m.prism([(-.012,.015),(.045,.015),(.070,-.152),(.028,-.160),(.006,-.071)],.047,c,.004)
    for s in (-1,1):
        for i in range(9):m.box((s*.024,-.067-i*.007,.028+i*.002),(.0018,.002,.026),trim,.0002)
    m.box((0,-.158,.049),(.052,.017,.047),trim,.002)
    m.box((0,-.053,-.034),(.023,.010,.079),c,.002)
    m.box((0,-.028,-.074),(.023,.044,.010),c,.002)
    m.prism([(-.018,-.008),(-.008,-.008),(-.018,-.030),(-.026,-.037),(-.029,-.030)],.011,DARK,.001,part=1)
    return m,(0,.046,-.401)

def recolour_pistol(source,dest):
    raw=source.read_bytes()
    if hashlib.sha256(raw).hexdigest()!=PISTOL_SHA:raise ValueError('Pistol source drift: review colour mask before regenerating')
    magic,n,ni,w,h,*tip=struct.unpack_from('<8sIIII3f',raw)
    if magic!=b'TCGUN001':raise ValueError('Wrong pistol format')
    offset=36+n*32+ni*4
    if len(raw)!=offset+w*h*4:raise ValueError('Malformed pistol')
    pixels=bytearray(raw[offset:]);changed=0
    # Select only the blue plastic. Neutral/black/metal/lettering pixels unchanged.
    target_h=colorsys.rgb_to_hsv(*(v/255 for v in PALETTE['handgun']))[0]
    for i in range(0,len(pixels),4):
        r,g,b=pixels[i:i+3];hh,ss,vv=colorsys.rgb_to_hsv(r/255,g/255,b/255)
        if .48<=hh<=.69 and ss>=.22 and vv>=.035:
            new=colorsys.hsv_to_rgb(target_h,max(.28,min(.60,ss*.60)),vv)
            pixels[i:i+3]=bytes(round(x*255) for x in new);changed+=1
    if not w*h*.15<changed<w*h*.90:raise ValueError('Unexpected colour mask size')
    result=raw[:offset]+pixels;dest.write_bytes(result)
    meta={'name':'Pink Namco-style handgun','original_author':'Martoscar','colour_variant_by':'Thatcher Productions',
          'license':'CC-BY-4.0','source':'https://sketchfab.com/3d-models/namco-arcade-gun-15fbd5b9add94a34b5c21746e3dd32be',
          'source_asset_sha256':PISTOL_SHA,'sha256':hashlib.sha256(result).hexdigest(),
          'vertices':n,'triangles':ni//3,'recoloured_pixels':changed,'muzzle_m':tip,
          'modifications':'Only blue shell texels recoloured pink; original geometry, UVs, normals, muzzle and neutral/metal pixels preserved.'}
    dest.with_suffix('.json').write_text(json.dumps(meta,indent=2)+'\n');return meta

def build(output):
    output.mkdir(parents=True,exist_ok=True)
    reports={}
    for name,fn,label in [('arcade-machine-gun',machine,'Turquoise Crisis Zone-style cabinet controller'),('arcade-shotgun',shotgun,'Orange arcade shotgun prop')]:
        mesh,muzzle=fn();reports[name]=mesh.write(output/(name+'.tcgun'),muzzle,label)
    reports['namco-gun-pink']=recolour_pistol(ROOT/'quest/assets/models/namco-gun.tcgun',output/'namco-gun-pink.tcgun')
    report={'palette_srgb':PALETTE,'assets':reports,'shotgun_gameplay':'Not enabled by asset generation; requires real damage integration.'}
    (output/'arcade-weapon-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    return report

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'quest/assets/models')
    a=p.parse_args();print(json.dumps(build(a.output),indent=2))
