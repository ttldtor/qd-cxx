"""Acquire only the hash-pinned Windows formatter archives into ignored local storage."""
import hashlib
import json
from pathlib import Path
import urllib.request
import zipfile


def main():
    root = Path(__file__).resolve().parent
    cache = root / '.work'
    cache.mkdir(exist_ok=True)
    for tool in json.loads((root / 'cpp-tools.lock.json').read_text()):
        archive = cache / tool['url'].rsplit('/', 1)[1]
        if not archive.exists():
            urllib.request.urlretrieve(tool['url'], archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != tool['sha256']:
            raise RuntimeError(f'Integrity mismatch: {archive}')
        destination = cache / f"{tool['name']}-{tool['version']}"
        with zipfile.ZipFile(archive) as package:
            for name in package.namelist():
                if not (destination / name).resolve().is_relative_to(destination.resolve()):
                    raise RuntimeError(f'Unsafe archive entry: {name}')
            package.extractall(destination)
        executable = cache / tool['executable']
        if not executable.is_file():
            raise RuntimeError(f'Missing executable: {executable}')
        print(executable)


if __name__ == '__main__':
    main()
