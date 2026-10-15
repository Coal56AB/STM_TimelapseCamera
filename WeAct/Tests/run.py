#!/usr/bin/env python3
"""Тесты C-модулей и реальной FatFs на модели SD; Linux/WSL с GCC."""
from pathlib import Path
import os
import struct
import subprocess
root = Path(__file__).resolve().parents[1]
build = root / 'build/host'
build.mkdir(parents=True, exist_ok=True)
sources = ['Tests/test_main.c', 'Tests/host_disk.c', 'Core/Common/button.c', 'Core/Media/avi.c',
           'Core/Recording/recording.c', 'Core/Media/image_text.c', 'Core/Common/font.c', 'Core/Timelapse/timelapse_config.c', 'Core/UI/ui.c',
           'Core/Bsp/Storage/store_fatfs.c', 'Core/Storage/FatFs/ff.c', 'Core/Storage/FatFs/ffunicode.c']
includes = ['Tests', 'Core/Common', 'Core/Media', 'Core/Recording', 'Core/Storage',
            'Core/Timelapse', 'Core/UI', 'Core/Camera', 'Core/Storage/FatFs']
command = [os.environ.get('CC', 'cc'), '-std=c11', '-DFF_USE_MKFS=1', '-g', '-O1', '-Wall', '-Wextra', '-Werror',
           '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
subprocess.run(command + ['-I' + str(root / p) for p in includes] +
               [str(root / p) for p in sources] + ['-o', str(build / 'test')], check=True)
subprocess.run([str(build / 'test')], cwd=str(build), check=True)

display_includes = ['Core/Common', 'Tests/Display', 'Core/UI', 'Core/Camera', 'Core/Bsp/ST7735']
display_sources = ['Tests/Display/test_display.c', 'Core/Bsp/ST7735/display_port.c', 'Core/Common/font.c']
subprocess.run(command + ['-I' + str(root / p) for p in display_includes] +
               [str(root / p) for p in display_sources] + ['-o', str(build / 'test_display')], check=True)
subprocess.run([str(build / 'test_display')], check=True)

panel_sources = ['Tests/Display/test_st7735.c', 'Core/Bsp/ST7735/st7735.c', 'Core/Bsp/ST7735/st7735_reg.c']
subprocess.run(command + ['-Wno-unused-parameter', '-I' + str(root / 'Core/Bsp/ST7735')] +
               [str(root / p) for p in panel_sources] + ['-o', str(build / 'test_st7735')], check=True)
subprocess.run([str(build / 'test_st7735')], check=True)

camera_includes = ['Tests/Camera', 'Core/Camera', 'Core/Bsp/Camera']
camera_sources = ['Tests/Camera/test_camera.c', 'Core/Bsp/Camera/camera_port.c',
                  'Core/Bsp/Camera/ov7725.c', 'Core/Bsp/Camera/ov7725_regs.c', 'Core/Bsp/Camera/ov5640.c']
subprocess.run(command + ['-I' + str(root / p) for p in camera_includes] +
               [str(root / p) for p in camera_sources] + ['-o', str(build / 'test_camera')], check=True)
subprocess.run([str(build / 'test_camera')], check=True)

time_sources = ['Tests/test_time.c', 'Core/Common/clock.c', 'Core/Common/font.c',
                'Core/Media/image_text.c', 'Core/Timelapse/time_sync.c']
subprocess.run(command + ['-I' + str(root / p) for p in ['Core/Common', 'Core/Media', 'Core/Timelapse']] +
               [str(root / p) for p in time_sources] + ['-o', str(build / 'test_time')], check=True)
subprocess.run([str(build / 'test_time')], check=True)
control_sources = ['Tests/test_esp_control.c', 'Core/Timelapse/esp_control.c',
                   'Core/Timelapse/time_sync.c', 'Core/Common/clock.c']
subprocess.run(command + ['-I' + str(root / p) for p in ['Core/Common', 'Core/Timelapse', 'Core/Bsp/USB']] +
               [str(root / p) for p in control_sources] + ['-o', str(build / 'test_esp_control')], check=True)
subprocess.run([str(build / 'test_esp_control')], check=True)

data = (build / 'merged.avi').read_bytes()
u32 = lambda at: struct.unpack_from('<I', data, at)[0]
assert data[:4] == b'RIFF' and u32(4) + 8 == len(data) and data[8:12] == b'AVI '
def chunks(start, end):
    at = start
    while at < end:
        length = u32(at + 4)
        assert at + 8 + length <= end
        yield data[at:at + 4], at + 8, length
        at += 8 + length + (length & 1)
    assert at == end
top = list(chunks(12, len(data)))
assert [t[0] for t in top] == [b'LIST', b'LIST', b'idx1']
hdrl, movi, index = top
assert data[hdrl[1]:hdrl[1] + 4] == b'hdrl'
headers = list(chunks(hdrl[1] + 4, hdrl[1] + hdrl[2]))
assert headers[0][0] == b'avih' and u32(headers[0][1] + 16) == 3
stream_headers = list(chunks(headers[1][1] + 4, headers[1][1] + headers[1][2]))
assert stream_headers[0][0] == b'strh' and stream_headers[1][0] == b'strf'
frames = list(chunks(movi[1] + 4, movi[1] + movi[2]))
assert len(frames) == 3
for i, (kind, at, length) in enumerate(frames):
    assert kind == b'00db' and length == 320 * 240 * 3
    assert data[at:at + 3] == bytes([255, 255, 0])
    assert data[at + 239 * 960:at + 239 * 960 + 3] == bytes([0, 0, 255])
    tag, flags, offset, size = struct.unpack_from('<4sIII', data, index[1] + i * 16)
    assert tag == kind and flags == 16 and movi[1] + offset == at - 8 and size == length
print('PASS: independent RIFF parser, BGR24 colors, bottom-up rows, index offsets')
