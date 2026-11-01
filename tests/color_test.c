#include <stdio.h>
#include "../Middlewares/LVGL/src/misc/lv_color.h"
int main(void){lv_color_t red=lv_color_make(255,0,0),green=lv_color_make(0,255,0),blue=lv_color_make(0,0,255);
if(red.full!=0xf800||green.full!=0x07e0||blue.full!=0x001f){printf("RGB565 LTDC order wrong: %04x %04x %04x\n",red.full,green.full,blue.full);return 1;}puts("LTDC RGB565 colors: passed");return 0;}
