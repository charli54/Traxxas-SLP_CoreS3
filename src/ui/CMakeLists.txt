SET(SOURCES screens/ui_scr.c
    screens/ui_scr2.c
    screens/ui_scr3.c
    screens/ui_scr4.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_2146270223.c
    images/ui_img_bt_record_off_png.c
    images/ui_img_bt_record_on_png.c
    images/ui_img_bt_save_off_png.c
    images/ui_img_bt_save_on_png.c
    images/ui_img_bt_save_disable_png.c
    images/ui_img_bt_usb_off_png.c
    images/ui_img_bt_usb_on_png.c
    images/ui_img_bt_usb_disable_png.c
    images/ui_img_home_on_v2_png.c
    images/ui_img_home_off_v2_png.c
    images/ui_img_sauvegarder_off_scr01_png.c
    images/ui_img_sauvegarder_on_scr01_png.c
    images/ui_img_reprendre_off_scr201_png.c
    images/ui_img_reprendre_on_scr201_png.c
    images/ui_img_fond_scr4_png.c
    images/ui_img_btscr4_usb_off_png.c
    images/ui_img_btscr4_usb_on_png.c
    images/ui_img_btscr4_usb_disable_png.c
    fonts/ui_font_spaceRave16.c)

add_library(ui ${SOURCES})
