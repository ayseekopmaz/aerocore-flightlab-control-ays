import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / (name + '.py'))
    module = importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module
profile = load('lab_profile'); wind = load('wind_action')

class Profiles(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory();self.repo=Path(self.temp.name)
        self.source=self.repo/'Tools/simulation/gz'
        for model, xml in [('x500', '<sdf version="1.9"><model name="x500"><include merge="true"><uri>model://x500_base</uri></include></model></sdf>'), ('x500_base','<sdf version="1.9"><model name="x500_base"><link name="base_link"><inertial><mass>2</mass></inertial><sensor name="gps" type="navsat"/></link></model></sdf>')]:
            path=self.source/'models'/model;path.mkdir(parents=True);(path/'model.sdf').write_text(xml)
        (self.source/'worlds').mkdir();(self.source/'worlds/default.sdf').write_text('<sdf version="1.9"><world name="default"/></sdf>')
        self.config={'payload':.5,'cgX':.1,'cgY':0,'cgZ':0,'gpsNoise':2}
    def tearDown(self): self.temp.cleanup()
    def test_payload_noise_wind_and_original_preserved(self):
        before=(self.source/'models/x500_base/model.sdf').read_bytes()
        tmp, env=profile.prepare_lab_profile(self.repo,self.config)
        try:
            root=Path(tmp.name);model=ET.parse(root/'models/aerocore_base/model.sdf')
            payload=model.find('.//link[@name="aerocore_payload"]');self.assertEqual(payload.findtext('inertial/mass'),'0.5');self.assertEqual(payload.findtext('pose'),'0.1 0 0 0 0 0')
            self.assertEqual(model.findtext('.//navsat/position_sensing/horizontal/noise/stddev'),'2')
            world=ET.parse(root/'worlds/aerocore_lab.sdf');self.assertIsNotNone(world.find('.//plugin[@name="gz::sim::systems::WindEffects"]'))
            self.assertEqual(env['PX4_GZ_MODEL_NAME'],'aerocore_x500_0');self.assertEqual(env['PX4_GZ_STANDALONE'],'1');self.assertIn('aerocore-lab-',env['GZ_PARTITION']);self.assertEqual(before,(self.source/'models/x500_base/model.sdf').read_bytes())
            self.assertEqual(env['gz_command'], 'gz')
        finally:tmp.cleanup()
    def test_invalid_profile(self):
        for value in [float('nan'),-1,3]:
            with self.assertRaises(ValueError):profile.prepare_lab_profile(self.repo,{**self.config,'payload':value})
    def test_wind_world_has_physics_scene_and_px4_sensor_systems(self):
        original = (self.source / 'worlds/default.sdf').read_bytes()
        tmp, _ = profile.prepare_lab_profile(self.repo, self.config)
        try:
            world = ET.parse(Path(tmp.name) / 'worlds/aerocore_lab.sdf').getroot().find('world')
            names = [p.get('name') for p in world.findall('plugin')]
            for kind in ('Physics', 'UserCommands', 'SceneBroadcaster', 'Contact',
                         'Imu', 'AirPressure', 'AirSpeed', 'ApplyLinkWrench',
                         'NavSat', 'Magnetometer', 'Sensors', 'WindEffects'):
                self.assertEqual(names.count('gz::sim::systems::' + kind), 1)
            self.assertEqual((self.source / 'worlds/default.sdf').read_bytes(), original)
        finally:
            tmp.cleanup()

    def test_existing_plugins_and_server_sensor_options_are_preserved(self):
        path = self.source / 'worlds/default.sdf'
        path.write_text('<sdf version="1.9"><world name="default"><plugin '
                        'filename="gz-sim-physics-system" name="gz::sim::systems::Physics">'
                        '<engine><filename>test-engine</filename></engine></plugin></world></sdf>')
        config = self.repo / 'src/modules/simulation/gz_bridge/server.config'
        config.parent.mkdir(parents=True)
        config.write_text('<server_config><plugins><plugin entity_name="*" entity_type="world" '
                          'filename="gz-sim-sensors-system" name="gz::sim::systems::Sensors">'
                          '<render_engine>ogre</render_engine></plugin></plugins></server_config>')
        tmp, _ = profile.prepare_lab_profile(self.repo, self.config)
        try:
            world = ET.parse(Path(tmp.name) / 'worlds/aerocore_lab.sdf').getroot().find('world')
            physics = world.findall('plugin[@name="gz::sim::systems::Physics"]')
            self.assertEqual(len(physics), 1)
            self.assertEqual(physics[0].findtext('engine/filename'), 'test-engine')
            sensors = world.find('plugin[@name="gz::sim::systems::Sensors"]')
            self.assertEqual(sensors.findtext('render_engine'), 'ogre')
            self.assertNotIn('entity_type', sensors.attrib)
            self.assertNotIn('entity_name', sensors.attrib)
        finally:
            tmp.cleanup()
    def test_zero_noise_preserves_sensor_defaults(self):
        tmp, _=profile.prepare_lab_profile(self.repo,{**self.config,'payload':0,'gpsNoise':0})
        try:self.assertIsNone(ET.parse(Path(tmp.name)/'models/aerocore_base/model.sdf').find('.//navsat'))
        finally:tmp.cleanup()
    def test_wind_requires_subscriber(self):
        response=subprocess.CompletedProcess([],0,'Publishers [0]\nSubscribers [0]','')
        with patch.object(sys,'argv',['wind','3','90']),patch.object(wind.subprocess,'run',return_value=response):self.assertEqual(wind.main(),2)
    def test_wind_real_cli_format(self):
        info=subprocess.CompletedProcess([],0,'Subscribers [Address, Message Type]:\n tcp://127.0.0.1:123, gz.msgs.Wind\n','')
        sent=subprocess.CompletedProcess([],0,'','')
        with patch.object(sys,'argv',['wind','3','90']),patch.object(wind.subprocess,'run',side_effect=[info,sent]) as run:
            self.assertEqual(wind.main(),0);self.assertIn('enable_wind: true',run.call_args.args[0][-1])

if __name__=='__main__': unittest.main()
