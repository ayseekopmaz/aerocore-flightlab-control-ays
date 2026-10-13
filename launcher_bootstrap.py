"""Short Windows/WSL bootstrap. Bulk source/config travels on stdin, not argv."""
import json
import os
from pathlib import Path
import sys
import tempfile


def bootstrap():
    payload = bytearray()
    # Read exactly the handshake line: preserve later PX4/stop commands on stdin.
    while True:
        b = os.read(0, 1)
        if not b:
            raise ValueError('Başlatıcı verisi tamamlanmadan giriş kapandı.')
        if b == b'\n':
            break
        payload.extend(b)
        if len(payload) > 4 * 1024 * 1024:
            raise ValueError('Başlatıcı verisi 4 MiB sınırını aştı.')
    data = json.loads(payload)
    if not isinstance(data, dict) or set(data) != {'source', 'repository', 'config'}:
        raise ValueError('Başlatıcı veri biçimi geçersiz.')
    if not isinstance(data['source'], str) or not isinstance(data['repository'], str) or not isinstance(data['config'], dict):
        raise ValueError('Başlatıcı veri türleri geçersiz.')
    with tempfile.TemporaryDirectory(prefix='aerocore-launch-') as folder:
        script = Path(folder) / 'runner.py'
        script.write_text(data['source'], encoding='utf-8')
        config = Path(folder) / 'config.json'
        config.write_text(json.dumps(data['config']), encoding='utf-8')
        sys.argv = [str(script), data['repository'], config.read_text(encoding='utf-8')]
        namespace = {'__name__': '__main__', '__file__': str(script)}
        exec(compile(script.read_text(encoding='utf-8'), str(script), 'exec'), namespace)


if __name__ == '__main__':
    try:
        bootstrap()
    except (ValueError, OSError) as exc:
        print('AERO_ERROR: Başlatıcı aktarımı başarısız: ' + str(exc), flush=True)
        sys.exit(2)
