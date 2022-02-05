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
#include "stdlib.h"
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hub75.h"


uint32_t *frameBuffer = NULL; // each entry contains RGB data for 2 or 4 consective pixels on one HUB75 channel
uint32_t *ctrlBuffer  = NULL; // N bit planes * # of scan lines

uint16_t    masterBrightness = 0;
uint16_t    bitPlanes = DISPLAY_MAXPLANES;

static PIO display_pio = pio0;
static uint display_sm_data;
static uint display_offset_data;
static uint display_sm_ctrl;
static uint display_offset_ctrl;

static int display_dma_chan;
static int ctrl_dma_chan;

static rgb_t overlayColors[16];
static uint8_t *overlayBuffer = NULL;

static void dma_hub75_handler()
{
    // Clear the interrupt request.
    if (dma_hw->ints0 & (1u << display_dma_chan))
    {
        dma_hw->ints0 = 1u << display_dma_chan;
        // start next display cycle
        dma_channel_set_read_addr(display_dma_chan, &frameBuffer[0], true);
    }
    if (dma_hw->ints0 & (1u << ctrl_dma_chan))
    {
        dma_hw->ints0 = 1u << ctrl_dma_chan;
        // start next display cycle
        dma_channel_set_read_addr(ctrl_dma_chan, &ctrlBuffer[0], true);
    }
}


