#pragma once

bool touchscreen_has_device();
int touchscreen_init();
void touchscreen_uninit();
void touchscreen_loop();
void touchscreen_render_buttons();
