#!/usr/bin/env python3
"""Run host regressions and render the actual firmware/GFX frames into documentation GIFs.

Requires a C++17 compiler, installed PlatformIO GFX dependency, and Pillow.
The fixtures replace GPIO, network and system time. Images are examples, not photos.
"""
from pathlib import Path
import json
import hashlib
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    from PIL import Image, ImageDraw
    gfx = ROOT / '.pio/libdeps/esp32dev/Adafruit GFX Library'
    if not (gfx / 'Adafruit_GFX.cpp').is_file():
        raise SystemExit('Install project libraries first: pio pkg install')
    compiler = os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        raise SystemExit('A C++17 compiler is required for the host renderer.')
    with tempfile.TemporaryDirectory(prefix='red-panel-render-') as folder:
        temporary = Path(folder)
        shutil.copyfile(ROOT / 'src/main.cpp', temporary / 'app_under_test.cpp')
        executable = temporary / 'regression'
        capture = temporary / 'frames.json'
        subprocess.run([compiler, '-std=c++17', '-DARDUINO=100', '-fsanitize=address,undefined',
                        '-fno-omit-frame-pointer', '-I' + str(temporary),
                        '-I' + str(ROOT / 'test_host/stubs'), '-I' + str(ROOT / 'include'),
                        '-I' + str(gfx), str(ROOT / 'test_host/regression.cpp'),
                        str(gfx / 'Adafruit_GFX.cpp'), str(ROOT / 'src/debug.cpp'),
                        '-o', str(executable)], check=True)
        subprocess.run([str(executable), str(capture)], check=True)
        scenes = json.loads(capture.read_text())
        # Two fixed secrets.h variants (the copied source's own folder wins the include
        # search): defaults with OTA/login disabled, then both passwords set.
        variants = {'network': ('PanelSetup32', '', ''), 'network-secured': ('PanelSetup32', 'pagepass', 'otapass')}
        for folder, (ap, page, ota) in variants.items():
            network = temporary / folder
            network.mkdir()
            for name in ('PanelNetwork.cpp', 'PanelNetwork.h'):
                shutil.copyfile(ROOT / 'src' / name, network / name)
            (network / 'secrets.h').write_text(
                f'#pragma once\n#define SECRET_AP_PASSWORD "{ap}"\n#define SECRET_PANEL_PASSWORD "{page}"\n'
                f'#define SECRET_OTA_PASSWORD "{ota}"\n')
            network_test = network / 'regression'
            network_status = network / 'status.json'
            subprocess.run([compiler, '-std=c++17', '-fsanitize=address,undefined',
                            '-fno-omit-frame-pointer', '-I' + str(network),
                            '-I' + str(ROOT / 'test_host/network_stubs'), '-I' + str(ROOT / 'include'),
                            str(ROOT / 'test_host/network_regression.cpp'), str(network / 'PanelNetwork.cpp'),
                            str(ROOT / 'src/debug.cpp'), '-o', str(network_test)], check=True)
            subprocess.run([str(network_test), str(network_status)], check=True)
        network_status = temporary / 'network/status.json'
        status = json.loads(network_status.read_text())
        assert status['location'] == 'Workshop' and status['zone'] == 'Etc/UTC'
        assert status['timeValid'] and status['connected'] and status['time']
        assert status['device']['pins']['SK'] == 18 and len(status['zones']) == 15
        driver_test = temporary / 'refresh-regression'
        subprocess.run([compiler, '-std=c++17', '-DARDUINO=100', '-fsanitize=address,undefined',
                        '-fno-omit-frame-pointer', '-I' + str(ROOT / 'test_host/driver_stubs'),
                        '-I' + str(ROOT / 'test_host/stubs'), '-I' + str(ROOT / 'include'),
                        '-I' + str(gfx), str(ROOT / 'test_host/driver_regression.cpp'),
                        str(ROOT / 'src/Hub08Panel.cpp'), str(gfx / 'Adafruit_GFX.cpp'),
                        str(ROOT / 'src/debug.cpp'), '-o', str(driver_test)], check=True)
        subprocess.run([str(driver_test)], check=True)
    output = ROOT / 'images/demos'
    output.mkdir(parents=True, exist_ok=True)
    manifest = []
    for index, scene in enumerate(scenes, 1):
        name = ''.join(ch if ch.isalnum() else '-' for ch in scene['name'].lower()).strip('-')
        frames = []
        for frame in scene['frames']:
            image = Image.new('RGB', (640, 160), (17, 26, 33))
            draw = ImageDraw.Draw(image)
            bits = bytes.fromhex(frame['bitmap'])
            red = 45 + round(210 * frame['brightness'] / 255)
            for y in range(16):
                for x in range(64):
                    on = bits[y*8+x//8] & (0x80 >> (x%8))
                    colour = (red, round(red*.13), round(red*.17)) if on else (33, 44, 52)
                    draw.ellipse((x*10+2, y*10+2, x*10+8, y*10+8), fill=colour)
            frames.append(image)
        filename = f'{index:02d}-{name}.gif'
        frames[0].save(output / filename, save_all=True, append_images=frames[1:],
                       duration=max(20, scene['interval']), loop=0, optimize=False, disposal=2)
        manifest.append({'file': filename, 'name': scene['name'], 'key': scene['key'],
                         'frames': len(frames), 'intervalMs': scene['interval'], 'width': 640, 'height': 160,
                         'sha256': hashlib.sha256((output / filename).read_bytes()).hexdigest()})
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    descriptions = {
        'Boot 1': 'Startup header and firmware version.',
        'Boot 2': 'Panel geometry, scan ratio and selected brightness.',
        'Boot 3': 'Network/time status. This fixture is connected and synchronised; a fresh offline boot shows waiting states instead.',
        'LETTERS': 'Key 1, or t to hold: A-H above I-P, one letter per 8x8 module.',
        'CHECKERBOARD': 'Key 2: alternating single-pixel phases.',
        'BOUNCING BALLS': 'Key 3: independent moving circles, filled and outlined.',
        'STARFIELD': 'Key 4: several speeds and short horizontal star trails.',
        'PIXEL RAIN': 'Key 5: staggered falling trails.',
        'ANIMATED BARS': 'Key 6: simulated equalizer bars; no audio input.',
        "CONWAY'S LIFE": 'Key 7: actual glider/blinker updates on wrapping edges.',
        'BEATING HEARTS': 'Key 8: enlarged pixel art with whole-display brightness modulation.',
        'SCROLLING TEXT': 'Key 9 or s: full-height text crossing the screen.',
        'PONG': 'Key g: automatic paddles, collisions, goals and fresh serves.',
        'CLOCK': 'Starts the cycle and returns for 10 seconds between demos. Key c holds it indefinitely: local HH:MM:SS, blinking colons, seconds bar and NTP source label. Captured with a fixed network-time fixture.',
        'All pixels off': 'Key f: an empty pixel buffer at the selected brightness.',
        'All pixels on': 'Key o: every pixel on at the selected brightness.',
        'Output blanked': 'Key b: output disabled. Visually dark like f; it uses zero output on-time.',
        'Clock offline holdover': 'Time continues from its last sync; HLD replaces the NTP label.',
        'Clock waiting for Wi-Fi': 'No valid time yet: NO WIFI / JOIN AP. No sample epoch is displayed.',
        'Clock waiting for NTP': 'Wi-Fi is connected, but time has not arrived: SYNC TIME / WAIT NTP.',
    }
    gallery = [
        '# Screen output examples',
        'Generated from the actual `src/main.cpp` drawing routines and installed Adafruit GFX library, using host fixtures for panel GPIO, Wi-Fi and time. These are **rendered examples, not photographs or proof of hardware operation**.',
        'Each dot represents one pixel. GIFs sample animation frames to keep the archive compact; dot colour and brightness are illustrative, not calibrated light measurements. Clock values come from a fixed host time and do not represent the time when you open this document.',
        'Use the [panel guide](PANEL_GUIDE.md) for controls and the [ESP32 test configuration](ESP32_TEST_CONFIG.md) for wiring, Wi-Fi and bench checks.',
    ]
    for index, item in enumerate(manifest, 1):
        title = item['name'].capitalize() if item['name'].isupper() else item['name']
        gallery.extend([f'## {index:02d} — {title}',
                        descriptions[item['name']],
                        f'![Rendered {item["name"]} output](../images/demos/{item["file"]})'])
    gallery.extend(['## Regenerate and verify',
                    'Install the project libraries with `pio pkg install`. With a C++17 compiler and the dependencies in `tools/docs-requirements.txt`, run:',
                    '```sh\npython3 tools/render_examples.py\npython3 tools/build_docs.py\n```',
                    'The renderer runs host regressions with address/undefined-behaviour sanitizers before writing these GIFs and their [manifest](../images/demos/manifest.json). Tests cover clock/demo alternation and deadlines, controls, Conway rules, Pong, live-clock/DST/holdover, settings validation, portal/LAN routes, NVS failure/reload and reconnects. The actual refresh driver is exercised with timer/GPIO fixtures for scan-boundary frames, OE deadlines, late rows and brightness. Real interrupt latency, Wi-Fi association, captive portals and electrical output require the bench checks.',
                    'Original panel photographs remain separate in the [hardware photo log](PHOTO_LOG.md).'])
    (ROOT / 'docs/SCREEN_EXAMPLES.md').write_text('\n\n'.join(gallery) + '\n')
    print(f'Rendered {len(manifest)} firmware examples in images/demos/')

if __name__ == '__main__':
    main()
