/*****************************************************
 *
 *	LED matrix driver for Raspberry RP2040
 *	(c) Peter Schulten, Mülheim, Germany
 *	peter_(at)_pitschu.de
 *
 *  Unmodified reproduction and distribution of this entire
 *  source code in any form is permitted provided the above
 *  notice is preserved.
 *  I make this source code available free of charge and therefore
 *  offer neither support nor guarantee for its functionality.
 *  Furthermore, I assume no liability for the consequences of
 *  its use.
 *  The source code may only be used and modified for private,
 *  non-commercial purposes. Any further use requires my consent.
 *
 *	History
 *	25.01.2022	pitschu		Start of work
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include <math.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hub75.h"
#include "LEDmx.h"

uint32_t display_buffers[DISPLAY_FRAMEBUFFER_SIZE];
uint32_t* display_front_buf = &display_buffers[0];
uint32_t* display_back_buf = &display_buffers[0];

uint32_t* ledmxActiveImage = &display_buffers[0];

uint8_t  overlayBuffer[DISPLAY_FRAMEBUFFER_SIZE];

static QueueHandle_t flushBlock;

static alpha_t 		alphaChannel;


static void LEDmx_task(void* pvParameters)
{
    while (true)
    {
        LEDmx_getFlushSemaphore();
        hub75_update(ledmxActiveImage, overlayBuffer);
        LEDmx_putFlushSemaphore();
        vTaskDelay(3);

    }
}


void LEDmx_start()
{
    flushBlock = xSemaphoreCreateMutex();
    xSemaphoreGive(flushBlock);

    hub75_config(8);

    BaseType_t xReturned;
    TaskHandle_t xHandle = NULL;
    /* Create the task, storing the handle. */
    xReturned = xTaskCreate(
        LEDmx_task,       /* Function that implements the task. */
        "LEDmx task",   /* Text name for the task. */
        512,             /* Stack size in words, not bytes. */
        (void*)1,    /* Parameter passed into the task. */
        tskIDLE_PRIORITY,/* Priority at which the task is created. */
        &xHandle);
}



void LEDmx_getFlushSemaphore(void)
{
    xSemaphoreTake(flushBlock, 100);
}



void LEDmx_putFlushSemaphore(void)
{
    xSemaphoreGive(flushBlock);
}


void LEDmx_SetPixel(int x, int y, rgb_t color)
{
    ledmxActiveImage[y * DISPLAY_WIDTH + x] = color;
}


void LEDmx_SetPixelRGB(int x, int y, uint8_t R, uint8_t G, uint8_t B)
{
    LEDmx_SetPixel(x, y, RGB(R, G, B));
}



void LEDmx_SetMasterBrightness(int brt)
{
    hub75_set_masterbrightness(brt);
}




void LEDmx_DrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, rgb_t color, bool overlay)
{
    signed int x, y, addx, addy, dx, dy;
    signed long P;
    int i;

    dx = abs((signed int)(x2 - x1));
    dy = abs((signed int)(y2 - y1));
    x = x1;
    y = y1;

    addx = addy = 1;
    if (x1 > x2)
        addx = -1;
    if (y1 > y2)
        addy = -1;

    if (dx >= dy)
    {
        P = 2L * dy - dx;
        for (i = 0; i <= dx; ++i)
        {
            if (overlay)
                LEDmx_SetOverlayPixel(x, y, color);
            else
                LEDmx_DrawPixel(x, y, color);
            if (P < 0)
            {
                P += 2 * dy;
                x += addx;
            }
            else
            {
                P += 2 * dy - 2 * dx;
                x += addx;
                y += addy;
            }
        }
    }
    else
    {
        P = 2L * dx - dy;
        for (i = 0; i <= dy; ++i)
        {
            if (overlay)
                LEDmx_SetOverlayPixel(x, y, color);
            else
                LEDmx_DrawPixel(x, y, color);
            if (P < 0)
            {
                P += 2 * dx;
                y += addy;
            }
            else
            {
                P += 2 * dx - 2 * dy;
                x += addx;
                y += addy;
            }
        }
    }
}



void LEDmx_setAlphaDisabled(void)
{
    alphaChannel.active = 0;
}



void LEDmx_setAlpha(rgb_t color)
{
    alphaChannel.r = MAP_888_R_TO_PWM(color);
    alphaChannel.g = MAP_888_G_TO_PWM(color);
    alphaChannel.b = MAP_888_B_TO_PWM(color);

    alphaChannel.active = 1;
}



void LEDmx_setAlphaRGB(uint8_t r, uint8_t g, uint8_t b)
{
    alphaChannel.r = (r & 0xff) >> (8 - bitPlanes);
    alphaChannel.g = (g & 0xff) >> (8 - bitPlanes);
    alphaChannel.b = (b & 0xff) >> (8 - bitPlanes);

    alphaChannel.active = 1;
}



void LEDmx_DrawPixel(int16_t x, int16_t y, rgb_t color)
{
    uint8_t r, g, b;

    if (LEDmx_IsClipped(x, y))
        return;

    r = MAP_888_R_TO_PWM(color);
    g = MAP_888_G_TO_PWM(color);
    b = MAP_888_B_TO_PWM(color);

    if (alphaChannel.active)
    {
        if (alphaChannel.r == r && alphaChannel.g == g && alphaChannel.b == b)
            return;
    }

    LEDmx_SetPixelRGB(x, y, r, g, b);
    return;
}



void LEDmx_Rect(int16_t left, int16_t top, int16_t right, int16_t bottom, rgb_t color, bool overlay)
{
    register unsigned int x, y;

    for (y = top; y <= bottom; y++)
    {
        for (x = left; x <= right; x++)
        {
            if (overlay)
                LEDmx_SetOverlayPixel(x, y, color);
            else
                LEDmx_DrawPixel(x, y, color);
        }
    }
    return;
}



void LEDmx_ClearScreen(rgb_t color)
{
    for (int i = 0; i < DISPLAY_FRAMEBUFFER_SIZE; i++)
        ledmxActiveImage[i] = (uint32_t)color;
}




void LEDmx_BlankScreen(void)
{
    LEDmx_ClearScreen(BLACK);
    // needs no flush
}




void LEDmx_SetClip(int16_t l, int16_t r, int16_t t, int16_t b)
{
    //glcdSetWindow(l, t, r, b);
}



uint8_t LEDmx_IsClipped(int16_t x, int16_t y)
{
    if ((x < 0) || (x >= DISPLAY_WIDTH) || (y < 0) || (y >= DISPLAY_HEIGHT))
        return 1;

    return 0;
}


void LEDmx_ClearOverlay (void)
{
    memset (overlayBuffer, 0, sizeof(overlayBuffer));
}


void LEDmx_SetOverlayPixel(int x, int y, int color)
{
    if (!LEDmx_IsClipped(x,y))
        overlayBuffer [(y * DISPLAY_WIDTH) + x] = color;
}


void LEDmx_SetOverlayColor(int index, rgb_t color)
{
    hub75_set_overlaycolor(index, color);
}



inline uint32_t LEDmx_565toRGB(uint16_t pix) {
    uint32_t r_gamma = pix & 0xf800u;
    r_gamma *= r_gamma;
    uint32_t g_gamma = pix & 0x07e0u;
    g_gamma *= g_gamma;
    uint32_t b_gamma = pix & 0x001fu;
    b_gamma *= b_gamma;
    return (r_gamma >> 24 << 16) | (g_gamma >> 14 << 8) | (b_gamma >> 2 << 0);
}
