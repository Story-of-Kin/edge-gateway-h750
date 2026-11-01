"""Read live diagnostic RAM through CMSIS-DAP without halting or programming.

Optional --framebuffer captures the RGB565 framebuffer, not a photo of the panel.
Requires pyocd; Pillow is needed only for framebuffer PNG output.
"""
import argparse, json, re, struct, subprocess
from pathlib import Path
from pyocd.core.helpers import ConnectHelper
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(); p.add_argument('--framebuffer',action='store_true'); a=p.parse_args()
symbols=ROOT/'artifacts/symbols.txt'
subprocess.run([r'D:\Keil_v5\ARM\ARMCC\bin\fromelf.exe','--text','-s',str(ROOT/'artifacts/obj/edge_gateway_h750.axf'),'--output',str(symbols)],check=True)
match=re.search(r'\bg_gateway_diag\s+(0x[0-9a-fA-F]+)',symbols.read_text(errors='replace'))
if not match: raise SystemExit('Diagnostic symbol not found')
# H743 is used solely for the common H7/M7 RAM debug map; never program with it.
session=ConnectHelper.session_with_chosen_probe(target_override='stm32h743xx',connect_mode='attach',frequency=4000000)
if session is None: raise SystemExit('No CMSIS-DAP probe found')
with session:
    names='magic stage uptime_ms lcd_id lcd_width lcd_height flash_id flash_selftest storage_ready ui_frames touch_events sensor_polls temperature_valid light_valid network_state rx_overflows heartbeat_ui heartbeat_sensor heartbeat_network free_heap fatal_code touch_x touch_y cache_pending'.split()
    raw=bytes(session.target.read_memory_block8(int(match[1],16),96))
    data=dict(zip(names,struct.unpack('<24I',raw)))
    if data['magic']!=0x48373530: raise SystemExit('Firmware/ELF mismatch')
    report=json.dumps(data,indent=2); print(report); (ROOT/'artifacts/diagnostics.json').write_text(report)
    if a.framebuffer:
        from PIL import Image
        image=Image.new('RGB',(800,480)); pixels=[]
        for offset in range(0,800*480*2,8192):
            block=bytes(session.target.read_memory_block8(0xc0000000+offset,min(8192,800*480*2-offset)))
            for (v,) in struct.iter_unpack('<H',block):pixels.append((((v>>11)&31)*255//31,((v>>5)&63)*255//63,(v&31)*255//31))
        image.putdata(pixels); image.save(ROOT/'artifacts/framebuffer.png')
