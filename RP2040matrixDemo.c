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
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "ps_debug.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "LEDmx.h"


char				logTimeBuf[32];


void blinkTask(void* para)
{
    PRT_DEBUG("Starting BLINK task\n");
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    for (;; )
    {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        vTaskDelay(25);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        vTaskDelay(25);

        PRT_DEBUG("Blinker\n");
    }
}

void lifeTask(void* para)
{
    extern void life(uint16_t cmd);
    int count = 0;
    int bpp = 7;
    PRT_DEBUG("Starting LIFE task\n");
    vTaskDelay(100);
    life(0);
    while (1)
    {
        vTaskDelay(7);
 /*
        LEDmx_SetMasterBrightness(bpp++);
        if (bpp > 63)
            bpp = 0;
*/
        life(1);
        /*
                if (++count > 30)
                {
                    count = 0;
                    if (--bpp < 4)
                        bpp = 8;
                    hub75_config(bpp);
                }
        */
    }
}


void pongTask(void* para)
{
    extern void initPongGame(void);
    extern int playPongGame(int countDown);

    PRT_DEBUG("Starting PONG task\n");
    vTaskDelay(100);
    initPongGame();
    while (1)
    {
        vTaskDelay(3);
        playPongGame(1000);
    }
}



void app_main(void* para)
{
    vTaskDelay(500);    // delay 4  secs to give user a chance to start putty

    LOG_DEBUG("main started. LED_PIN=%d\n", PICO_DEFAULT_LED_PIN);
    // Initialize HUB75

    LEDmx_start();

    memset(display_buffers, 0, sizeof(display_buffers));

    LEDmx_SetMasterBrightness(20);
    LEDmx_ClearScreen(0x020202);
    vTaskDelay(100);
#if 0
#include "mountains_128x64_rgb565.h"
    const uint16_t* img = (const uint16_t*)mountains_128x64;
    for (int y = 0; y < DISPLAY_HEIGHT; y++) 
    {
        for (int x = 0; x < 128; x++)
        {
            LEDmx_SetPixel(x, y, LEDmx_565toRGB(img[y * 128 + x]));
        }
    }
    vTaskDelay(500);
#endif
    LEDmx_ClearScreen(BLACK);

    // Fill both framebuffers with a default pattern
    for (int x = 16; x < DISPLAY_WIDTH - 16; ++x) {
        for (int y = 16; y < DISPLAY_HEIGHT - 16; ++y) {
            uint32_t c = (x * 3) << 16 | (y * 3) << 8 | ((x * y) >> 4) << 0;

            LEDmx_SetPixel(x, y, c);
            LEDmx_SetPixel(x, y, c);
        }
    }

    for (int x = 4; x < DISPLAY_WIDTH - 4; ++x)
    {
        LEDmx_SetPixel(x, 4, 0x333300);        // yellow
        LEDmx_SetPixel(x, 60, 0x330033);       // pink
    }
    for (int y = 0; y < DISPLAY_HEIGHT; ++y)
    {
        LEDmx_SetPixel(1, y, 0x003300);        // green
        LEDmx_SetPixel(62, y, 0x000033);       // blue
    }

    TaskHandle_t xHandle = NULL;
    /* Create the task, storing the handle. */
    xTaskCreate(
        lifeTask,       /* Function that implements the task. */
        "LIFE task",   /* Text name for the task. */
        1000,             /* Stack size in words, not bytes. */
        (void*)1,    /* Parameter passed into the task. */
        tskIDLE_PRIORITY,/* Priority at which the task is created. */
        &xHandle);

    xHandle = NULL;
    xTaskCreate(
        pongTask,       // Function that implements the task.
        "Pong task",   // Text name for the task.
        1000,             // Stack size in words, not bytes.
        (void*)1,    // Parameter passed into the task.
        tskIDLE_PRIORITY,// Priority at which the task is created.
        &xHandle);

    xHandle = NULL;
    xTaskCreate(
        blinkTask,       // Function that implements the task.
        "Blink task",   // Text name for the task.
        500,             // Stack size in words, not bytes.
        (void*)1,    // Parameter passed into the task.
        tskIDLE_PRIORITY,// Priority at which the task is created.
        &xHandle);

    vTaskDelete(NULL);
}



int main() 
{
    stdio_init_all();
    gpio_init(15);
    gpio_set_dir(15, GPIO_OUT);

    /* Create the task, storing the handle. */
    xTaskCreate(
        app_main,       /* Function that implements the task. */
        "MAIN task",   /* Text name for the task. */
        2000,             /* Stack size in words, not bytes. */
        (void*)1,    /* Parameter passed into the task. */
        tskIDLE_PRIORITY,/* Priority at which the task is created. */
        NULL);

    vTaskStartScheduler();
 
    while (1) {};
    return 0;
}

