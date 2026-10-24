"""Keep the flash-resident settings page in sync with its editable HTML source."""
from pathlib import Path
import subprocess
import sys

Import('env')  # type: ignore[name-defined]  # Provided by PlatformIO/SCons.
subprocess.run([sys.executable, str(Path(env['PROJECT_DIR']) / 'tools/embed_web.py')], check=True)
