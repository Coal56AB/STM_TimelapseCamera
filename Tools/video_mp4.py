#!/usr/bin/env python3
"""Download a camera session as MP4/H.264, or convert an existing AVI."""
import argparse
import json
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.parse
import urllib.request
from pathlib import Path


def camera_url(camera, path, **query):
    return camera.rstrip('/') + path + '?' + urllib.parse.urlencode(query)


def list_sessions(camera):
    after = 0
    while True:
        with urllib.request.urlopen(camera_url(camera, '/api/recordings', after=after), timeout=30) as response:
            item = json.load(response)
        if item is None:
            return
        if not isinstance(item, dict) or not isinstance(item.get('id'), int) or not after < item['id'] <= 9999999:
            raise ValueError('Invalid recording list from camera')
        after = item['id']
        print('R{0:07d}: {1} frames, {2} fragments, {3:.1f} MiB{4}'.format(
            after, item.get('frames', 0), item.get('fragments', 0), item.get('size', 0) / 1048576,
            ' [damaged/incomplete]' if item.get('error') else ''))


def convert(output, ffmpeg, input_file=None, url=None):
    output = Path(output).resolve()
    if output.exists():
        raise FileExistsError('Output already exists: ' + str(output))
    if output.suffix.lower() != '.mp4':
        raise ValueError('Output filename must end with .mp4')
    # Keep unfinished output separate; publish only after download and encoding succeed.
    with tempfile.TemporaryDirectory(prefix='.timelapse-', dir=str(output.parent)) as directory:
        temporary = Path(directory) / 'video.mp4'
        command = [str(ffmpeg), '-hide_banner', '-loglevel', 'warning', '-nostdin', '-n',
                   '-i', str(input_file) if input_file else 'pipe:0', '-map', '0:v:0', '-an',
                   '-c:v', 'libx264', '-preset', 'fast', '-crf', '18', '-pix_fmt', 'yuv420p',
                   '-movflags', '+faststart', str(temporary)]
        process = subprocess.Popen(command, stdin=subprocess.PIPE if url else subprocess.DEVNULL)
        started = reported = time.monotonic()
        received = 0
        try:
            if url:
                with urllib.request.urlopen(url, timeout=30) as response:
                    expected = response.headers.get('Content-Length')
                    if expected is None or not expected.isdecimal() or int(expected) <= 0:
                        raise ValueError('Camera did not return a valid video length')
                    expected = int(expected)
                    while True:
                        block = response.read(65536)
                        if not block:
                            break
                        process.stdin.write(block)
                        received += len(block)
                        now = time.monotonic()
                        if now - reported >= 1:
                            print('\rDownload: {:.1f}%  {:.2f} MiB/s'.format(
                                received * 100 / expected, received / 1048576 / max(now - started, .001)),
                                end='', flush=True)
                            reported = now
                    if received != expected:
                        raise IOError('Download interrupted: {}/{} bytes'.format(received, expected))
                process.stdin.close()
            if process.wait() != 0:
                raise RuntimeError('FFmpeg could not encode the video')
            if not temporary.is_file() or not temporary.stat().st_size:
                raise RuntimeError('FFmpeg returned an empty video')
            # Exclusive creation also protects against another process creating the destination.
            with output.open('xb') as destination, temporary.open('rb') as source:
                try:
                    shutil.copyfileobj(source, destination, 1024 * 1024)
                except BaseException:
                    destination.close()
                    output.unlink()
                    raise
        except BaseException:
            if process.poll() is None:
                process.kill()
            process.wait()
            raise
        finally:
            if process.stdin and not process.stdin.closed:
                try:
                    process.stdin.close()
                except BrokenPipeError:
                    pass
        print('\nSaved: {} ({:.1f} MiB, {:.1f} s)'.format(
            output, output.stat().st_size / 1048576, time.monotonic() - started))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--list', action='store_true', help='list sessions on the camera')
    source.add_argument('--session', type=int, help='download and merge this session into MP4')
    source.add_argument('--input', type=Path, help='convert an existing AVI')
    parser.add_argument('--camera', default='http://192.168.4.1')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--ffmpeg', default='ffmpeg', help='FFmpeg executable, with libx264 support')
    args = parser.parse_args()
    if args.list:
        list_sessions(args.camera)
        return
    if args.session is not None and not 1 <= args.session <= 9999999:
        parser.error('--session must be between 1 and 9999999')
    if args.input is not None and not args.input.is_file():
        parser.error('input file does not exist')
    ffmpeg = shutil.which(args.ffmpeg)
    if not ffmpeg:
        parser.error('FFmpeg not found. Add it to PATH or pass --ffmpeg C:/path/ffmpeg.exe')
    output = args.output or (args.input.with_suffix('.mp4') if args.input else Path('R{:07d}.mp4'.format(args.session)))
    url = camera_url(args.camera, '/download', session=args.session, part=0) if args.session else None
    convert(output, ffmpeg, args.input, url)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, KeyboardInterrupt) as error:
        print('\nError: {}'.format(error), file=sys.stderr)
        sys.exit(1)
