#!/usr/bin/env python3
"""Independently inspect native MP4/H.264 output and decode it with FFmpeg."""
import argparse
import struct
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--ffmpeg', default='ffmpeg')
args = parser.parse_args()
build = Path(__file__).resolve().parents[1] / 'build/host'

for name, width, height, count, keys in [('h264-160.mp4', 160, 120, 30, [0, 25]),
                                       ('h264-320.mp4', 320, 240, 30, [0, 25]),
                                       ('merged.mp4', 160, 120, 65, [0, 25, 50])]:
    path = build / name
    data = path.read_bytes()
    u32 = lambda at: struct.unpack_from('>I', data, at)[0]

    def boxes(start, end):
        at = start
        while at < end:
            size = u32(at)
            assert size >= 8 and at + size <= end
            yield data[at + 4:at + 8], at, size
            at += size
        assert at == end

    top = list(boxes(0, len(data)))
    assert [item[0] for item in top[:3]] == [b'ftyp', b'moov', b'free']
    assert sum(item[2] for item in top[:3]) == 1024
    moov = top[1]
    headers = list(boxes(moov[1] + 8, moov[1] + moov[2]))
    mvhd = headers[0][1]
    assert u32(mvhd + 20) == 25 and u32(mvhd + 24) == count
    samples = top[3:]
    assert len(samples) == count * 2
    for frame in range(count):
        moof, mdat = samples[frame * 2:frame * 2 + 2]
        assert moof[0] == b'moof' and mdat[0] == b'mdat'
        members = list(boxes(moof[1] + 8, moof[1] + moof[2]))
        mfhd, traf = members
        assert mfhd[0] == b'mfhd' and u32(mfhd[1] + 12) == frame + 1
        tfhd, tfdt, trun = list(boxes(traf[1] + 8, traf[1] + traf[2]))
        assert tfhd[0] == b'tfhd' and tfdt[0] == b'tfdt' and trun[0] == b'trun'
        assert struct.unpack_from('>Q', data, tfdt[1] + 12)[0] == frame
        assert u32(trun[1] + 12) == 1 and u32(trun[1] + 20) == 1
        assert moof[1] + u32(trun[1] + 16) == mdat[1] + 8
        assert u32(trun[1] + 24) == mdat[2] - 8
        key = u32(trun[1] + 28) == 0x2000000
        assert key == (frame in keys)
        at, end = mdat[1] + 8, mdat[1] + mdat[2]
        types = []
        while at < end:
            size = u32(at)
            assert size > 0 and at + 4 + size <= end
            types.append(data[at + 4] & 31)
            at += 4 + size
        assert at == end and (5 in types) == key

    common = [args.ffmpeg, '-v', 'error', '-xerror', '-i', str(path)]
    frames = subprocess.check_output(common + ['-f', 'framemd5', '-'])
    assert len([line for line in frames.splitlines() if line and not line.startswith(b'#')]) == count
    rgb = subprocess.check_output(common + ['-frames:v', '1', '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-'])
    assert len(rgb) == width * height * 3
    def pixel(x, y): return rgb[(y * width + x) * 3:(y * width + x) * 3 + 3]
    red, cyan = pixel(width - 20, 20), pixel(width - 20, height - 30)
    assert red[0] > 180 and max(red[1:]) < 80
    assert cyan[0] < 80 and min(cyan[1:]) > 160
    white = sum(min(pixel(x, y)) > 170 for y in range(height - 15, height - 2) for x in range(2, 116))
    assert white > 50
    # Seek into the last GOP; verifies the merged timestamps and decoder references.
    subprocess.run([args.ffmpeg, '-v', 'error', '-xerror', '-ss', str((count - 2) / 25),
                    '-i', str(path), '-frames:v', '1', '-f', 'null', '-'], check=True)
    print('PASS:', name, 'MP4 boxes, AVC samples, continuous timeline, all frames, colors, timestamp, seek')
