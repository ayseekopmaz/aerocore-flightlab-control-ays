"""Procedural IHA Maker quad: no x500 mesh, mass or motor physics.
PX4-compatible sensor definitions are copied from the installed repository only.
All unspecified geometry/dynamics are explicit approximations in the manifest.
"""
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import tempfile
import xml.etree.ElementTree as ET
if 'ensure_world_systems' not in globals():
    from lab_profile import child_text, ensure_world_systems


def number(obj, key, low, high):
    v = obj.get(key)
    if type(v) not in (int, float) or not math.isfinite(v) or not low <= v <= high:
        raise ValueError('IHA model değeri sınır dışında: ' + key)
    return float(v)


def validate_hardware(h):
    if not isinstance(h, dict) or h.get('schemaVersion') != 1:
        raise ValueError('IHA Maker schemaVersion=1 gerekli.')
    e, hw, sim = h.get('reportEstimates', {}), h.get('hardware', {}), h.get('simulation', {})
    mass = number(e, 'takeoffMassG', 10, 10000) / 1000
    span = number(e, 'wheelbaseMm', 50, 2000) / 1000
    thrust = number(e, 'maxTotalThrustG', .1, 100000) * 9.80665 / 1000
    rpm = number(e, 'maxRpm', 100, 200000)
    diameter = number(hw, 'propellerDiameterIn', .1, 30) * .0254
    rotor_mass = number(sim, 'rotorMassEachG', .01, 100) / 1000
    if 4 * rotor_mass >= mass: raise ValueError('Pervane kütleleri toplam kütleden küçük olmalı.')
    if diameter >= span / math.sqrt(2): raise ValueError('Pervaneler çakışıyor.')
    cg = e.get('cgMmForwardRightUp')
    if not isinstance(cg, list) or len(cg) != 3 or any(type(x) not in (int, float) or not math.isfinite(x) or abs(x) > 500 for x in cg):
        raise ValueError('Ağırlık merkezi [ileri, sağ, yukarı] mm gerekli.')
    for key in ('motorTimeConstantUpS', 'motorTimeConstantDownS'): number(sim,key,.001,2)
    number(sim,'momentConstantM',.0001,.2)
    return mass, span, thrust, rpm * 2 * math.pi / 60, diameter, rotor_mass, [cg[0]/1000,-cg[1]/1000,cg[2]/1000]


def inertial(link, mass, diagonal, cg=(0,0,0)):
    i=ET.SubElement(link,'inertial'); child_text(i,'pose',' '.join(map(str,(*cg,0,0,0))))
    child_text(i,'mass',mass); tensor=ET.SubElement(i,'inertia')
    for key,v in zip(('ixx','iyy','izz'),diagonal): child_text(tensor,key,v)
    for key in ('ixy','ixz','iyz'): child_text(tensor,key,0)


def shape(link,name,size,pose,color='0.12 0.15 0.18 1',collision=True):
    for kind in ('visual','collision') if collision else ('visual',):
        element=ET.SubElement(link,kind,name=name);child_text(element,'pose',' '.join(map(str,pose)))
        box=ET.SubElement(ET.SubElement(element,'geometry'),'box');child_text(box,'size',' '.join(map(str,size)))
        if kind=='visual':
            material=ET.SubElement(element,'material');child_text(material,'ambient',color);child_text(material,'diffuse',color)


