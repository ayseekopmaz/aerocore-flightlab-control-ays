"""Gazebo transport action, argv only; no shell commands or global process termination."""
import json
import os
import math
import subprocess
import sys


def main():
    try:
        if len(sys.argv)>3: os.environ['GZ_PARTITION']=sys.argv[3]
        speed, direction = float(sys.argv[1]), float(sys.argv[2])
        if not (math.isfinite(speed) and math.isfinite(direction) and 0 <= speed <= 15 and 0 <= direction <= 360):
            raise ValueError('Rüzgâr sınırı geçersiz.')
        # ENU wind direction is travel direction: 0 = North, 90 = East.
        angle = math.radians(direction)
        topic = '/world/aerocore_lab/wind'
        info = subprocess.run(['gz', 'topic', '-i', '-t', topic], capture_output=True, text=True, timeout=5)
        if info.returncode or 'Subscribers' not in info.stdout:
            raise ValueError('WindEffects abonesi bulunamadı; AeroCore test dünyası gerekli.')
        import re
        match = re.search(r'Subscribers\s*\[\s*(\d+)\s*\]', info.stdout)
        subscriber_section = info.stdout.split('Subscribers', 1)[-1]
        subscriber_section = subscriber_section.split('Publishers', 1)[0]
        has_address = re.search(r'(?:tcp|udp)://', subscriber_section) is not None
        if not ((match and int(match.group(1)) > 0) or has_address):
            raise ValueError('WindEffects abonesi yok; olay uygulanmadı.')
        message = f'linear_velocity: {{ x: {speed*math.sin(angle)} y: {speed*math.cos(angle)} z: 0 }} enable_wind: true'
        sent = subprocess.run(['gz', 'topic', '-t', topic, '-m', 'gz.msgs.Wind', '-p', message], capture_output=True, text=True, timeout=5)
        if sent.returncode: raise ValueError(sent.stderr or sent.stdout or 'Rüzgâr yayınlanamadı.')
        # Publication to an existing subscriber is not an aerodynamic calibration.
        print(json.dumps({'ok': True, 'evidence': 'Gazebo WindEffects subscriber + transport publication', 'speed': speed, 'direction': direction}))
        return 0
    except (ValueError, OSError, subprocess.TimeoutExpired, IndexError) as exc:
        print(json.dumps({'ok': False, 'error': str(exc)})); return 2


if __name__ == '__main__': sys.exit(main())
