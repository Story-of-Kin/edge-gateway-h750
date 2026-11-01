#include "gateway.h"
#include "gateway_board.h"
#include "lvgl.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/TOUCH/touch.h"
#include <string.h>
extern uint16_t *g_ltdc_framebuf[2];
static lv_color_t pixels[800*24];
static void flush(lv_disp_drv_t*d,const lv_area_t*a,lv_color_t*p)
{
    int y;size_t width=(size_t)(a->x2-a->x1+1);
    if(a->x1>=0&&a->y1>=0&&a->x2<800&&a->y2<480){
        for(y=a->y1;y<=a->y2;y++){memcpy(g_ltdc_framebuf[0]+y*800+a->x1,p,width*2);p+=width;}
        g_gateway_diag.ui_frames++;
    }
    lv_disp_flush_ready(d);
}
static void touch(lv_indev_drv_t*d,lv_indev_data_t*p)
{
    (void)d;tp_dev.scan(0);p->state=LV_INDEV_STATE_RELEASED;
    if((tp_dev.sta&TP_PRES_DOWN)&&tp_dev.x[0]<800&&tp_dev.y[0]<480){
        p->state=LV_INDEV_STATE_PRESSED;p->point.x=tp_dev.x[0];p->point.y=tp_dev.y[0];
        g_gateway_diag.touch_events++;g_gateway_diag.touch_x=tp_dev.x[0];g_gateway_diag.touch_y=tp_dev.y[0];gateway_backlight(1);
    }
}
void lv_port_h750_init(void)
{
    static lv_disp_draw_buf_t buffer;static lv_disp_drv_t display;static lv_indev_drv_t input;
    lv_disp_draw_buf_init(&buffer,pixels,0,800*24);lv_disp_drv_init(&display);
    display.hor_res=800;display.ver_res=480;display.draw_buf=&buffer;display.flush_cb=flush;lv_disp_drv_register(&display);
    lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=touch;lv_indev_drv_register(&input);
}
