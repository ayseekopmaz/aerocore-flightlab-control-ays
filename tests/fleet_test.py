import importlib.util
import json
import os
from pathlib import Path
import select
import subprocess
import sys
import tempfile
import time
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import fleet_profile
from profile_test import Profiles

class FleetProfileTests(unittest.TestCase):
    setUp=Profiles.setUp
    tearDown=Profiles.tearDown
    def config(self):
        return {'gateway':'172.18.144.1','vehicles':[{'system':i+1,'name':f'Drone {i+1}','profile':{**self.config,'payload':i*.2},'parameters':{},'batteryModel':'PX4 synthetic'} for i in range(3)]}
    def test_three_models_distinct_physics_and_shared_world(self):
        c=FleetProfileTests.config(self); before=(self.source/'models/x500_base/model.sdf').read_bytes()
        tmp,env,m=fleet_profile.prepare_fleet_profile(self.repo,c)
        try:
            root=Path(tmp.name);world=ET.parse(root/'worlds/aerocore_lab.sdf').getroot().find('world')
            self.assertIsNotNone(world);models=world.findall('include');self.assertEqual(len(models),3)
            self.assertEqual([x.findtext('name') for x in models],['aerocore_x500_1','aerocore_x500_2','aerocore_x500_3'])
            self.assertEqual([v['port'] for v in m['vehicles']],[14561,14562,14563])
            self.assertEqual(len({v['sha256'] for v in m['vehicles']}),3)
            for n in range(1,4):
                model=ET.parse(root/f'models/aerocore_x500_{n}/model.sdf')
                self.assertEqual(model.findtext('.//include/uri'),f'model://aerocore_base_{n}')
            self.assertEqual(before,(self.source/'models/x500_base/model.sdf').read_bytes())
        finally:tmp.cleanup()
    def test_invalid_duplicate_id_and_gateway(self):
        c=FleetProfileTests.config(self);c['vehicles'][1]['system']=1
        with self.assertRaises(ValueError):fleet_profile.validate_fleet(c)
        c=FleetProfileTests.config(self);c['gateway']='hello'
        with self.assertRaises(ValueError):fleet_profile.validate_fleet(c)
    def test_no_reserved_system_id_parameter(self):
        c=FleetProfileTests.config(self);c['vehicles'][0]['parameters']={'MAV_SYS_ID':3}
        with self.assertRaises(ValueError):fleet_profile.validate_fleet(c)

class FleetLauncherTests(FleetProfileTests):
    def run_fleet(self,fail_gz=False):
        self.repo.joinpath('Makefile').write_text('')
        etc=self.repo/'build/px4_sitl_default/etc';etc.mkdir(parents=True)
        binary=self.repo/'build/px4_sitl_default/bin/px4';binary.parent.mkdir()
        marker=self.repo/'instances.jsonl'
        binary.write_text('#!'+sys.executable+'\n'+f'''import os,sys,json
print('Startup script returned successfully',flush=True)
with open({str(marker)!r},'a') as f:f.write(json.dumps({{'args':sys.argv[1:],'cwd':os.getcwd(),'model':os.environ.get('PX4_GZ_MODEL_NAME')}})+'\\n')
for line in sys.stdin:
    print('SEEN '+line.strip(),flush=True)
''');binary.chmod(0o755)
        tools=self.repo/'tools';tools.mkdir()
        for name,body in [('make','pass'),('gz',("import sys\nsys.exit(7)" if fail_gz else "import sys,time\nif sys.argv[1:]==['service','-l']:print('/world/aerocore_lab/scene/info')\nelse:time.sleep(120)"))]:
            p=tools/name;p.write_text('#!'+sys.executable+'\n'+body+'\n');p.chmod(0o755)
        env=dict(os.environ,PATH=str(tools),DISPLAY=':0')
        # Exact concatenated Qt resource execution, not only module imports.
        source='\n'.join((ROOT/f).read_text() for f in ['lab_profile.py','fleet_profile.py','fleet_launcher.py'])
        child=subprocess.Popen([sys.executable,'-u','-c',source,str(self.repo),json.dumps(FleetProfileTests.config(self))],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env)
        data=b'';deadline=time.monotonic()+12
        try:
            while time.monotonic()<deadline:
                ready,_,_=select.select([child.stdout],[],[],.1)
                if ready:
                    chunk=os.read(child.stdout.fileno(),16384)
                    if not chunk:break
                    data+=chunk
                    if b'AERO_FLEET_READY' in data:break
                if child.poll() is not None:break
            if child.poll() is None:child.stdin.write(b'__AERO_STOP__\n');child.stdin.flush()
            tail,_=child.communicate(timeout=8);data+=tail
            return child.returncode,data,marker
        finally:
            if child.poll() is None:child.kill();child.wait()
    def test_three_instances_ready_and_owned_shutdown(self):
        code,data,marker=self.run_fleet();self.assertEqual(code,0,data.decode());self.assertIn(b'AERO_FLEET_READY',data)
        entries=[json.loads(x) for x in marker.read_text().splitlines()];self.assertEqual(len(entries),3)
        self.assertEqual({tuple(x['args']) for x in entries},{('-i','0'),('-i','1'),('-i','2')});self.assertEqual(len({x['cwd'] for x in entries}),3)
        for port in [14561,14562,14563]:self.assertIn(f'mavlink start -u {port}'.encode(),data)
        for x in entries:self.assertFalse(Path(x['cwd']).exists())
    def test_gazebo_exit_prevents_px4_spawn(self):
        code,data,marker=self.run_fleet(True);self.assertEqual(code,2);self.assertIn(b'AERO_ERROR',data);self.assertFalse(marker.exists())

if __name__=='__main__':unittest.main()