def make_model(repo,h,profile,name):
    mass,span,thrust,omega,diameter,rotor_mass,cg=validate_hardware(h)
    a=span/(2*math.sqrt(2)); model=ET.Element('model',name=name)
    base=ET.SubElement(model,'link',name='base_link');child_text(base,'enable_wind','true')
    body_mass=mass-4*rotor_mass
    # Approximate box central mass plus four motor point masses. Not measured inertia.
    motor_mass=number(h['hardware'],'motorMassEachG',1,1000)/1000 if 'motorMassEachG' in h['hardware'] else .032
    if 4*motor_mass>=body_mass:raise ValueError('Motor ve pervane toplam kütlesi kalkış kütlesinden küçük olmalı.')
    center_mass=body_mass-4*motor_mass
    dims_mm=h['simulation'].get('bodyDimensionsMm',[100,70,45])
    if not isinstance(dims_mm,list) or len(dims_mm)!=3 or any(type(v) not in (int,float) or not math.isfinite(v) or not 5<=v<=500 for v in dims_mm):raise ValueError('Geçersiz gövde boyutları.')
    dims=tuple(v/1000 for v in dims_mm)
    inertia=[center_mass*(dims[1]**2+dims[2]**2)/12+4*motor_mass*a*a,
             center_mass*(dims[0]**2+dims[2]**2)/12+4*motor_mass*a*a,
             center_mass*(dims[0]**2+dims[1]**2)/12+8*motor_mass*a*a]
    # Shift base COM to ensure complete vehicle COM matches the PDF with rotor masses.
    body_cg=[v*mass/body_mass for v in cg]
    body_cg[2]=(cg[2]*mass-4*rotor_mass*.025)/body_mass
    inertial(base,body_mass,inertia,body_cg)
    shape(base,'center_plate',(dims[0],dims[1],min(dims[2],.008)),(0,0,0,0,0,0))
    shape(base,'battery',(.075,.035,.035),(0,0,.028,0,0,0),'0.9 0.16 0.2 1',False)
    shape(base,'battery_strap',(.012,.037,.037),(0,0,.028,0,0,0),'0.08 0.09 0.10 1',False)
    camera_enabled=h['hardware'].get('cameraEnabled',True)
    if type(camera_enabled) is not bool:raise ValueError('cameraEnabled boolean olmalı.')
    if camera_enabled:shape(base,'camera',(.025,.025,.025),(dims[0]/2+.005,0,-.005,0,0,0),'0.05 0.05 0.05 1',False)
    for angle in (math.pi/4,-math.pi/4):shape(base,'arm_'+str(angle),(span,.012,.008),(0,0,0,0,0,angle))
    template=repo/'Tools/simulation/gz/models/x500_base/model.sdf'
    if not template.is_file():raise ValueError('PX4 uyumlu sensör şablonu yok: '+str(template))
    source=ET.parse(template)
    needed={'imu','navsat','air_pressure','magnetometer'};found=set()
    for sensor in source.findall('.//sensor'):
        if sensor.get('type') in needed:
            s=copy.deepcopy(sensor);pose=child_text(s,'pose','0 0 0 0 0 0');pose.attrib.clear();base.append(s);found.add(s.get('type'))
    if not {'imu','navsat','air_pressure'}.issubset(found):raise ValueError('Sensör şablonunda IMU/GPS/barometre eksik.')
    noise=profile['gpsNoise']
    if noise:
        for s in base.findall('sensor[@type="navsat"]'):
            nav=s.find('navsat')
            if nav is None:nav=ET.SubElement(s,'navsat')
            pos=nav.find('position_sensing')
            if pos is None:pos=ET.SubElement(nav,'position_sensing')
            for axis in ('horizontal','vertical'):
                ax=pos.find(axis)
                if ax is None:ax=ET.SubElement(pos,axis)
                n=ax.find('noise')
                if n is None:n=ET.SubElement(ax,'noise')
                n.set('type','gaussian');child_text(n,'mean',0);child_text(n,'stddev',noise)
    if camera_enabled:
        camera_width=number(h['simulation'],'cameraWidth',64,3840) if 'cameraWidth' in h['simulation'] else 640
        camera_height=number(h['simulation'],'cameraHeight',64,2160) if 'cameraHeight' in h['simulation'] else 480
        camera_rate=number(h['simulation'],'cameraRateHz',1,60) if 'cameraRateHz' in h['simulation'] else 30
        if camera_width!=int(camera_width) or camera_height!=int(camera_height):raise ValueError('Kamera boyutları tam sayı olmalı.')
        camera_width=int(camera_width);camera_height=int(camera_height)
        camera=ET.SubElement(base,'sensor',name='iha_camera',type='camera');child_text(camera,'pose','.06 0 0 0 0 0');child_text(camera,'always_on','true');child_text(camera,'update_rate',camera_rate);child_text(camera,'topic',f'/iha_maker/{name}/camera')
        cam=ET.SubElement(camera,'camera');child_text(cam,'horizontal_fov',1.2);image=ET.SubElement(cam,'image');child_text(image,'width',camera_width);child_text(image,'height',camera_height);child_text(image,'format','R8G8B8');clip=ET.SubElement(cam,'clip');child_text(clip,'near',.02);child_text(clip,'far',300)
    total_mass=mass+profile['payload']
    full_cg=[(mass*cg[i]+profile['payload']*profile[k])/total_mass for i,k in enumerate(('cgX','cgY','cgZ'))]
    params={'CA_AIRFRAME':0,'CA_ROTOR_COUNT':4}
    positions=((a,-a),(-a,a),(a,a),(-a,-a)) # FLU; first pair CCW, second CW.
    for index,(x,y) in enumerate(positions):
        rotor=ET.SubElement(model,'link',name=f'rotor_{index}');child_text(rotor,'pose',f'{x} {y} .025 0 0 0')
        # Rotor thickness .001, radius diameter/2, uniform disk approximation.
        r=diameter/2;inertial(rotor,rotor_mass,(rotor_mass*(3*r*r+.001**2)/12,rotor_mass*(3*r*r+.001**2)/12,.5*rotor_mass*r*r))
        blades=int(number(h['hardware'],'propellerBlades',2,6)) if 'propellerBlades' in h['hardware'] else 3
        for blade in range(blades):
            angle=2*math.pi*blade/blades
            shape(rotor,'blade_'+str(blade),(diameter/2,.008,.001),(diameter/4*math.cos(angle),diameter/4*math.sin(angle),0,0,0,angle),'1 .40 .05 1',False)
        shape(base,f'motor_{index}',(.024,.024,.025),(x,y,.0125,0,0,0),collision=False)
        joint=ET.SubElement(model,'joint',name=f'rotor_{index}_joint',type='revolute');child_text(joint,'parent','base_link');child_text(joint,'child',f'rotor_{index}')
        axis=ET.SubElement(joint,'axis');child_text(axis,'xyz','0 0 1');limit=ET.SubElement(axis,'limit');child_text(limit,'lower',-1e16);child_text(limit,'upper',1e16)
        plugin=ET.SubElement(model,'plugin',filename='gz-sim-multicopter-motor-model-system',name='gz::sim::systems::MulticopterMotorModel')
        values={'jointName':f'rotor_{index}_joint','linkName':f'rotor_{index}','turningDirection':'ccw' if index<2 else 'cw','timeConstantUp':h['simulation']['motorTimeConstantUpS'],'timeConstantDown':h['simulation']['motorTimeConstantDownS'],'maxRotVelocity':omega,'motorConstant':thrust/4/omega**2,'momentConstant':h['simulation']['momentConstantM'],'commandSubTopic':'command/motor_speed','motorNumber':index,'rotorDragCoefficient':0,'rollingMomentCoefficient':0,'rotorVelocitySlowdownSim':10,'motorType':'velocity'}
        for key,value in values.items():child_text(plugin,key,value)
        # Gazebo FLU -> PX4 FRD. Relative to total vehicle center of mass.
        params.update({f'CA_ROTOR{index}_PX':x-full_cg[0],f'CA_ROTOR{index}_PY':-y+full_cg[1],f'CA_ROTOR{index}_PZ':-(.025-full_cg[2]),f'CA_ROTOR{index}_KM':h['simulation']['momentConstantM']*(1 if index<2 else -1),f'SIM_GZ_EC_MIN{index+1}':0,f'SIM_GZ_EC_MAX{index+1}':omega})
    if profile['payload']:
        m=profile['payload'];payload=ET.SubElement(model,'link',name='aerocore_payload');child_text(payload,'pose',f"{profile['cgX']} {profile['cgY']} {profile['cgZ']} 0 0 0")
        inertial(payload,m,(.4*m*.05**2,)*3);shape(payload,'payload',(.05,.05,.05),(0,0,0,0,0,0),'0.15 .35 .8 1')
        fixed=ET.SubElement(model,'joint',name='payload_fixed',type='fixed');child_text(fixed,'parent','base_link');child_text(fixed,'child','aerocore_payload')
    return model,params,{'massKg':mass,'diagonalM':span,'maxThrustN':thrust,'maxOmegaRadS':omega,'inertiaApproxKgM2':inertia,'cameraTopic':f'/iha_maker/{name}/camera' if camera_enabled else None,'thrustWeightRatio':thrust/((mass+profile['payload'])*9.80665),'assumptions':h['simulation'].get('assumptions',[])}


