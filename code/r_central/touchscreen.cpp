#include "touchscreen.h"
#include "keyboard.h"
#include "shared_vars.h"
#include "../base/base.h"
#include "../renderer/render_engine.h"
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

static int s_fdTouch = -1;
static int s_minX = 0, s_maxX = 0;
static int s_minY = 0, s_maxY = 0;
static int s_lastX = 0, s_lastY = 0;
static bool s_pressed = false;
static bool s_hasDevice = false;

bool touchscreen_has_device()
{
    return s_hasDevice;
}

int touchscreen_init()
{
    char szFile[64];
    for (int i=0; i<10; i++)
    {
        sprintf(szFile, "/dev/input/event%d", i);
        int fd = open(szFile, O_RDONLY | O_NONBLOCK);
        if ( fd < 0 )
            continue;
        unsigned long evbit = 0;
        if ( ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), &evbit) < 0 )
        {
            close(fd);
            continue;
        }
        if ( ! (evbit & (1<<EV_ABS)) )
        {
            close(fd);
            continue;
        }
        unsigned long keybit = 0;
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), &keybit);
        if ( ! (keybit & (1<<BTN_TOUCH)) )
        {
            close(fd);
            continue;
        }
        struct input_absinfo absX, absY;
        if ( ioctl(fd, EVIOCGABS(ABS_X), &absX) < 0 ||
             ioctl(fd, EVIOCGABS(ABS_Y), &absY) < 0 )
        {
            close(fd);
            continue;
        }
        s_fdTouch = fd;
        s_minX = absX.minimum;
        s_maxX = absX.maximum;
        s_minY = absY.minimum;
        s_maxY = absY.maximum;
        s_hasDevice = true;
        log_line("[Touch] Using input device %s", szFile);
        return 1;
    }
    log_line("[Touch] No touchscreen device found");
    return 0;
}

void touchscreen_uninit()
{
    if ( s_fdTouch >= 0 )
        close(s_fdTouch);
    s_fdTouch = -1;
    s_hasDevice = false;
}

static void _process_touch(int x, int y)
{
    if ( ! g_pRenderEngine )
        return;
    if ( s_maxX <= s_minX || s_maxY <= s_minY )
        return;
    float fx = ((float)x - (float)s_minX) / ((float)s_maxX - (float)s_minX);
    float fy = ((float)y - (float)s_minY) / ((float)s_maxY - (float)s_minY);
    if ( fy > 0.8 )
    {
        if ( fx < 0.3 )
            keyboard_add_triggered_input_event(INPUT_EVENT_PRESS_MENU);
        else if ( fx > 0.7 )
            keyboard_add_triggered_input_event(INPUT_EVENT_PRESS_BACK);
    }
    else if ( fx > 0.8 )
    {
        if ( fy < 0.5 )
            keyboard_add_triggered_input_event(INPUT_EVENT_PRESS_PLUS);
        else
            keyboard_add_triggered_input_event(INPUT_EVENT_PRESS_MINUS);
    }
}

void touchscreen_loop()
{
    if ( s_fdTouch < 0 )
        return;
    struct input_event events[16];
    int r = read(s_fdTouch, events, sizeof(events));
    if ( r <= 0 )
        return;
    int count = r / sizeof(struct input_event);
    for (int i=0;i<count;i++)
    {
        if ( events[i].type == EV_ABS )
        {
            if ( events[i].code == ABS_X || events[i].code == ABS_MT_POSITION_X )
                s_lastX = events[i].value;
            if ( events[i].code == ABS_Y || events[i].code == ABS_MT_POSITION_Y )
                s_lastY = events[i].value;
        }
        if ( events[i].type == EV_KEY && events[i].code == BTN_TOUCH )
        {
            if ( events[i].value == 1 )
                s_pressed = true;
            else if ( events[i].value == 0 )
            {
                if ( s_pressed )
                    _process_touch(s_lastX, s_lastY);
                s_pressed = false;
            }
        }
    }
}

void touchscreen_render_buttons()
{
    if ( ! s_hasDevice )
        return;
    if ( NULL == g_pRenderEngine )
        return;
    g_pRenderEngine->setFill(0,0,0,0.25);
    g_pRenderEngine->setStroke(0,0,0,0);
    g_pRenderEngine->drawRect(0,0.8,0.3,0.2); // menu area
    g_pRenderEngine->drawRect(0.7,0.8,0.3,0.2); // back area
    g_pRenderEngine->drawRect(0.8,0,0.2,0.5); // plus area
    g_pRenderEngine->drawRect(0.8,0.5,0.2,0.5); // minus area
}

