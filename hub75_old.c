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


uint32_t frameBuffer[8 * (DISPLAY_WIDTH / 2) * DISPLAY_SCAN] ; //   __attribute__((aligned(32768)));

int     masterBrightness = 31;

PIO display_pio = pio0;
uint display_sm_data;
uint display_offset_data;

int display_dma_chan;

bool display_redraw = false;
bool display_flip = false;

int display_redraw_curidx = 0;

uint8_t display_framenum = 0;


static void dma_handler() {
    // Clear the interrupt request.
    dma_hw->ints0 = 1u << display_dma_chan;
    // start next display cycle
    dma_channel_set_read_addr(display_dma_chan, &frameBuffer[0], true);
}



void hub75_init() 
{
    // Initialize PIO
    display_sm_data = pio_claim_unused_sm(display_pio, true);

    display_offset_data = pio_add_program(display_pio, &ps_hub75_data_program);

    ps_hub75_data_program_init(
            display_pio,
            display_sm_data,
            display_offset_data,
            DISPLAY_DATAPINS_BASE,
            DISPLAY_CLKPIN
            );

    // Initialize DMA
    display_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(display_dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, DREQ_PIO0_TX0+display_sm_data);

//    channel_config_set_ring(&c, false, 15);     // ring size is 8192

    dma_channel_configure(
            display_dma_chan,
            &c,
            &pio0_hw->txf[display_sm_data],
            NULL,  // Will be set later for each transfer
            8 * ((DISPLAY_WIDTH / 2) * DISPLAY_SCAN),     // complete frame buffer
            false
            ); 
    // Tell the DMA to raise IRQ line 0 when the channel finishes a block
    dma_channel_set_irq0_enabled(display_dma_chan, true);
    // Configure the processor to run dma_handler() when DMA IRQ 0 is asserted
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}



void hub75_start() {
    dma_handler();      // start DMA
}




int hub75_update(int state, uint32_t *image)
{
    int x, y, b, plane;
    uint32_t *ip;
    uint32_t *fp;
    uint8_t flag = 0;

    for (b = 0; b < 8; b++)
    {
        ip = image;
        fp = &frameBuffer[b * DISPLAY_SCAN * (DISPLAY_WIDTH/2)];

        for (y = 0; y < DISPLAY_SCAN; y++)
        {
            uint32_t *ip_u = image + (y * DISPLAY_WIDTH);
            uint32_t* ip_l = image + ((y + DISPLAY_SCAN) * DISPLAY_WIDTH);

            for (x = 0; x < DISPLAY_WIDTH / 2; x++)
            {
                uint32_t img = (((*ip_u & (1 << b))>>b)<<2 |
                                (((*ip_u >> 8) & (1 << b))>>b)<<1 | 
                                ((*ip_u>>16) & (1 << b))>>b) |
                               ((((*ip_l & (1 << b))>>b)<<2 | 
                                 (((*ip_l >> 8) & (1 << b))>>b)<<1 | 
                                 (((*ip_l >> 16) & (1 << b)))>>b) << 3);
                ip_u++;
                ip_l++;
                img |= ((((*ip_u & (1 << b)) >> b) << 2 |
                        (((*ip_u >> 8) & (1 << b)) >> b) << 1 |
                        ((*ip_u >> 16) & (1 << b)) >> b) |
                       ((((*ip_l & (1 << b)) >> b) << 2 |
                         (((*ip_l >> 8) & (1 << b)) >> b) << 1 |
                         (((*ip_l >> 16) & (1 << b))) >> b) << 3)) << 16;
                ip_u++;
                ip_l++;

                if (x == (DISPLAY_WIDTH / 2) - 2)       // 
                {
                    uint32_t delay = (((1u << b) - 1u));
                    img |= (delay & 0x1fu) << (11u + 16u) | (delay & 0x3e0u) << (11u - 5u);
                }

                if (x == (DISPLAY_WIDTH / 2) - 1)
                {
                    uint32_t brt = 31 - (masterBrightness & 0x1f);

                    img |= (brt & 0x1fu) << (11u + 16u) | (brt & 0x3e0u) << (11u - 5u);

                    img |= ((((y) & 0x1F) << ROW_SHIFT) | ((y) & 0x1F) << (ROW_SHIFT + 16));
               }
                else
                    img |= ((((y-1) & 0x1F) << ROW_SHIFT) | (y-1 & 0x1F) << (ROW_SHIFT + 16));
                
                *fp++ = img;
            }
        }
    }

    return state;
}


void  hub75_set_masterbrightness(int brt)
{
    masterBrightness = brt;
    
}


static inline uint32_t LEDmx_565toRGB(uint16_t pix) {
    uint32_t r_gamma = pix & 0xf800u;
    r_gamma *= r_gamma;
    uint32_t g_gamma = pix & 0x07e0u;
    g_gamma *= g_gamma;
    uint32_t b_gamma = pix & 0x001fu;
    b_gamma *= b_gamma;
    return (b_gamma >> 2 << 16) | (g_gamma >> 14 << 8) | (r_gamma >> 24 << 0);
}
