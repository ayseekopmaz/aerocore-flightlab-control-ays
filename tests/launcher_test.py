import os
from pathlib import Path
import select
import subprocess
import sys
import tempfile
import time
import unittest

LAUNCHER = Path(__file__).resolve().parents[1] / 'simulator_launcher.py'

@unittest.skipIf(os.name != 'posix', 'PTY launcher executes inside Linux WSL')
class LauncherTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.repo = self.root / 'PX4'
        self.repo.mkdir()
        (self.repo / 'Makefile').write_text('')
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        self.env = dict(os.environ, PATH=str(self.bin), DISPLAY=':0')
        self.tool('gz', 'pass')
        self.tool('make', 'pass')
    def tearDown(self):
        self.temp.cleanup()
    def tool(self, name, body):
        file = self.bin / name
        file.write_text('#!' + sys.executable + '\n' + body + '\n')
        file.chmod(0o755)
    def run_helper(self, repo=None):
        return subprocess.run([sys.executable, '-u', str(LAUNCHER), str(repo or self.repo)],
                              input=b'', stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              env=self.env, timeout=5)
    def test_missing_repository(self):
        result = self.run_helper(self.root / 'missing')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'Makefile', result.stdout)
    def test_missing_gazebo(self):
        (self.bin / 'gz').unlink()
        result = self.run_helper()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'Gazebo gz', result.stdout)
    def test_missing_wslg(self):
        self.env.pop('DISPLAY', None)
        self.env.pop('WAYLAND_DISPLAY', None)
        result = self.run_helper()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'WSLg', result.stdout)
    def test_pty_command_and_owned_shutdown(self):
        self.tool('make', '''import signal,sys
signal.signal(signal.SIGINT,lambda *_:sys.exit(0))
print('MOCK_TTY:'+str(sys.stdin.isatty()),flush=True)
print('Startup script returned successfully',flush=True)
for line in sys.stdin:
    print('MOCK_COMMAND:'+line.strip(),flush=True)''')
        process = subprocess.Popen([sys.executable, '-u', str(LAUNCHER), str(self.repo)],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, env=self.env, bufsize=0)
        data = b''
        def read_until(token):
            nonlocal data
            deadline = time.monotonic() + 4
            while token not in data and time.monotonic() < deadline:
                ready, _, _ = select.select([process.stdout], [], [], .1)
                if ready:
                    chunk = os.read(process.stdout.fileno(), 8192)
                    if not chunk:break
                    data += chunk
            self.assertIn(token, data)
        try:
            read_until(b'Startup script returned successfully')
            self.assertIn(b'MOCK_TTY:True', data)
            process.stdin.write(b'mavlink status\n')
            read_until(b'MOCK_COMMAND:mavlink status')
            process.stdin.write(b'__AERO_STOP__\n')
            rest, _ = process.communicate(timeout=4)
            self.assertEqual(process.returncode, 0, data + rest)
        finally:
            if process.poll() is None:process.kill();process.wait()
    def test_profile_launch_builds_then_binds_owned_world(self, server_failure=False):
        source=self.repo/'Tools/simulation/gz'
        for name,xml in [('x500','<sdf version="1.9"><model name="x500"><include merge="true"><uri>model://x500_base</uri></include></model></sdf>'),('x500_base','<sdf version="1.9"><model name="x500_base"><link name="base_link"/></model></sdf>')]:
            path=source/'models'/name;path.mkdir(parents=True);(path/'model.sdf').write_text(xml)
        (source/'worlds').mkdir();(source/'worlds/default.sdf').write_text('<sdf version="1.9"><world name="default"/></sdf>')
        self.tool('make', "import sys\nprint('BUILD_ARGS:'+','.join(sys.argv[1:]),flush=True)")
        self.tool('gz', "import time,signal,sys,os\nif sys.argv[1:]==['service','-l']:\n    print('/world/aerocore_lab/scene/info');sys.exit(0)\nif os.environ.get('TEST_GAZEBO_EXIT'):sys.exit(7)\nsignal.signal(signal.SIGINT,lambda *_:sys.exit(0))\nwhile True: time.sleep(.1)")
        binary=self.repo/'build/px4_sitl_default/bin/px4';binary.parent.mkdir(parents=True)
        binary.write_text('#!'+sys.executable+"\nimport os,sys,signal\nsignal.signal(signal.SIGINT,lambda *_:sys.exit(0))\nprint('BIND_MODEL:'+os.environ.get('PX4_GZ_MODEL_NAME',''),flush=True)\nprint('GZ_COMMAND:'+os.environ.get('gz_command',''),flush=True)\nprint('Startup script returned successfully',flush=True)\nfor line in sys.stdin: print('MOCK_COMMAND:'+line.strip(),flush=True)\n");binary.chmod(0o755)
        import json
        configuration=json.dumps({'payload':0,'cgX':0,'cgY':0,'cgZ':0,'gpsNoise':0})
        if server_failure:
            self.env['TEST_GAZEBO_EXIT'] = '1'
            self.tool('gz', "import sys\nif sys.argv[1:]==['service','-l']:sys.exit(0)\nsys.exit(7)")
        launcher_args = getattr(self, 'profile_launcher_args', [str(LAUNCHER)])
        process=subprocess.Popen([sys.executable,'-u',*launcher_args,str(self.repo),configuration],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=self.env,bufsize=0)
        data=b''
        try:
            deadline=time.monotonic()+4
            while b'Startup script returned successfully' not in data and time.monotonic()<deadline:
                ready,_,_=select.select([process.stdout],[],[],.1)
                if ready:
                    chunk=os.read(process.stdout.fileno(),16384)
                    if not chunk: break
                    data+=chunk
            if server_failure:
                rest, _ = process.communicate(timeout=4)
                data += rest
                self.assertNotEqual(process.returncode, 0, data)
                self.assertIn(b'Gazebo process exited before world readiness', data)
                self.assertNotIn(b'BIND_MODEL:', data)
                return
            self.assertIn(b'BUILD_ARGS:px4_sitl_default',data);self.assertIn(b'BIND_MODEL:aerocore_x500_0',data);self.assertIn(b'AERO_PROFILE:',data);self.assertIn(b'AERO_FIRMWARE:',data)
            self.assertIn(b'GZ_COMMAND:gz', data)
            self.assertLess(data.index(b'AERO_GAZEBO_READY:'), data.index(b'BIND_MODEL:'))
            process.stdin.write(b'__AERO_STOP__\n');rest,_=process.communicate(timeout=4);self.assertEqual(process.returncode,0,data+rest)
        finally:
            if process.poll() is None:process.kill();process.wait()

    def test_embedded_profile_launch_matches_qt_session(self):
        profile = LAUNCHER.with_name('lab_profile.py').read_text()
        self.profile_launcher_args = ['-c', profile + '\n' + LAUNCHER.read_text()]
        self.test_profile_launch_builds_then_binds_owned_world()

    def test_gazebo_exit_is_reported_before_px4_starts(self):
        self.test_profile_launch_builds_then_binds_owned_world(server_failure=True)

    def test_child_failure_is_not_success(self):
        self.tool('make', "import sys\nprint('MOCK_FATAL',flush=True)\nsys.exit(9)")
        process = subprocess.Popen([sys.executable, '-u', str(LAUNCHER), str(self.repo)],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, env=self.env)
        try:
            process.wait(timeout=4)
            output = process.stdout.read()
            self.assertEqual(process.returncode, 9, output)
            self.assertIn(b'MOCK_FATAL', output)
        finally:
            if process.poll() is None:process.kill();process.wait()
            process.stdin.close();process.stdout.close()

if __name__ == '__main__':unittest.main()