static void hub75_init() 
{
    // Initialize PIO
    display_sm_data = pio_claim_unused_sm(display_pio, true);
    display_sm_ctrl = pio_claim_unused_sm(display_pio, true);

#ifdef PCB_LAYOUT_V1
    display_offset_data = pio_add_program(display_pio, &ps_64_data_program);
    ps_64_data_program_init(
        display_pio,
        display_sm_data,
        display_offset_data,
        PIO_DATA_OUT_BASE, PIO_DATA_OUT_CNT,
        PIO_DATA_SET_BASE, PIO_DATA_SET_CNT,
        PIO_DATA_SIDE_BASE, PIO_DATA_SIDE_CNT
    );

    display_offset_ctrl = pio_add_program(display_pio, &ps_64_ctrl_program);
    ps_64_ctrl_program_init(
        display_pio,
        display_sm_ctrl,
        display_offset_ctrl,
        PIO_CTRL_OUT_BASE, PIO_CTRL_OUT_CNT,
        PIO_CTRL_SET_BASE, PIO_CTRL_SET_CNT,
        PIO_CTRL_SIDE_BASE, PIO_CTRL_SIDE_CNT
    );
#endif
#ifdef PCB_LAYOUT_V2
#if HUB75_SIZE == 4040
    display_offset_data = pio_add_program(display_pio, &ps_64_data_program);
    ps_64_data_program_init(
        display_pio,
        display_sm_data,
        display_offset_data,
        PIO_DATA_OUT_BASE, PIO_DATA_OUT_CNT,
        PIO_DATA_SET_BASE, PIO_DATA_SET_CNT,
        PIO_DATA_SIDE_BASE, PIO_DATA_SIDE_CNT
    );

    display_offset_ctrl = pio_add_program(display_pio, &ps_64_ctrl_program);
    ps_64_ctrl_program_init(
        display_pio,
        display_sm_ctrl,
        display_offset_ctrl,
        PIO_CTRL_OUT_BASE, PIO_CTRL_OUT_CNT,
        PIO_CTRL_SET_BASE, PIO_CTRL_SET_CNT,
        PIO_CTRL_SIDE_BASE, PIO_CTRL_SIDE_CNT
    );
#else
    display_offset_data = pio_add_program(display_pio, &ps_128_data_program);
    ps_128_data_program_init(
        display_pio,
        display_sm_data,
        display_offset_data,
        PIO_DATA_OUT_BASE, PIO_DATA_OUT_CNT,
        PIO_DATA_SET_BASE, PIO_DATA_SET_CNT,
        PIO_DATA_SIDE_BASE, PIO_DATA_SIDE_CNT
    );

    display_offset_ctrl = pio_add_program(display_pio, &ps_128_ctrl_program);
    ps_128_ctrl_program_init(
        display_pio,
        display_sm_ctrl,
        display_offset_ctrl,
        PIO_CTRL_OUT_BASE, PIO_CTRL_OUT_CNT,
        PIO_CTRL_SET_BASE, PIO_CTRL_SET_CNT,
        PIO_CTRL_SIDE_BASE, PIO_CTRL_SIDE_CNT
    );
#endif
#endif


    // Initialize data port DMA
    display_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(display_dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, DREQ_PIO0_TX0 + display_sm_data);

    //    channel_config_set_ring(&c, false, 15);     // ring size is 8192

    dma_channel_configure(
        display_dma_chan,
        &c,
        &pio0_hw->txf[display_sm_data],
        NULL,  // Will be set later for each transfer
#if HUB75_SIZE == 4040
        bitPlanes * ((DISPLAY_WIDTH / 4) * DISPLAY_SCAN),     // complete frame buffer for X bit planes
#elif HUB75_SIZE == 8080
        bitPlanes * ((DISPLAY_WIDTH / 2) * DISPLAY_SCAN),     // complete frame buffer for X bit planes
#endif
        false
    );
    dma_channel_set_irq0_enabled(display_dma_chan, true);

    // Initialize control port DMA
    ctrl_dma_chan = dma_claim_unused_channel(true);
    c = dma_channel_get_default_config(ctrl_dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, DREQ_PIO0_TX0 + display_sm_ctrl);

    dma_channel_configure(
        ctrl_dma_chan,
        &c,
        &pio0_hw->txf[display_sm_ctrl],
        NULL,  // Will be set later for each transfer
        (bitPlanes * DISPLAY_SCAN),     // complete frame buffer for X bit planes
        false
    );
    dma_channel_set_irq0_enabled(ctrl_dma_chan, true);

    irq_set_exclusive_handler(DMA_IRQ_0, dma_hub75_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}



static void hub75_start()
{
    dma_channel_set_read_addr(display_dma_chan, frameBuffer, true);
    dma_channel_set_read_addr(ctrl_dma_chan, ctrlBuffer, true);
}



void hub75_config(int bpp)
{
    if (bpp < 4) bpp = 4;
    if (bpp > 8) bpp = 8;

    bitPlanes = bpp;

    irq_set_enabled(DMA_IRQ_0, false);      // stop interrupts on DMA channels
    gpio_init(DISPLAY_OENPIN);          // switch display OFF
    gpio_set_dir(DISPLAY_OENPIN, GPIO_OUT);
    gpio_put(DISPLAY_OENPIN, 1);

    if (pio_sm_is_claimed(display_pio, display_sm_ctrl))
    {
        pio_sm_set_enabled(display_pio, display_sm_ctrl, false);
        pio_sm_init(display_pio, display_sm_ctrl, 0, NULL);
        pio_sm_unclaim(display_pio, display_sm_ctrl);
    }

    if (pio_sm_is_claimed(display_pio, display_sm_data))
    {
        pio_sm_set_enabled(display_pio, display_sm_data, false);
        pio_sm_init(display_pio, display_sm_data, 0, NULL);
        pio_sm_unclaim(display_pio, display_sm_data);
    }

    pio_clear_instruction_memory(display_pio);

    if (dma_channel_is_claimed(display_dma_chan))
    {
        dma_channel_abort(display_dma_chan);
        dma_channel_config c = dma_channel_get_default_config(display_dma_chan);
        channel_config_set_enable(&c, false);
        dma_channel_set_config(display_dma_chan, &c, false);
        dma_channel_unclaim(display_dma_chan);
    }

    if (dma_channel_is_claimed(ctrl_dma_chan))
    {
        dma_channel_abort(ctrl_dma_chan);
        dma_channel_config c = dma_channel_get_default_config(ctrl_dma_chan);
        channel_config_set_enable(&c, false);
        dma_channel_set_config(ctrl_dma_chan, &c, false);
        dma_channel_unclaim(ctrl_dma_chan);
    }
    irq_remove_handler(DMA_IRQ_0, dma_hub75_handler);

    if (frameBuffer)
        free(frameBuffer);

#if HUB75_SIZE == 4040
    frameBuffer = (uint32_t*)malloc(bitPlanes * (DISPLAY_WIDTH / 4) * DISPLAY_SCAN * sizeof(uint32_t));
    memset(frameBuffer, 0, bitPlanes * (DISPLAY_WIDTH / 4) * DISPLAY_SCAN * sizeof(uint32_t));
#elif HUB75_SIZE == 8080
    frameBuffer = (uint32_t*)malloc(bitPlanes * (DISPLAY_WIDTH / 2) * DISPLAY_SCAN * sizeof(uint32_t));
    memset(frameBuffer, 0, bitPlanes * (DISPLAY_WIDTH / 2) * DISPLAY_SCAN * sizeof(uint32_t));
#endif

    if (overlayBuffer)
        free(overlayBuffer);

#if HUB75_SIZE == 4040
    overlayBuffer = (uint8_t*)malloc(bitPlanes * (DISPLAY_WIDTH / 4) * DISPLAY_SCAN * sizeof(uint8_t));
    memset(overlayBuffer, 0, bitPlanes * (DISPLAY_WIDTH / 4) * DISPLAY_SCAN * sizeof(uint8_t));
#elif HUB75_SIZE == 8080
    overlayBuffer = (uint8_t*)malloc(bitPlanes * (DISPLAY_WIDTH / 2) * DISPLAY_SCAN * sizeof(uint8_t));
    memset(overlayBuffer, 0, bitPlanes * (DISPLAY_WIDTH / 2) * DISPLAY_SCAN * sizeof(uint8_t));
#endif

    if (ctrlBuffer)
        free(ctrlBuffer);

    ctrlBuffer = (uint32_t*)malloc(bitPlanes * DISPLAY_SCAN * sizeof(uint32_t));
    memset(ctrlBuffer, 0, bitPlanes * DISPLAY_SCAN * sizeof(uint32_t));

    hub75_init();
    hub75_start();
}


void  hub75_set_masterbrightness(int brt)
{
    masterBrightness = brt;
}



void hub75_set_overlaycolor(int index, rgb_t color)
{
    if (index < 1 || index > 15)    // index 0 is used internally for 'no overlay'
        return;
    overlayColors[index] = color;
}



int hub75_update(rgb_t *image, uint8_t *overlay)
{
    int x, y, b, plane;
    uint32_t *ip;
    uint32_t *fp, *cp;
    uint8_t flag = 0;

    for (b = (8 - bitPlanes); b < 8; b++)     // only MSB bits of RGB color
    {
        ip = image;
        fp = &frameBuffer[(b - (8 - bitPlanes)) * DISPLAY_SCAN * (DISPLAY_WIDTH / 4)];
        cp = &ctrlBuffer[(b - (8 - bitPlanes)) * DISPLAY_SCAN];

        for (y = 0; y < DISPLAY_SCAN; y++)
        {
            uint32_t* ip_uu = image + (y * DISPLAY_WIDTH);
            uint32_t* ip_lu = image + ((y + DISPLAY_SCAN) * DISPLAY_WIDTH);
            uint8_t* op_uu = overlay + (y * DISPLAY_WIDTH);
            uint8_t* op_lu = overlay + ((y + DISPLAY_SCAN) * DISPLAY_WIDTH);

            for (x = 0; x < DISPLAY_WIDTH / 4; x++)     // 4 pixels per framebuffer word
            {
                uint32_t ipu = *ip_uu++;
                uint32_t ipl = *ip_lu++;
                if (*op_uu != 0)
                    ipu = overlayColors[*op_uu];
                op_uu++;
                if (*op_lu != 0)
                    ipl = overlayColors[*op_lu];
                op_lu++;
                
                uint32_t img = (((ipu & (1 << b))>>b)<<2 |
                    (((ipu >> 8) & (1 << b)) >> b) << 1 |
                    ((ipu >> 16) & (1 << b)) >> b) |
                    ((((ipl & (1 << b)) >> b) << 2 |
                        (((ipl >> 8) & (1 << b)) >> b) << 1 |
                        (((ipl >> 16) & (1 << b))) >> b) << 3);
                ipu = *ip_uu++;
                ipl = *ip_lu++;
                if (*op_uu != 0)
                    ipu = overlayColors[*op_uu];
                op_uu++;
                if (*op_lu != 0)
                    ipl = overlayColors[*op_lu];
                op_lu++;
                img |= ((((ipu & (1 << b)) >> b) << 2 |
                    (((ipu >> 8) & (1 << b)) >> b) << 1 |
                    ((ipu >> 16) & (1 << b)) >> b) |
                    ((((ipl & (1 << b)) >> b) << 2 |
                        (((ipl >> 8) & (1 << b)) >> b) << 1 |
                        (((ipl >> 16) & (1 << b))) >> b) << 3)) << 8;
                ipu = *ip_uu++;
                ipl = *ip_lu++;
                if (*op_uu != 0)
                    ipu = overlayColors[*op_uu];
                op_uu++;
                if (*op_lu != 0)
                    ipl = overlayColors[*op_lu];
                op_lu++;
                img |= ((((ipu & (1 << b)) >> b) << 2 |
                    (((ipu >> 8) & (1 << b)) >> b) << 1 |
                    ((ipu >> 16) & (1 << b)) >> b) |
                    ((((ipl & (1 << b)) >> b) << 2 |
                        (((ipl >> 8) & (1 << b)) >> b) << 1 |
                        (((ipl >> 16) & (1 << b))) >> b) << 3)) << 16;
                ipu = *ip_uu++;
                ipl = *ip_lu++;
                if (*op_uu != 0)
                    ipu = overlayColors[*op_uu];
                op_uu++;
                if (*op_lu != 0)
                    ipl = overlayColors[*op_lu];
                op_lu++;
                img |= ((((ipu & (1 << b)) >> b) << 2 |
                    (((ipu >> 8) & (1 << b)) >> b) << 1 |
                    ((ipu >> 16) & (1 << b)) >> b) |
                    ((((ipl & (1 << b)) >> b) << 2 |
                        (((ipl >> 8) & (1 << b)) >> b) << 1 |
                        (((ipl >> 16) & (1 << b))) >> b) << 3)) << 24;
          
                *fp++ = img;
            }

            uint32_t ctrl = ((y) & 0x1F);                           // ADDR lines: bits 0..4
            ctrl |= ((1u << (b - (8 - bitPlanes)) - 1u) << 6) << 5;                 // DELAY:      bits 5..17
            ctrl |= ((63 - (masterBrightness & 0x3f)) << ((b - (8 - bitPlanes)) > 0 ? (b - (8 - bitPlanes)) - 1 : 0)) << 18;  // BRT:        bits 18..30

            *cp++ = ctrl;
        }
    }

    return 0;
}
