import importlib.util
from pathlib import Path
import tempfile

module_path = Path(__file__).resolve().parents[1] / 'discover_px4.py'
spec = importlib.util.spec_from_file_location('aero_discover_px4', module_path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

with tempfile.TemporaryDirectory() as directory:
    home = Path(directory)
    assert module.locate(home) is None
    invalid = home / 'PX4-Autopilot'
    invalid.mkdir()
    assert module.locate(home) is None  # A matching directory name is insufficient.
    nested = home / 'src' / 'renamed-flight-stack'
    (nested / 'Tools' / 'simulation').mkdir(parents=True)
    (nested / 'platforms').mkdir()
    (nested / 'CMakeLists.txt').write_text('project(PX4)')
    assert module.locate(home) == nested.resolve()
    original = home / 'PX4-Autopilot'
    (original / 'Tools' / 'simulation').mkdir(parents=True)
    (original / 'platforms').mkdir()
    (original / 'CMakeLists.txt').write_text('project(PX4)')
    assert module.locate(home) == original.resolve()
