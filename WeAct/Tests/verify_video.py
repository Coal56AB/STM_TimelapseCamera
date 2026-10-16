#!/usr/bin/env python3
"""Independent MJPEG/OpenDML checks and FFmpeg decoding of test recordings."""
import argparse
import struct
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--ffmpeg', default='ffmpeg')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1] / 'build/host'


def check(path):
    data = path.read_bytes()
    word = lambda at: struct.unpack_from('<I', data, at)[0]

    def chunks(start, end):
        while start < end:
            size = word(start + 4)
            assert start + 8 + size <= end
            yield data[start:start + 4], start + 8, size
            start += 8 + size + (size & 1)
        assert start == end

    all_frames, supers = [], []
    for number, (tag, riff, length) in enumerate(chunks(0, len(data))):
        assert tag == b'RIFF' and data[riff:riff + 4] == (b'AVI ' if number == 0 else b'AVIX')
        parts = list(chunks(riff + 4, riff + length))
        if not number:
            hdrl = parts.pop(0)
            headers = list(chunks(hdrl[1] + 4, hdrl[1] + hdrl[2]))
            strl = headers[1]
            stream = list(chunks(strl[1] + 4, strl[1] + strl[2]))
            assert data[stream[0][1]:stream[0][1] + 8] == b'vidsMJPG'
            assert data[stream[1][1] + 16:stream[1][1] + 20] == b'MJPG'
            if len(stream) == 3:
                index = stream[2]
                supers = [struct.unpack_from('<QII', data, index[1] + 24 + i * 16) for i in range(word(index[1] + 4))]
        movi, index = parts
        frames = list(chunks(movi[1] + 4, movi[1] + movi[2]))
        for i, (kind, at, size) in enumerate(frames):
            assert kind == b'00dc' and data[at:at + 2] == b'\xff\xd8' and data[at + size - 2:at + size] == b'\xff\xd9'
            if index[0] == b'idx1':
                tag, flags, offset, count = struct.unpack_from('<4sIII', data, index[1] + i * 16)
                assert tag == kind and flags == 16 and movi[1] + offset == at - 8 and count == size
            else:
                assert index[0] == b'ix00' and word(index[1] + 4) == len(frames)
                assert supers[number] == (index[1] - 8, index[2] + 8, len(frames))
                base = struct.unpack_from('<Q', data, index[1] + 12)[0]
                offset, count = struct.unpack_from('<II', data, index[1] + 24 + i * 8)
                assert base + offset == at and count == size
            all_frames.append(data[at:at + size])
    return all_frames


fragment = check(root / 'compressed-fragment.avi')
merged = check(root / 'compressed-merged.avi')
assert len(fragment) == 30 and len(merged) == 65 and fragment == merged[:30]
decoded = subprocess.check_output([args.ffmpeg, '-v', 'error', '-xerror', '-i', str(root / 'compressed-merged.avi'), '-f', 'framemd5', '-'])
assert len([line for line in decoded.splitlines() if line and not line.startswith(b'#')]) == 65
rgb = subprocess.check_output([args.ffmpeg, '-v', 'error', '-xerror', '-i', str(root / 'compressed-merged.avi'),
                               '-frames:v', '1', '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-'])
assert len(rgb) == 320 * 240 * 3
pixel = lambda x, y: rgb[(y * 320 + x) * 3:(y * 320 + x) * 3 + 3]
r, g, b = pixel(200, 40)
assert r > 240 and g < 15 and b < 15, (r, g, b)
r, g, b = pixel(200, 180)
assert r < 15 and g > 240 and b > 240, (r, g, b)
white = sum(min(pixel(x, y)) > 220 for y in range(225, 238) for x in range(10, 115))
assert white > 50, white
print('PASS: MJPEG bytes unchanged by merge; all AVI indexes; FFmpeg decoded 65 frames, correct colors/orientation/timestamp')
