"""Three independent x500 profiles in one isolated Gazebo world."""
import contextlib
import io
import ipaddress
import re
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import tempfile
import xml.etree.ElementTree as ET
if "prepare_lab_profile" not in globals():
    from lab_profile import prepare_lab_profile, child_text


def validate_fleet(config):
    if not isinstance(config, dict) or set(config) != {'vehicles', 'gateway'}:
        raise ValueError('Fleet config requires vehicles and gateway.')
    ipaddress.IPv4Address(config['gateway'])
    vehicles = config['vehicles']
    if not isinstance(vehicles, list) or not 1 <= len(vehicles) <= 3:
        raise ValueError('Fleet size must be 1–3.')
    for index, v in enumerate(vehicles):
        if not isinstance(v, dict) or set(v) != {'system', 'name', 'profile', 'parameters', 'batteryModel'}:
            raise ValueError('Invalid vehicle fields.')
        if type(v['system']) is not int or v['system'] != index + 1:
            raise ValueError('System IDs must be contiguous from 1.')
        if not isinstance(v['name'], str) or not 1 <= len(v['name']) <= 60:
            raise ValueError('Invalid vehicle name.')
        if not isinstance(v['batteryModel'], str) or len(v['batteryModel']) > 120:
            raise ValueError('Invalid battery label.')
        params = v['parameters']
        if not isinstance(params, dict) or len(params) > 30:
            raise ValueError('Invalid parameters.')
        for name, value in params.items():
            if not re.fullmatch(r'[A-Z][A-Z0-9_]{0,15}', name) or name in ('MAV_SYS_ID', 'SYS_AUTOSTART'):
                raise ValueError('Invalid or reserved parameter: ' + name)
            if type(value) not in (int, float) or not math.isfinite(value) or abs(value) > 1e6:
                raise ValueError('Invalid parameter value.')
        if 'SIM_BAT_DRAIN' in params and not 0 <= params['SIM_BAT_DRAIN'] <= 3600:
            raise ValueError('Battery drain out of range.')
        if 'SIM_BAT_MIN_PCT' in params and not 0 <= params['SIM_BAT_MIN_PCT'] <= 100:
            raise ValueError('Battery minimum out of range.')
    return vehicles


def prepare_fleet_profile(repo, config):
    vehicles = validate_fleet(config)
    temp = tempfile.TemporaryDirectory(prefix='aerocore-fleet-')
    root = Path(temp.name)
    (root / 'models').mkdir(); (root / 'worlds').mkdir()
    manifests = []
    try:
        for v in vehicles:
            # Reuse tested single-model physics preparation, preserving originals.
            with contextlib.redirect_stdout(io.StringIO()):
                child, env = prepare_lab_profile(repo, v['profile'])
            try:
                old = Path(child.name)
                model_name = 'aerocore_x500_' + str(v['system'])
                base_name = 'aerocore_base_' + str(v['system'])
                for original, renamed in [('aerocore_x500', model_name), ('aerocore_base', base_name)]:
                    target = root / 'models' / renamed
                    shutil.copytree(old / 'models' / original, target)
                    tree = ET.parse(target / 'model.sdf')
                    if original == 'aerocore_x500':
                        tree.getroot().find('model').set('name', model_name)
                        for uri in tree.findall('.//include/uri'):
                            if uri.text == 'model://aerocore_base': uri.text = 'model://' + base_name
                    tree.write(target / 'model.sdf', encoding='utf-8', xml_declaration=True)
                # Hash physical profile independently of SYS/name/spawn position.
                h = hashlib.sha256()
                for p in sorted((old/'models').rglob('*.sdf')): h.update(p.read_bytes())
                manifests.append({**v, 'sha256': h.hexdigest(), 'model': model_name,
                                  'instance': v['system']-1, 'port': 14561+v['system']-1})
                if v['system'] == 1:
                    world_tree = ET.parse(old/'worlds/aerocore_lab.sdf')
                    world = world_tree.getroot().find('world')
                    for include in list(world.findall('include')):
                        if include.findtext('name') == 'aerocore_x500_0': world.remove(include)
            finally:
                child.cleanup()
        for v in manifests:
            include = ET.SubElement(world, 'include')
            child_text(include, 'uri', 'model://' + v['model'])
            child_text(include, 'name', v['model'])
            child_text(include, 'pose', f"0 {(v['system']-1)*8} 0.2 0 0 0")
        world_tree.write(root/'worlds/aerocore_lab.sdf', encoding='utf-8', xml_declaration=True)
        env['GZ_PARTITION'] = root.name
        source = repo/'Tools/simulation/gz'
        env['GZ_SIM_RESOURCE_PATH'] = ':'.join((str(root/'models'), str(root/'worlds'), str(source/'models'), str(source/'worlds'), os.environ.get('GZ_SIM_RESOURCE_PATH','')))
        env.pop('PX4_GZ_MODEL_NAME', None)
        manifest = {'vehicles': manifests, 'partition': root.name, 'world': 'aerocore_lab', 'gateway': config['gateway']}
        print('AERO_FLEET: ' + json.dumps(manifest, separators=(',', ':')), flush=True)
        return temp, env, manifest
    except Exception:
        temp.cleanup(); raise