def prepare_iha_fleet(repo,config):
    validator=globals().get('validate_fleet')
    if validator is None:
        from fleet_profile import validate_fleet as validator
    vehicles=validator(config);temp=tempfile.TemporaryDirectory(prefix='aerocore-iha-');root=Path(temp.name)
    try:
        (root/'models').mkdir();(root/'worlds').mkdir()
        world_source=repo/'Tools/simulation/gz/worlds/default.sdf';tree=ET.parse(world_source);world=tree.getroot().find('world');world.set('name','aerocore_lab');ensure_world_systems(world,repo)
        # Keep default terrain/lights; vehicle is added below.
        wind=world.find('wind')
        if wind is None:wind=ET.SubElement(world,'wind')
        child_text(wind,'linear_velocity','0 0 0')
        if not any('WindEffects' in p.get('name','') for p in world.findall('plugin')):ET.SubElement(world,'plugin',filename='gz-sim-wind-effects-system',name='gz::sim::systems::WindEffects')
        manifest=[]
        for v in vehicles:
            p=v['profile']
            if set(p)!={'payload','cgX','cgY','cgZ','gpsNoise'}:raise ValueError('Geçersiz laboratuvar profili.')
            for k,value in p.items():
                if type(value) not in (int,float) or not math.isfinite(value):raise ValueError('Sonlu laboratuvar değerleri gerekli.')
            if not 0<=p['payload']<=2 or not 0<=p['gpsNoise']<=10 or any(abs(p[k])>.5 for k in ('cgX','cgY','cgZ')):raise ValueError('Laboratuvar profili sınır dışında.')
            h=v.get('hardware')
            if h is None:raise ValueError('IHA modunda her araç hardware profili içermeli.')
            name='iha_maker_'+str(v['system']);model,params,physics=make_model(repo,h,p,name)
            folder=root/'models'/name;folder.mkdir();sdf=ET.Element('sdf',version='1.9');sdf.append(model);ET.ElementTree(sdf).write(folder/'model.sdf',encoding='utf-8',xml_declaration=True)
            cfg=ET.Element('model');child_text(cfg,'name',name);child_text(cfg,'version','1');node=child_text(cfg,'sdf','model.sdf');node.set('version','1.9');ET.ElementTree(cfg).write(folder/'model.config',encoding='utf-8')
            include=ET.SubElement(world,'include');child_text(include,'uri','model://'+name);child_text(include,'name',name);child_text(include,'pose',f"0 {(v['system']-1)*8} .15 0 0 0")
            # Geometry overrides win over old fleet geometry/actuator range settings.
            parameters={k:value for k,value in v['parameters'].items() if k not in params};ratio=physics['thrustWeightRatio']
            if ratio>1:params['MPC_THR_HOVER']=math.sqrt(1/ratio)
            manifest.append({**v,'parameters':parameters,'modelParameters':params,'physics':physics,'sha256':hashlib.sha256((folder/'model.sdf').read_bytes()+json.dumps(h,sort_keys=True).encode()).hexdigest(),'model':name,'instance':v['system']-1,'port':14560+v['system'],'batteryModel':'PX4 synthetic; PDF battery documented, not electrical simulation'})
        tree.write(root/'worlds/aerocore_lab.sdf',encoding='utf-8',xml_declaration=True)
        env=os.environ.copy();source=repo/'Tools/simulation/gz';env.update({'GZ_PARTITION':root.name,'GZ_SIM_RESOURCE_PATH':':'.join((str(root/'models'),str(root/'worlds'),str(source/'models'),str(source/'worlds'),env.get('GZ_SIM_RESOURCE_PATH',''))),'PX4_GZ_WORLD':'aerocore_lab','PX4_GZ_STANDALONE':'1','PX4_SYS_AUTOSTART':'4001','gz_command':'gz'})
        env['GZ_SIM_SYSTEM_PLUGIN_PATH']=str(repo/'build/px4_sitl_default/src/modules/simulation/gz_plugins')+':'+env.get('GZ_SIM_SYSTEM_PLUGIN_PATH','')
        server=repo/'src/modules/simulation/gz_bridge/server.config'
        if server.is_file():env['GZ_SIM_SERVER_CONFIG_PATH']=str(server)
        for key in ('PX4_GZ_MODEL','PX4_GZ_MODEL_NAME','PX4_SIM_MODEL'):env.pop(key,None)
        result={'vehicles':manifest,'partition':root.name,'world':'aerocore_lab','gateway':config['gateway']}
        print('AERO_FLEET: '+json.dumps(result,separators=(',',':')),flush=True)
        return temp,env,result
    except Exception:temp.cleanup();raise
