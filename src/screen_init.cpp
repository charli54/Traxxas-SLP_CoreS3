#include "screen_init.h"
#include <M5Unified.h>
#include <M5GFX.h>
#include <lvgl.h>

static M5GFX gfx;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[320 * 10];

// ── Display flush ────────────────────────────────────────────────
static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    gfx.startWrite();
    gfx.setAddrWindow(area->x1, area->y1,
                      area->x2 - area->x1 + 1,
                      area->y2 - area->y1 + 1);
    gfx.pushPixels((uint16_t *)color_p,
                   (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1));
    gfx.endWrite();
    lv_disp_flush_ready(disp);
}

// ── Touch input ──────────────────────────────────────────────────
static void my_touch_read(lv_indev_drv_t *indev, lv_indev_data_t *data) {
    auto touch = M5.Touch.getDetail();
    if (touch.state == m5::touch_state_t::touch ||
        touch.state == m5::touch_state_t::touch_begin) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = touch.x;
        data->point.y = touch.y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// ── Point d'entrée public ────────────────────────────────────────
void screen_init(void) {
    // Hardware
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Power.begin();
    M5.Power.setChargeCurrent(200);
    delay(100);
    gfx.begin();
    gfx.setRotation(1);

    // LVGL core
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, nullptr, 320 * 10);

    // Display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = 320;
    disp_drv.ver_res  = 240;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Touch driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touch_read;
    lv_indev_drv_register(&indev_drv);

    // Background screen
    static lv_style_t style_screen;
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, lv_color_hex(0x000000));
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);
    lv_obj_add_style(lv_scr_act(), &style_screen, LV_PART_MAIN);
    
    // Si la carte est alimentée par batterie, cette commande force la sortie 5V/3.3V du Port A
    M5.Power.setExtOutput(true);
}