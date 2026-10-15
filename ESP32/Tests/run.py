from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[1]
build = root / 'build/host'
build.mkdir(parents=True, exist_ok=True)
link = root / 'TimelapseWifi'
subprocess.run(['g++', '-std=c++11', '-DCONFIG_IDF_TARGET_ESP32S3=1', '-DARDUINO_USB_MODE=1', '-DARDUINO_USB_CDC_ON_BOOT=1', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-g', '-I' + str(root / 'Tests'),
                '-I' + str(link), str(root / 'Tests/test_link.cpp'), str(link / 'CameraLink.cpp'),
                '-o', str(build / 'test_link')], check=True)
subprocess.run([str(build / 'test_link')], check=True)
