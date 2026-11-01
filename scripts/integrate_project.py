"""Update generated Keil file groups from the sources in this repository."""
from pathlib import Path
import re, xml.etree.ElementTree as E
ROOT=Path(__file__).resolve().parents[1]
P=ROOT/'Projects/MDK-ARM/edge_gateway_h750.uvprojx'
t=E.parse(P);groups=t.find('.//Target/Groups')
def group(name,paths):
    g=E.SubElement(groups,'Group');E.SubElement(g,'GroupName').text=name;files=E.SubElement(g,'Files')
    for p in paths:
        f=E.SubElement(files,'File');E.SubElement(f,'FileName').text=p.name
        E.SubElement(f,'FileType').text='1'
        E.SubElement(f,'FilePath').text='..\\..\\'+str(p.relative_to(ROOT)).replace('/','\\')
for g in list(groups):
    if g.findtext('GroupName') in ['Gateway','Gateway/Port','FreeRTOS','LVGL']:groups.remove(g)
group('Gateway', sorted((ROOT/'App').glob('*.c')))
group('Gateway/Port',sorted((ROOT/'Port').glob('*.c'))+[ROOT/'Drivers/BSP/SPI/spi.c',ROOT/'Drivers/BSP/PCF8574/pcf8574.c'])
group('FreeRTOS',sorted((ROOT/'Middlewares/FreeRTOS').glob('*.c'))+[ROOT/'Middlewares/FreeRTOS/portable/MemMang/heap_4.c',ROOT/'Middlewares/FreeRTOS/portable/RVDS/ARM_CM4F/port.c'])
group('LVGL',sorted((ROOT/'Middlewares/LVGL/src').rglob('*.c')))
g=next(g for g in groups if g.findtext('GroupName')=='Drivers/STM32H7xx_HAL_Driver')
for name in ['stm32h7xx_hal_spi.c','stm32h7xx_hal_spi_ex.c','stm32h7xx_hal_iwdg.c']:
    if any(f.findtext('FileName')==name for f in g.findall('./Files/File')):continue
    files=g.find('Files');f=E.SubElement(files,'File');E.SubElement(f,'FileName').text=name;E.SubElement(f,'FileType').text='1';E.SubElement(f,'FilePath').text='..\\..\\Drivers\\STM32H7xx_HAL_Driver\\Src\\'+name
control=t.find('.//Cads/VariousControls')
define=control.find('Define')
if 'LV_CONF_INCLUDE_SIMPLE' not in (define.text or ''):define.text=(define.text or '')+',LV_CONF_INCLUDE_SIMPLE'
t.write(P,encoding='utf-8',xml_declaration=True)
p=ROOT/'Middlewares/LVGL/lv_conf.h';s=p.read_text(encoding='utf-8');s=re.sub(r'(#define LV_TICK_CUSTOM_INCLUDE)\s+[^\n]+',r'\1 "stm32h7xx_hal.h"',s);p.write_text(s,encoding='utf-8')
print('Keil groups regenerated:',sum(1 for _ in t.findall('.//Group/Files/File')),'files')
