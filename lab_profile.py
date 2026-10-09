"""Isolated Gazebo model/world preparation. No PX4 source files are modified."""
import hashlib
import copy
import json
import math
import os
from pathlib import Path
import shutil
import tempfile
import xml.etree.ElementTree as ET


def child_text(node, tag, value):
    item = node.find(tag)
    if item is None:
        item = ET.SubElement(node, tag)
    item.text = str(value)
    return item


def ensure_world_systems(world, repo):
    """Explicit world plugins suppress Gazebo 8's default server plugins.

    Carry required PX4 world systems into the isolated SDF, preserving existing
    settings and configured sensor options. Never duplicate a world system.
    """
    required = (
        ('Physics', 'physics'), ('UserCommands', 'user-commands'),
        ('SceneBroadcaster', 'scene-broadcaster'), ('Contact', 'contact'),
        ('Imu', 'imu'), ('AirPressure', 'air-pressure'),
        ('AirSpeed', 'air-speed'), ('ApplyLinkWrench', 'apply-link-wrench'),
        ('NavSat', 'navsat'), ('Magnetometer', 'magnetometer'),
        ('Sensors', 'sensors'),
    )
    config = repo / 'src/modules/simulation/gz_bridge/server.config'
    configured = []
    if config.is_file():
        try:
            configured = ET.parse(config).getroot().findall('.//plugin')
        except ET.ParseError as exc:
            raise ValueError('PX4 server.config XML okunamadı: ' + str(exc)) from exc
    for kind, library in required:
        name = 'gz::sim::systems::' + kind
        filename = 'gz-sim-' + library + '-system'
        if any(p.get('name') == name or p.get('filename') == filename
               for p in world.findall('plugin')):
            continue
        template = next((p for p in configured
                         if p.get('entity_type') == 'world'
                         and p.get('entity_name', '*') in ('*', world.get('name'))
                         and (p.get('name') == name or p.get('filename') == filename)), None)
        if template is not None:
            plugin = copy.deepcopy(template)
            plugin.attrib.pop('entity_name', None)
            plugin.attrib.pop('entity_type', None)
            world.append(plugin)
        else:
            plugin = ET.SubElement(world, 'plugin', filename=filename, name=name)
            if kind == 'Sensors':
                child_text(plugin, 'render_engine', 'ogre2')


