"""Use the external Loader's public black-box fixture; do not import its implementation."""
from pathlib import Path
import sys,subprocess,tempfile,base64
sys.path.insert(0,sys.argv[2])
from lupa.lua54 import LuaRuntime
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='zml-meme-services-') as tmp:
    d=Path(tmp)
    (d/'mod.ini').write_text('[mod]\nid=fixture\nlibrary=Fixture.dll\napi=1\nicon=fixture.png\nenabled=true\n',encoding='utf8')
    (d/'Fixture.dll').write_bytes(b'fixture only')
    (d/'fixture.png').write_bytes(base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+j0l0AAAAASUVORK5CYII='))
    process=subprocess.Popen([sys.argv[1],'--serve',str(d/'mod.ini'),str(root/'mod/mod.ini')],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    try:
        lua=LuaRuntime(unpack_returned_tuples=True)
        def route(_,path):
            process.stdin.write((path+'\n').encode('utf8'));process.stdin.flush()
            header=process.stdout.readline()
            assert header,'services process ended: '+process.stderr.read().decode(errors='replace')
            size=int(header);data=process.stdout.read(size);assert len(data)==size
            return data.decode('utf8')
        lua.globals().route=route
        lua.execute('loadstring=load;LuaManagerInst={LoadLua=function(s,p)return route(s,p)end}')
        lua.execute(route(None,'ZML/Api'))
        icon_data=lua.eval("ZML.icon_data('disconnect-meme')")
        assert isinstance(icon_data,str) and base64.b64decode(icon_data,validate=True)==(root/'mod/icon.png').read_bytes()
        lua.execute('''
          local m=ZML.mod('disconnect-meme');assert(m and m.config_menu=='standard' and #m.config.fields==6)
          assert(m.version=='0.3.0' and m.depends[1]=='keybinds' and m.icon=='png')
          assert(ZML.config_entry('disconnect-meme')==nil)
          local v=ZML.get('disconnect-meme');assert(v.enabled=='true' and v.hotkey==nil and v.timeout=='120')
          assert(v.fast_failure=='true' and v.failure_delay=='3')
          assert(ZML.set('disconnect-meme','failure_delay','5.0'))
          assert(ZML.set('disconnect-meme','fast_failure',false))
          assert(not ZML.set('disconnect-meme','failure_delay',0))
          assert(not ZML.set('disconnect-meme','failure_delay',16))
          assert(not ZML.set('disconnect-meme','failure_delay',1.5))
          assert(v.message:find('骑乘摩托车') and v.show_notice=='true')
          assert(ZML.set('disconnect-meme','timeout','60.0'))
          assert(not ZML.set('disconnect-meme','hotkey','Ctrl+Shift+F12'))
          assert(ZML.set('disconnect-meme','message','测试\\\\n下一行'))
          assert(ZML.get('disconnect-meme').message=='测试\\\\n下一行')
          assert(not ZML.set('disconnect-meme','message','<b>unsafe</b>'))
          assert(not ZML.set('disconnect-meme','timeout',181))
          assert(not ZML.set('disconnect-meme','timeout',60.5))
          assert(not ZML.set('disconnect-meme','hotkey','F10'))
          assert(ZML.set('disconnect-meme','show_notice',false))
          assert(ZML.set('disconnect-meme','enabled',false))
          assert(ZML.get('disconnect-meme').enabled=='false')
        ''')
        print('PNG icon byte-exact public API, standard menu metadata, hot values, UTF-8/newlines and rejected values passed')
    finally:
        process.stdin.close()
        try:process.wait(timeout=5)
        except subprocess.TimeoutExpired:process.kill();process.wait()
        assert process.returncode==0,process.stderr.read().decode(errors='replace')
