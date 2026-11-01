"""Compile and run the production protocol/storage logic against independent fixtures."""
from pathlib import Path
import os, shutil, subprocess, unittest
ROOT=Path(__file__).resolve().parents[1]
GCC=os.environ.get('GATEWAY_HOST_CC') or shutil.which('gcc') or r'D:\Program Files (x86)\Dev-Cpp\MinGW64\bin\gcc.exe'
class CoreTests(unittest.TestCase):
    def run_case(self, name, sources):
        target=ROOT/'artifacts'/(name+'.exe'); target.parent.mkdir(exist_ok=True)
        command=[GCC,'-std=c99','-Wall','-Wextra','-Werror','-I',str(ROOT/'App'),'-I',str(ROOT/'Port'),'-I',str(ROOT/'Middlewares/LVGL'),'-DLV_CONF_INCLUDE_SIMPLE',str(ROOT/'tests'/(name+'.c'))]
        command += [str(ROOT/'App'/s) for s in sources]
        command += ['-o',str(target)]
        result=subprocess.run(command,capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        result=subprocess.run([str(target)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        print(result.stdout.strip())
    def test_protocol_fixtures(self):
        self.run_case('protocol_test',['gateway_protocol.c'])
    def test_storage_recovery(self):
        self.run_case('storage_test',['gateway_store.c','gateway_protocol.c'])
    def test_at_stream(self):
        self.run_case('at_test',['gateway_at.c'])
    def test_sensor_formats(self):
        self.run_case('sensor_test',['gateway_protocol.c'])
    def test_network_without_flash(self):
        self.run_case('network_test',['gateway_protocol.c','gateway_at.c','gateway_store.c'])
    def test_ltdc_color_layout(self):
        self.run_case('color_test',[])
if __name__=='__main__': unittest.main(verbosity=2)
