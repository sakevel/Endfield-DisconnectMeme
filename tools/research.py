"""Read-only metadata + bounded disassembly. Requires the preinstalled unity-re Python tools.
No game assets or account data are included in the distributable package.
"""
from pathlib import Path
import struct, json, hashlib, re, bisect
import pefile, capstone
game = Path(r'D:\Game\Hypergryph Launcher\games\Endfield Game')
out = Path(__file__).resolve().parents[1]/'artifacts/research'
out.mkdir(parents=True, exist_ok=True)
b = (game/'Endfield_Data/il2cpp_data/Metadata/global-metadata.dat').read_bytes()
h = struct.unpack_from('<64I', b)
assert h[:2] == (0xfab11baf, 29)
def s(i): return b[h[6]+i:b.index(b'\0', h[6]+i)].decode('utf8')
types = [struct.unpack_from('<17i8H2I', b, i) for i in range(h[40],h[40]+h[41],92)]
names = {t[2]: '.'.join(filter(None,(s(t[1]),s(t[0])))) for t in types}
images = [struct.unpack_from('<10I',b,i) for i in range(h[42],h[42]+h[43],40)]
selected=[]
want={'LoginManager','LoginRootPanel','LoginEnterGamePanel','HGNetSession','HGNetBaseSession','TcpIO','INetIO','GameInstance','GameplayNetwork','NetClientManager','LoginAlertDialog','LoginMenuPanel','< _DoReconnectAsync>d__89'.replace(' ',''),'ReturnToLoginReason'}
manager_type=next(t[2] for t in types if s(t[0])=='LoginManager')
for ti,t in enumerate(types):
    methods=[]
    for ix in range(t[9], t[9]+t[17]):
        m=struct.unpack_from('<6i4H',b,h[12]+ix*32)
        if s(t[0]) not in want and m[2]!=manager_type: continue
        params=[struct.unpack_from('<3i',b,h[22]+(m[3]+k)*12) for k in range(m[-1])]
        methods.append(dict(name=s(m[0]),ret=names.get(m[2],str(m[2])),static=bool(m[6]&16),
          params=[dict(name=s(p[0]),type=names.get(p[2],str(p[2]))) for p in params],token=m[5]))
    if not methods: continue
    image=next(s(i[0]) for i in images if i[2]<=ti<i[2]+i[3])
    fields=[struct.unpack_from('<3i',b,h[24]+i*12) for i in range(t[8],t[8]+t[19])]
    selected.append(dict(image=image,name=names[t[2]],methods=methods,
       fields=[dict(name=s(f[0]),type=names.get(f[1],str(f[1]))) for f in fields]))
pe=pefile.PE(str(game/'GameAssembly.dll'), fast_load=True)
base=pe.OPTIONAL_HEADER.ImageBase; raw=pe.__data__
def off(va): return pe.get_offset_from_rva(va-base)
def q(va): return struct.unpack_from('<Q',raw,off(va))[0]
tables={}
for image in ['Entry.Beyond.dll','Network.Beyond.dll','Gameplay.Beyond.dll']:
    hits=[]
    for match in re.finditer(re.escape(image.encode()+b'\0'),raw):
        va=base+pe.get_rva_from_offset(match.start())
        for ref in re.finditer(re.escape(struct.pack('<Q',va)),raw):
            at=ref.start();count=struct.unpack_from('<I',raw,at+8)[0]; ptr=struct.unpack_from('<Q',raw,at+16)[0]
            if 100<count<300000 and base<=ptr<base+pe.OPTIONAL_HEADER.SizeOfImage: hits.append((count,ptr))
    assert len(hits)==1,(image,hits)
    count,ptr=hits[0]; addresses=sorted(set(q(ptr+i*8) for i in range(count)))
    tables[image]=(ptr,addresses)
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
targets={'TestDropNetIO','ReadData','ReadCryptoData','WriteData','WriteCryptoData','Available',
 'AlertDialog','_OnEnterGameClicked','OnValueChanged','Init','_OnLoginClicked',
 'get_instance','get_isInGameplay','get_gameplayNetwork','get_session','ClearSync','OnDestroy','PreTick','get_isNetSessionRunning','UpdateInGameThread','OnReconnectTimesOver','InternalClose','Close','_UpdateNetUI','ReturnToLogin','ShutDown','OpenAutoReconnect','CloseTCPMsgThreadTask','TestCloseNetIO','IsConnected','ConnectAsync','MoveNext','NeedKeepConnect','_ReconnectCheckerTick','_DoReconnectAsync'}
with (out/'disassembly.txt').open('w',encoding='utf8') as f:
    for t in selected:
        if t['image'] not in tables: continue
        ptr,addresses=tables[t['image']]
        for m in t['methods']:
            if m['name'] not in targets: continue
            va=q(ptr+((m['token']&0xffffff)-1)*8); idx=bisect.bisect_right(addresses,va)
            size=min(6000,addresses[idx]-va if idx<len(addresses) else 0)
            f.write(f"\n{t['name']} {m['name']} {hex(va)} bytes={size}\n")
            for ins in md.disasm(raw[off(va):off(va)+size],va): f.write(f'{ins.address:x} {ins.mnemonic} {ins.op_str}\n')
(out/'metadata.json').write_text(json.dumps(dict(metadataSHA256=hashlib.sha256(b).hexdigest(),types=selected),indent=2),encoding='utf8')
print('metadata29 selected',len(selected),'types; bounded Entry/Network disassembly written')
for t in selected:
    if t['name'] not in ['Beyond.LoginManager','Beyond.Network.TcpIO'] and not any(m['ret']=='Beyond.LoginManager' for m in t['methods']):continue
    print(t['name'],[(m['name'],m['ret'],m['static']) for m in t['methods']])