def prepare_lab_profile(repo, profile):
    allowed = {'payload', 'cgX', 'cgY', 'cgZ', 'gpsNoise'}
    if set(profile) != allowed:
        raise ValueError('Model profili alanları geçersiz.')
    for key, value in profile.items():
        if type(value) not in (int, float) or not math.isfinite(value):
            raise ValueError('Model profili sonlu sayılar içermeli.')
    if not 0 <= profile['payload'] <= 2 or not 0 <= profile['gpsNoise'] <= 10:
        raise ValueError('Kütle / GPS gürültüsü sınır dışında.')
    if any(abs(profile[k]) > .5 for k in ('cgX', 'cgY', 'cgZ')):
        raise ValueError('Yük konumu sınır dışında.')
    source = repo / 'Tools/simulation/gz'
    for relative in ('models/x500/model.sdf', 'models/x500_base/model.sdf', 'worlds/default.sdf'):
        if not (source / relative).is_file():
            raise ValueError('Gazebo model kaynağı bulunamadı: ' + relative)
    temp = tempfile.TemporaryDirectory(prefix='aerocore-lab-')
    root = Path(temp.name)
    try:
        models, worlds = root / 'models', root / 'worlds'
        models.mkdir(); worlds.mkdir()
        for old, new in (('x500', 'aerocore_x500'), ('x500_base', 'aerocore_base')):
            shutil.copytree(source / 'models' / old, models / new)
            path = models / new / 'model.sdf'
            tree = ET.parse(path); model = tree.getroot().find('model')
            # Keep base nested model name, so scoped joints and links stay valid.
            if old == 'x500':
                model.set('name', 'aerocore_x500')
                for uri in model.findall('.//include/uri'):
                    if uri.text and uri.text.strip() == 'model://x500_base':
                        uri.text = 'model://aerocore_base'
            else:
                for link in model.findall('link'):
                    child_text(link, 'enable_wind', 'true')
                mass = profile['payload']
                if mass > 0:
                    link = ET.SubElement(model, 'link', name='aerocore_payload')
                    child_text(link, 'pose', f"{profile['cgX']} {profile['cgY']} {profile['cgZ']} 0 0 0")
                    inertial = ET.SubElement(link, 'inertial')
                    child_text(inertial, 'mass', mass)
                    inertia = ET.SubElement(inertial, 'inertia')
                    # Uniform solid sphere, 5 cm radius. Fixed payload, not released in flight.
                    for axis in ('ixx', 'iyy', 'izz'): child_text(inertia, axis, .4 * mass * .05 ** 2)
                    for axis in ('ixy', 'ixz', 'iyz'): child_text(inertia, axis, 0)
                    visual = ET.SubElement(link, 'visual', name='payload')
                    sphere = ET.SubElement(ET.SubElement(visual, 'geometry'), 'sphere')
                    child_text(sphere, 'radius', .05)
                    joint = ET.SubElement(model, 'joint', name='payload_fixed', type='fixed')
                    child_text(joint, 'parent', 'base_link'); child_text(joint, 'child', 'aerocore_payload')
                if profile['gpsNoise'] > 0:
                    sensors = model.findall('.//sensor[@type="navsat"]')
                    if not sensors:
                        raise ValueError('NavSat sensörü bulunamadı; GPS gürültüsü uygulanamadı.')
                    for sensor in sensors:
                        nav = sensor.find('navsat')
                        if nav is None: nav = ET.SubElement(sensor, 'navsat')
                        position = nav.find('position_sensing')
                        if position is None: position = ET.SubElement(nav, 'position_sensing')
                        for axis in ('horizontal', 'vertical'):
                            node = position.find(axis)
                            if node is None: node = ET.SubElement(position, axis)
                            noise = node.find('noise')
                            if noise is None: noise = ET.SubElement(node, 'noise')
                            noise.set('type', 'gaussian'); child_text(noise, 'mean', 0); child_text(noise, 'stddev', profile['gpsNoise'])
            tree.write(path, encoding='utf-8', xml_declaration=True)
        tree = ET.parse(source / 'worlds/default.sdf'); world = tree.getroot().find('world')
        world.set('name', 'aerocore_lab')
        ensure_world_systems(world, repo)
        wind = world.find('wind')
        if wind is None: wind = ET.SubElement(world, 'wind')
        child_text(wind, 'linear_velocity', '0 0 0')
        if not any('WindEffects' in p.attrib.get('name', '') for p in world.findall('plugin')):
            plugin = ET.SubElement(world, 'plugin', filename='gz-sim-wind-effects-system', name='gz::sim::systems::WindEffects')
            child_text(plugin, 'force_approximation_scaling_factor', 1)
        include = ET.SubElement(world, 'include')
        child_text(include, 'uri', 'model://aerocore_x500')
        child_text(include, 'name', 'aerocore_x500_0')
        child_text(include, 'pose', '0 0 0.2 0 0 0')
        world_path = worlds / 'aerocore_lab.sdf'; tree.write(world_path, encoding='utf-8', xml_declaration=True)
        digest = hashlib.sha256()
        for path in sorted(root.rglob('*.sdf')): digest.update(path.read_bytes())
        manifest = {'profile': profile, 'sha256': digest.hexdigest(), 'world': 'aerocore_lab', 'partition': root.name,
                    'physics': 'Gazebo WindEffects generic / fixed spherical payload', 'seed': 'Gazebo sensor noise uses simulator RNG'}
        print('AERO_PROFILE: ' + json.dumps(manifest, separators=(',', ':')), flush=True)
        env = os.environ.copy()
        env['GZ_SIM_RESOURCE_PATH'] = ':'.join((str(models), str(worlds), str(source/'models'), str(source/'worlds'), env.get('GZ_SIM_RESOURCE_PATH', '')))
        env['GZ_PARTITION'] = root.name
        # Older PX4 standalone startup scripts leave this shell variable unset.
        # They then invoke an empty command while probing Gazebo services.
        env['gz_command'] = 'gz'
        env['GZ_SIM_SYSTEM_PLUGIN_PATH'] = str(repo/'build/px4_sitl_default/src/modules/simulation/gz_plugins') + ':' + env.get('GZ_SIM_SYSTEM_PLUGIN_PATH', '')
        config = repo/'src/modules/simulation/gz_bridge/server.config'
        if config.is_file(): env['GZ_SIM_SERVER_CONFIG_PATH'] = str(config)
        env['PX4_GZ_WORLD'] = 'aerocore_lab'; env['PX4_GZ_MODEL_NAME'] = 'aerocore_x500_0'; env['PX4_GZ_STANDALONE'] = '1'; env['PX4_SYS_AUTOSTART'] = '4001'
        env.pop('PX4_SIM_MODEL', None); env.pop('PX4_GZ_MODEL', None)
        return temp, env
    except Exception:
        temp.cleanup()
        raise
