"""Run a command with UTC process time and retain timestamped, unmodified output lines."""
import argparse
from datetime import datetime, timezone
import os
from pathlib import Path
import subprocess
import sys


def utc_now():
    return datetime.now(timezone.utc).isoformat(timespec='milliseconds').replace('+00:00', 'Z')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log', required=True, type=Path)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command
    if command[:1] == ['--']:
        command = command[1:]
    if not command:
        parser.error('a command is required after --')
    args.log.parent.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment['TZ'] = 'UTC'
    with args.log.open('w', encoding='utf-8', newline='\n') as log:
        def emit(message):
            line = f'{utc_now()} {message}'
            print(line, flush=True)
            log.write(line + '\n')
            log.flush()
        emit(f'COMMAND {command!r}; cwd={str(Path.cwd())!r}; TZ=UTC')
        try:
            with subprocess.Popen(command, env=environment, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True, encoding='utf-8',
                                  errors='replace') as process:
                for line in process.stdout:
                    emit(line.rstrip('\r\n'))
                code = process.wait()
        except OSError as error:
            emit(f'LAUNCH FAILED: {error}')
            return 1
        emit(f'EXIT {code}')
        return code


if __name__ == '__main__':
    sys.exit(main())
