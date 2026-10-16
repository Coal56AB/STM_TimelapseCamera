#!/usr/bin/env python3
"""Exercise local conversion and camera HTTP downloads with the real FFmpeg."""
import argparse
import http.server
import importlib.util
import subprocess
import tempfile
import threading
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--ffmpeg', default='ffmpeg')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('video_mp4', root / 'Tools/video_mp4.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
data = (root / 'WeAct/build/host/compressed-merged.avi').read_bytes()


class Camera(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-Type', 'video/x-msvideo')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data[:1000] if self.path == '/broken' else data)


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Camera)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
try:
    with tempfile.TemporaryDirectory() as directory:
        directory = Path(directory)
        first = directory / 'local.mp4'
        module.convert(first, args.ffmpeg, root / 'WeAct/build/host/compressed-merged.avi')
        second = directory / 'wifi.mp4'
        module.convert(second, args.ffmpeg, url='http://127.0.0.1:{}/video'.format(server.server_port))
        assert first.read_bytes() == second.read_bytes()
        hashes = subprocess.check_output([args.ffmpeg, '-v', 'error', '-xerror', '-i', str(second), '-f', 'framemd5', '-'])
        assert len([line for line in hashes.splitlines() if line and not line.startswith(b'#')]) == 65
        broken = directory / 'broken.mp4'
        try:
            module.convert(broken, args.ffmpeg, url='http://127.0.0.1:{}/broken'.format(server.server_port))
        except (OSError, ValueError, RuntimeError):
            pass
        else:
            raise AssertionError('Truncated download was accepted')
        assert not broken.exists()
        try:
            module.convert(first, args.ffmpeg, root / 'WeAct/build/host/compressed-merged.avi')
        except FileExistsError:
            pass
        else:
            raise AssertionError('Existing file was overwritten')
        assert not list(directory.glob('.timelapse-*'))
finally:
    server.shutdown()
    server.server_close()
    thread.join()
print('PASS: local and HTTP MP4/H.264 conversion, all 65 frames, interrupted download cleanup, no overwrite')
