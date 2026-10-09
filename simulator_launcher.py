"""Owned PX4/Gazebo SITL process launcher; Python standard library only."""
import errno
import json
import os
from pathlib import Path
import select
import shutil
import signal
import subprocess
import sys
import time


def fail(message, code=2):
    print('AERO_ERROR: ' + message, flush=True)
    return code


def main():
    if len(sys.argv) not in (2, 3):
        return fail('PX4 proje klasörü belirtilmedi.')
    repo = Path(sys.argv[1]).expanduser().resolve()
    if not (repo / 'Makefile').is_file():
        return fail('PX4 Makefile bulunamadı: ' + str(repo))
    if not shutil.which('make'):
        return fail('WSL içinde make bulunamadı.')
    if not shutil.which('gz'):
        return fail('WSL içinde Gazebo gz bulunamadı. WSL ortamınızın PATH ayarını kontrol edin.')
    if not os.environ.get('DISPLAY') and not os.environ.get('WAYLAND_DISPLAY'):
        return fail('WSLg görüntü ortamı yok: DISPLAY / WAYLAND_DISPLAY tanımlı değil.')

    lab_temp = None
    environment = os.environ.copy()
    if len(sys.argv) == 3:
        try:
            # Qt embeds lab_profile.py and this file in one python -c script.
            # A local import of the same name would shadow that global function.
            profile_factory = globals().get('prepare_lab_profile')
            if profile_factory is None:
                from lab_profile import prepare_lab_profile as profile_factory
            lab_temp, environment = profile_factory(repo, json.loads(sys.argv[2]))
        except (ValueError, OSError) as exc:
            return fail('Test model profili hazırlanamadı: ' + str(exc))

    # Use a real PTY for pxh, but do not depend on nested bash/script quoting.
    import fcntl
    import termios
    master, slave = os.openpty()

    def attach_terminal():
        os.setsid()
        fcntl.ioctl(slave, termios.TIOCSCTTY, 0)

    try:
        command = ['make', 'px4_sitl', 'gz_x500']
        if lab_temp:
            # Build separately: the ready-made gz_x500 target overrides model/world env.
            wrapper = r"""
import os, sys, subprocess, time
from pathlib import Path
repo=Path(sys.argv[1]); world=Path(sys.argv[2])
code=subprocess.call(['make','px4_sitl_default'],cwd=repo)
if code: sys.exit(code)
binary=repo/'build/px4_sitl_default/bin/px4'
if not binary.is_file():
    print('AERO_ERROR: PX4 binary derleme sonrası bulunamadı.',flush=True);sys.exit(2)
rootfs=repo/'build/px4_sitl_default/rootfs'
cwd=rootfs if rootfs.is_dir() else repo
print('AERO_GAZEBO: world='+str(world)+' partition='+os.environ.get('GZ_PARTITION',''),flush=True)
sim=subprocess.Popen(['gz','sim','-r','-v','4',str(world)])
px4=None
try:
    # Starting a process does not prove that its world is discoverable.
    # Probe in the same partition/environment that PX4 will inherit.
    service='/world/aerocore_lab/scene/info'
    deadline=time.monotonic()+60
    last_probe='No service-list response'
    while True:
        if sim.poll() is not None:
            print('AERO_ERROR: Gazebo process exited before world readiness; code='+str(sim.returncode),flush=True)
            sys.exit(2)
        try:
            probe=subprocess.run(['gz','service','-l'],capture_output=True,text=True,timeout=3)
            services=probe.stdout.splitlines()
            if probe.returncode==0 and service in [line.strip() for line in services]:
                print('AERO_GAZEBO_READY: '+service,flush=True)
                break
            last_probe=(probe.stderr+'\n'+probe.stdout).strip()[-1800:]
        except subprocess.TimeoutExpired:
            last_probe='gz service -l timed out'
        if time.monotonic()>=deadline:
            print('AERO_ERROR: Gazebo world service not ready after 60s: '+service+'; '+last_probe,flush=True)
            sys.exit(2)
        time.sleep(.25)
    px4=subprocess.Popen([str(binary)],cwd=cwd)
    sys.exit(px4.wait())
except KeyboardInterrupt:
    sys.exit(0)
finally:
    for process in (px4,sim):
        if process is not None and process.poll() is None:
            process.terminate()
            try: process.wait(timeout=3)
            except subprocess.TimeoutExpired: process.kill();process.wait()
"""
            command = [sys.executable, '-u', '-c', wrapper, str(repo), str(Path(lab_temp.name)/'worlds/aerocore_lab.sdf')]
        child = subprocess.Popen(command, cwd=repo,
                                 stdin=slave, stdout=slave, stderr=slave,
                                 preexec_fn=attach_terminal, close_fds=True, env=environment)
    except OSError as exc:
        os.close(master)
        os.close(slave)
        if lab_temp: lab_temp.cleanup()
        return fail('PX4 süreç başlatma hatası: ' + str(exc))
    os.close(slave)
    print('AERO_LAUNCHER_READY: PX4 / Gazebo işlemi başlatıldı.', flush=True)
    stopping = False
    stop_at = 0.0

    def stop(*_):
        nonlocal stopping, stop_at
        if stopping:
            return
        stopping, stop_at = True, time.monotonic()
        try:
            os.killpg(child.pid, signal.SIGINT)
        except ProcessLookupError:
            pass

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    input_bytes = b''
    stdin_open = True
    master_open = True
    startup_output = b''
    fingerprinted = False
    try:
        while child.poll() is None:
            readers = ([master] if master_open else []) + ([0] if stdin_open else [])
            ready, _, _ = select.select(readers, [], [], 0.2)
            if master in ready:
                try:
                    data = os.read(master, 16384)
                except OSError as exc:
                    if exc.errno != errno.EIO:
                        raise
                    data = b''
                if data:
                    startup_output = (startup_output + data)[-16000:]
                    if not fingerprinted and b'Startup script returned successfully' in startup_output:
                        fingerprinted = True
                        binary = repo / 'build/px4_sitl_default/bin/px4'
                        if binary.is_file():
                            import hashlib
                            print('AERO_FIRMWARE: ' + hashlib.sha256(binary.read_bytes()).hexdigest(), flush=True)
                    sys.stdout.buffer.write(data)
                    sys.stdout.buffer.flush()
                else:
                    master_open = False
            if 0 in ready:
                data = os.read(0, 4096)
                if not data:
                    stdin_open = False
                    stop()
                else:
                    input_bytes += data
                    while b'\n' in input_bytes:
                        line, input_bytes = input_bytes.split(b'\n', 1)
                        if line.strip() == b'__AERO_STOP__':
                            stop()
                        elif not stopping and master_open:
                            os.write(master, line + b'\n')
            if stopping and time.monotonic() - stop_at > 8:
                try:
                    os.killpg(child.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                break
        child.wait(timeout=3)
        os.set_blocking(master, False)
        while True:
            try:
                tail = os.read(master, 16384)
            except (BlockingIOError, OSError):
                break
            if not tail:
                break
            sys.stdout.buffer.write(tail)
            sys.stdout.buffer.flush()
    finally:
        if child.poll() is None:
            try:
                os.killpg(child.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            child.wait()
        os.close(master)
        if lab_temp: lab_temp.cleanup()
    print('AERO_LAUNCHER_EXIT: ' + str(child.returncode), flush=True)
    return 0 if stopping else (child.returncode if child.returncode >= 0 else 128 - child.returncode)


if __name__ == '__main__':
    sys.exit(main())
