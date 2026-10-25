"""Supply the OTA upload password for the esp32dev_ota environment.

Uses PANEL_OTA_PASSWORD when set (for example the panel's previous password
while changing it), otherwise SECRET_OTA_PASSWORD from include/secrets.h.
Runs as a post: script, after the platform has defined the espota flags.
"""
from pathlib import Path
import os
import re

Import('env')  # type: ignore[name-defined]  # Provided by PlatformIO/SCons.

password = os.environ.get('PANEL_OTA_PASSWORD', '')
source = 'PANEL_OTA_PASSWORD'
if not password:
    secrets = Path(env['PROJECT_DIR']) / 'include/secrets.h'
    match = re.search(r'#define\s+SECRET_OTA_PASSWORD\s+"([^"]*)"',
                      secrets.read_text()) if secrets.is_file() else None
    password = match.group(1) if match else ''
    source = 'include/secrets.h'
if not password:
    raise SystemExit('No OTA password: set SECRET_OTA_PASSWORD in include/secrets.h')
# espota's --debug prints its options, including the password; leave it out.
env.Replace(UPLOADERFLAGS=[flag for flag in env['UPLOADERFLAGS'] if flag != '--debug'])
env.Append(UPLOADERFLAGS=['--auth=' + password])
print(f'OTA password from {source}')
