#!/usr/bin/env python3
"""Local UI preview with example status. Does not connect to or configure a real ESP32."""
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from urllib.parse import parse_qs
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
ZONES = re.findall(r'\{"([A-Za-z_/]+)",', (ROOT / 'include/ClockSettings.h').read_text())
STATE = {
    'version': '1.7.0', 'ota': 'ready', 'token': 'preview-only-token', 'location': 'Sydney',
    'zone': 'Australia/Sydney', 'brightness': 90, 'startClock': False,
    'clockHold': 10, 'scrollText': '',
    'nightMode': True, 'nightStart': '22:00', 'nightEnd': '06:00', 'nightActive': False,
    'displayOn': True, 'displayManual': False,
    'connected': True, 'timeValid': True, 'ssid': 'Panel-Test-2G', 'rssi': -52,
    'url': 'http://192.168.1.80/panel', 'uptime': 120, 'time': '23:59:58',
    'date': 'Sat 24 Oct 2026 AEDT', 'lastSyncAge': 12,
    'ntpServers': ['pool.ntp.org', 'time.cloudflare.com'], 'zones': ZONES,
    'device': {'panel': '64x16 HUB08', 'board': 'esp32dev', 'host': 'red-panel-ABCDEF',
               'rowPeriodUs': 500, 'signalDelayUs': 1, 'guardUs': 50,
               'dataActiveLow': True, 'oeActiveLow': True,
               'pins': {'LA': 25, 'LB': 26, 'LC': 27, 'LD': 13, 'EN': 22, 'R1': 23, 'LT': 4, 'SK': 18}},
}

class Handler(BaseHTTPRequestHandler):
    def reply(self, code, value):
        body = json.dumps(value).encode()
        self.send_response(code)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == '/api/status':
            self.reply(200, STATE)
        elif self.path in ('/', '/panel'):
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.end_headers()
            self.wfile.write((ROOT / 'web/index.html').read_bytes())
        else:
            self.send_error(404)

    def do_POST(self):
        values = {key: value[0] for key, value in parse_qs(self.rfile.read(int(self.headers.get('Content-Length', 0))).decode()).items()}
        if values.get('token') != STATE['token']:
            return self.reply(403, {'message': 'Reload the settings page and try again.'})
        if self.path == '/api/settings':
            location = values.get('location', '').strip()
            brightness = values.get('brightness', '')
            hold = values.get('clockHold', '')
            text = values.get('scrollText', '')
            if not (1 <= len(location.encode()) <= 32 and all(ord(c) >= 32 for c in location)
                    and values.get('zone') in ZONES and brightness.isascii() and brightness.isdigit()
                    and 0 <= int(brightness) <= 255 and values.get('view') in ('playlist', 'clock')
                    and hold.isascii() and hold.isdigit() and 5 <= int(hold) <= 3600
                    and len(text) <= 64 and all(' ' <= c <= '~' for c in text)
                    and values.get('nightMode') in ('on', 'off')
                    and all(re.fullmatch(r'([01]\d|2[0-3]):[0-5]\d', values.get(k, ''))
                            for k in ('nightStart', 'nightEnd'))):
                return self.reply(400, {'message': 'Invalid settings.'})
            STATE.update(location=location, zone=values['zone'], brightness=int(brightness),
                         startClock=values['view'] == 'clock', clockHold=int(hold), scrollText=text,
                         nightMode=values['nightMode'] == 'on', nightStart=values['nightStart'],
                         nightEnd=values['nightEnd'])
            self.reply(200, {'message': 'Saved and applied. Preview state only.'})
        elif self.path == '/api/display':
            if values.get('state') not in ('on', 'off'):
                return self.reply(400, {'message': 'Choose on or off.'})
            STATE.update(displayOn=values['state'] == 'on', displayManual=True)
            self.reply(200, {'message': 'Display ' + values['state'] + '. Preview only.'})
        elif self.path == '/api/sync':
            self.reply(200, {'message': 'Network time requested. Preview only.'})
        elif self.path == '/api/wifi':
            self.reply(200, {'message': 'Setup hotspot requested. Preview only.'})
        else:
            self.send_error(404)

    def log_message(self, *_):
        pass

if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    print(f'UI preview (mock status only): http://127.0.0.1:{port}/panel', flush=True)
    HTTPServer(('127.0.0.1', port), Handler).serve_forever()
