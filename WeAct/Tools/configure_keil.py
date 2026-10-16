#!/usr/bin/env python3
"""Подключение модулей приложения к существующему проекту, созданному CubeMX."""
from pathlib import Path
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
project = root / 'MDK-ARM/Timelapse.uvprojx'
tree = ET.parse(project)
target = tree.find('./Targets/Target')

def set_text(parent, path, value):
    element = parent.find(path)
    if element is None:
        assert '/' not in path, path
        element = ET.SubElement(parent, path)
    element.text = value

set_text(target, 'pCCUsed', '6190000::V6.19::ARMCLANG')
set_text(target, 'uAC6', '1')
options = target.find('TargetOption')
common = options.find('TargetCommonOption')
for key, value in {
    'PackID': 'Keil.STM32H7xx_DFP.4.1.3', 'PackURL': 'https://www.keil.com/pack/',
    'OutputDirectory': '..\\build\\keil\\', 'OutputName': 'Timelapse',
    'ListingPath': '..\\build\\keil\\', 'CreateHexFile': '1',
    'SFDFile': '$$Device:STM32H723VGTx$CMSIS/SVD/STM32H723.svd',
}.items():
    set_text(common, key, value)
arm = options.find('TargetArmAds')
for key, value in {
    'ArmAdsMisc/useUlib': '1', 'Cads/Optim': '6', 'Cads/uGnu': '0', 'Cads/wLevel': '3',
    'Cads/VariousControls/MiscControls': '-std=c11 -Wall -Wextra -Wno-unused-parameter -O2',
    'Cads/VariousControls/Define': 'USE_HAL_DRIVER,STM32H723xx,USER_VECT_TAB_ADDRESS,DATA_IN_D2_SRAM',
    'LDads/umfTarg': '0', 'LDads/ScatterFile': 'Timelapse.sct',
}.items():
    set_text(arm, key, value)
modules = sorted(p for p in (root / 'Core').iterdir() if p.is_dir() and p.name not in ['Inc', 'Src'])
include = arm.find('Cads/VariousControls/IncludePath')
paths = include.text.split(';')
groups = target.find('Groups')
for module in modules:
    paths.append('../' + module.relative_to(root).as_posix())
    paths.extend('../' + p.relative_to(root).as_posix() for p in module.rglob('*') if p.is_dir())
    name = module.relative_to(root).as_posix()
    group = next((g for g in groups if g.findtext('GroupName') == name), None)
    if group is None:
        group = ET.SubElement(groups, 'Group')
        ET.SubElement(group, 'GroupName').text = name
        ET.SubElement(group, 'Files')
    files = group.find('Files')
    existing = {f.findtext('FilePath') for f in files}
    for source in sorted(module.rglob('*')):
        if source.suffix not in ['.c', '.h']:
            continue
        path = '../' + source.relative_to(root).as_posix()
        if path in existing:
            continue
        file = ET.SubElement(files, 'File')
        ET.SubElement(file, 'FileName').text = source.name
        ET.SubElement(file, 'FileType').text = '1' if source.suffix == '.c' else '5'
        ET.SubElement(file, 'FilePath').text = path
include.text = ';'.join(dict.fromkeys(paths))
# CMSIS headers are already copied by CubeMX; the old RTE component is redundant.
components = tree.find('./RTE/components')
if components is not None:
    for component in list(components):
        if component.get('Cclass') == 'CMSIS' and component.get('Cgroup') == 'CORE':
            components.remove(component)
tree.write(project, encoding='utf-8', xml_declaration=True)

# CDC ACM devices can advertise protocol 0 and a non-zero data subclass.
# Apply after CubeMX recopies the stock middleware; keep its class/ACM checks.
cdc = root / 'Middlewares/ST/STM32_USB_Host_Library/Class/CDC/Src/usbh_cdc.c'
source = cdc.read_text(encoding='utf-8')
for old, new in [
    ('ABSTRACT_CONTROL_MODEL, COMMON_AT_COMMAND);', 'ABSTRACT_CONTROL_MODEL, 0xFFU); /* Match CDC ACM with or without AT protocol. */'),
    ('RESERVED, NO_CLASS_SPECIFIC_PROTOCOL_CODE);', '0xFFU, NO_CLASS_SPECIFIC_PROTOCOL_CODE); /* Data subclass is device-specific. */'),
]:
    assert old in source or new in source, 'Unexpected CDC middleware version'
    source = source.replace(old, new)
cdc.write_text(source, encoding='utf-8')
# ITCM holds chroma data; enable it before C startup clears the scatter regions.
system = root / 'Core/Src/system_stm32h7xx.c'
source = system.read_text(encoding='utf-8')
marker = '  /* Camera: enable ITCM before scatter initialization. */'
if marker not in source:
    anchor = 'void SystemInit (void)\n{'
    assert anchor in source, 'Unexpected SystemInit layout'
    source = source.replace(anchor, anchor + '\n' + marker + '\n  SCB->VTOR = FLASH_BASE;\n  SCB->ITCMCR |= SCB_ITCMCR_EN_Msk;\n  __DSB();\n  __ISB();')
    system.write_text(source, encoding='utf-8')
print(project)
