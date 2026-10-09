"""One Gazebo server, independent PX4 instances/PTYs and bounded owned cleanup."""
import errno
import os
import json
import hashlib
from pathlib import Path
import shutil
if "prepare_fleet_profile" not in globals():
    from fleet_profile import prepare_fleet_profile
import fcntl
import select
import signal
import subprocess
import sys
import termios
import time


def terminate_owned(processes):
    for p in processes:
        if p.poll() is None:
            try: os.killpg(p.pid, signal.SIGINT)
            except ProcessLookupError: pass
    deadline = time.monotonic() + 4
    while any(p.poll() is None for p in processes) and time.monotonic() < deadline:
        time.sleep(.05)
    for p in processes:
        if p.poll() is None:
            try: os.killpg(p.pid, signal.SIGKILL)
            except ProcessLookupError: pass
        p.wait()


def check_panel_stop():
    if select.select([0], [], [], 0)[0]:
        line = os.read(0, 4096)
        if not line or b'__AERO_STOP__' in line: raise KeyboardInterrupt


def fleet_main():
    processes, terminals = [], {}
    temp = None
    def interrupted(*_): raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, interrupted); signal.signal(signal.SIGINT, interrupted)
    try:
        if len(sys.argv) != 3: raise ValueError('Repository and fleet JSON required.')
        repo = Path(sys.argv[1]).expanduser().resolve()
        if not (repo/'Makefile').is_file(): raise ValueError('PX4 Makefile bulunamadı.')
        if not shutil.which('make') or not shutil.which('gz'): raise ValueError('make / gz bulunamadı.')
        if not os.environ.get('DISPLAY') and not os.environ.get('WAYLAND_DISPLAY'): raise ValueError('WSLg görüntü ortamı bulunamadı.')
        temp, env, manifest = prepare_fleet_profile(repo, json.loads(sys.argv[2]))
        build = subprocess.Popen(['make', 'px4_sitl_default'], cwd=repo, start_new_session=True)
        processes.append(build)
        while build.poll() is None:
            check_panel_stop(); time.sleep(.1)
        if build.returncode: raise ValueError('PX4 derleme başarısız: '+str(build.returncode))
        binary = repo/'build/px4_sitl_default/bin/px4'
        if not binary.is_file(): raise ValueError('PX4 binary bulunamadı.')
        print('AERO_FIRMWARE: '+hashlib.sha256(binary.read_bytes()).hexdigest(), flush=True)
        sim = subprocess.Popen(['gz', 'sim', '-r', '-v', '4', str(Path(temp.name)/'worlds/aerocore_lab.sdf')], env=env, start_new_session=True)
        processes.append(sim)
        deadline = time.monotonic()+60
        while True:
            check_panel_stop()
            if sim.poll() is not None: raise ValueError('Gazebo açılışta kapandı: '+str(sim.returncode))
            try:
                probe = subprocess.run(['gz','service','-l'], env=env, capture_output=True, text=True, timeout=3)
                if probe.returncode == 0 and '/world/aerocore_lab/scene/info' in [x.strip() for x in probe.stdout.splitlines()]: break
            except subprocess.TimeoutExpired: pass
            if time.monotonic() >= deadline: raise ValueError('Gazebo dünya servisi 60 sn içinde hazır olmadı.')
            time.sleep(.25)
        print('AERO_GAZEBO_READY: /world/aerocore_lab/scene/info', flush=True)
        for v in manifest['vehicles']:
            index = v['instance']; sysid = v['system']
            cwd = Path(temp.name)/('instance_'+str(index)); cwd.mkdir(); (cwd/'fs').mkdir()
            # Fresh parameters/logs per instance. Share only immutable build resources.
            resource = repo/'build/px4_sitl_default/etc'
            if not resource.is_dir(): resource = repo/'build/px4_sitl_default/rootfs/etc'
            if not resource.is_dir(): raise ValueError('PX4 etc startup resources bulunamadı.')
            (cwd/'etc').symlink_to(resource, target_is_directory=True)
            master, slave = os.openpty()
            def attach():
                os.setsid(); fcntl.ioctl(slave, termios.TIOCSCTTY, 0)
            child_env = dict(env, PX4_GZ_MODEL_NAME=v['model'])
            for key in list(child_env):
                if key.startswith("PX4_PARAM_"): del child_env[key]
            # PX4 rcS supports validated PX4_PARAM_<name> boot overrides.
            # Battery timing remains scoped to experiments rather than consuming
            # charge during operator setup. Runner reads back all requested values.
            for key,value in v['parameters'].items():
                if key not in ('SIM_BAT_DRAIN', 'SIM_BAT_MIN_PCT'):
                    child_env['PX4_PARAM_'+key] = str(value)
            try:
                child = subprocess.Popen([str(binary), '-i', str(index)], cwd=cwd, env=child_env,
                                         stdin=slave, stdout=slave, stderr=slave, preexec_fn=attach, close_fds=True)
            except Exception:
                os.close(master); raise
            finally: os.close(slave)
            processes.append(child)
            terminals[master] = {'p':child,'system':sysid,'bytes':b'', 'history':b'', 'queue':[], 'next':0, 'started':False,'ready':False,'port':v['port']}
        stdin = b''; deadline = time.monotonic()+120
        while True:
            if sim.poll() is not None: raise ValueError('Gazebo oturumu beklenmedik şekilde kapandı.')
            for t in terminals.values():
                if t['p'].poll() is not None: raise ValueError('SYS '+str(t['system'])+' PX4 kapandı: '+str(t['p'].returncode))
                if t['queue'] and time.monotonic() >= t['next']:
                    fd = next(fd for fd,x in terminals.items() if x is t)
                    os.write(fd, (t['queue'].pop(0)+'\n').encode()); t['next']=time.monotonic()+.2
                elif t['started'] and not t['queue'] and not t['ready'] and time.monotonic() >= t['next']:
                    t['ready']=True; print('AERO_VEHICLE_READY: '+str(t['system']), flush=True)
            if all(t['ready'] for t in terminals.values()) and deadline:
                print('AERO_FLEET_READY', flush=True); deadline=0
            if deadline and time.monotonic() > deadline: raise ValueError('PX4 çoklu açılış 120 sn içinde tamamlanmadı.')
            readable,_,_=select.select([0,*terminals],[],[],.05)
            for fd in readable:
                if fd == 0:
                    data=os.read(0,4096)
                    if not data: return 0
                    stdin+=data
                    if len(stdin)>4096: raise ValueError('Panel input line too long.')
                    while b'\n' in stdin:
                        line,stdin=stdin.split(b'\n',1)
                        if line.strip()==b'__AERO_STOP__': return 0
                    continue
                t=terminals[fd]
                try: data=os.read(fd,16384)
                except OSError as exc:
                    if exc.errno==errno.EIO: raise ValueError('PX4 terminal kapandı: SYS '+str(t['system'])) from exc
                    raise
                if not data: raise ValueError('PX4 terminal EOF: SYS '+str(t['system']))
                t['history']=(t['history']+data)[-16000:]
                t['bytes']+=data
                while b'\n' in t['bytes']:
                    line,t['bytes']=t['bytes'].split(b'\n',1)
                    print('[SYS '+str(t['system'])+'] '+line.decode(errors='replace').rstrip('\r'), flush=True)
                if len(t['bytes'])>4096: t['bytes']=t['bytes'][-4096:]
                if not t['started'] and b'Startup script returned successfully' in t['history']:
                    t['started']=True; port=t['port']
                    t['queue']=[f"mavlink start -u {port} -o 14560 -t {manifest['gateway']} -m custom -r 50000"]
                    t['queue'] += [f'mavlink stream -u {port} -s {name} -r {rate}' for name,rate in [('HEARTBEAT',1),('ATTITUDE',20),('GLOBAL_POSITION_INT',10),('SYS_STATUS',1),('EXTENDED_SYS_STATE',2),('GPS_RAW_INT',5)]]
                if t['started'] and b'ERROR [mavlink]' in t['history']: raise ValueError('MAVLink yapılandırma hatası: SYS '+str(t['system']))
    except KeyboardInterrupt:
        return 0
    except (ValueError, OSError, json.JSONDecodeError) as exc:
        print('AERO_ERROR: '+str(exc), flush=True); return 2
    finally:
        # Only our process groups. No pkill, no WSL shutdown, no global Gazebo stop.
        signal.signal(signal.SIGTERM, signal.SIG_IGN); signal.signal(signal.SIGINT, signal.SIG_IGN)
        terminate_owned(processes)
        for fd in terminals: os.close(fd)
        if temp: temp.cleanup()


if __name__ == '__main__':
    sys.exit(fleet_main())
