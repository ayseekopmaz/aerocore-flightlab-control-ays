"""Read-only, bounded PX4 source tree discovery inside the chosen WSL distro."""

from pathlib import Path
import os


def locate(home: Path) -> Path | None:
    home = home.resolve()
    if not home.is_dir():
        return None

    def valid(path: Path) -> bool:
        return ((path / "CMakeLists.txt").is_file()
                and (path / "Tools" / "simulation").is_dir()
                and (path / "platforms").is_dir())

    # Normal clones first; do not search mounted Windows drives or unrelated users.
    for parent in (home, home / "src", home / "workspace", home / "projects"):
        candidate = parent / "PX4-Autopilot"
        if valid(candidate):
            return candidate.resolve()

    ignored = {".cache", ".local", ".git", ".venv", "build",
               "node_modules", ".cargo", ".rustup", ".vscode"}
    for root, dirs, _ in os.walk(home):
        depth = len(Path(root).relative_to(home).parts)
        dirs[:] = sorted(d for d in dirs if d not in ignored and not d.startswith(".")) if depth < 4 else []
        if valid(Path(root)):
            return Path(root).resolve()
    return None


if __name__ == "__main__":
    result = locate(Path.home())
    if result:
        print(result)
