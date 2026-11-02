/*
 * LVGL与H750显示/触摸之间的桥接。
 * LVGL先在小绘图缓冲中渲染，flush把像素复制到SDRAM，LTDC再读取SDRAM输出到屏幕。
 * RGB565每像素2字节，使用原生16位顺序（LV_COLOR_16_SWAP=0），不要套用SPI小屏的字节交换。
 */
#include "gateway.h"
#include "gateway_board.h"
#include "lvgl.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/TOUCH/touch.h"
#include <string.h>
extern uint16_t *g_ltdc_framebuf[2];
/* 只分配800列×24行的局部绘图缓冲；完整800×480帧缓冲已由板厂LTDC驱动放在SDRAM。 */
static lv_color_t pixels[800 * 24];
/*
 * 显示刷新回调：a为待刷矩形，p为紧凑排列的像素数组；边界坐标包含x2/y2。
 * 数据复制完成后必须调用lv_disp_flush_ready，告诉LVGL可以继续复用绘图缓冲。
 */
static void flush(lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *p)
{
    int y;
    size_t width = (size_t)(a->x2 - a->x1 + 1);
    if (a->x1 >= 0 && a->y1 >= 0 && a->x2 < 800 && a->y2 < 480)
    {
        /* 目标每行跨度为800像素；源每行只有矩形宽度，因此要逐行复制并推进源指针。 */
        for (y = a->y1; y <= a->y2; y++)
        {
            memcpy(g_ltdc_framebuf[0] + y * 800 + a->x1, p, width * 2);
            p += width;
        }
        g_gateway_diag.ui_frames++;
    }
    lv_disp_flush_ready(d);
}
/* 输入回调：扫描真实触摸，向LVGL报告按下/释放与坐标；没有触摸时默认释放。 */
static void touch(lv_indev_drv_t *d, lv_indev_data_t *p)
{
    (void)d;
    tp_dev.scan(0);
    p->state = LV_INDEV_STATE_RELEASED;
    /* 仅接受屏幕范围内的真实触摸坐标；记录诊断并立即打开背光。 */
    if ((tp_dev.sta & TP_PRES_DOWN) && tp_dev.x[0] < 800 && tp_dev.y[0] < 480)
    {
        p->state = LV_INDEV_STATE_PRESSED;
        p->point.x = tp_dev.x[0];
        p->point.y = tp_dev.y[0];
        g_gateway_diag.touch_events++;
        g_gateway_diag.touch_x = tp_dev.x[0];
        g_gateway_diag.touch_y = tp_dev.y[0];
        gateway_backlight(1);
    }
}
/* 注册显示驱动与指针输入驱动，在lv_init之后调用。static驱动对象需长期有效。 */
void lv_port_h750_init(void)
{
    static lv_disp_draw_buf_t buffer;
    static lv_disp_drv_t display;
    static lv_indev_drv_t input;
    /* 缓冲容量单位是像素，不是字节；使用单缓冲，第二个缓冲指针为0。 */
    lv_disp_draw_buf_init(&buffer, pixels, 0, 800 * 24);
    lv_disp_drv_init(&display);
    display.hor_res = 800;
    display.ver_res = 480;
    display.draw_buf = &buffer;
    display.flush_cb = flush;
    lv_disp_drv_register(&display);
    lv_indev_drv_init(&input);
    input.type = LV_INDEV_TYPE_POINTER;
    input.read_cb = touch;
    lv_indev_drv_register(&input);
}
