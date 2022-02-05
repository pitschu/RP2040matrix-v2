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
#include "stdint.h"
#include "stdlib.h"
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "ps_debug.h"
#include "hub75.h"

#if HUB75_SIZE == 4040
uint32_t frameBuffer[DISPLAY_MAXPLANES * (DISPLAY_WIDTH / 4) * DISPLAY_SCAN]; // each entry contains RGB data for 2 or 4 consective pixels on one HUB75 channel
#elif HUB75_SIZE == 8080
uint32_t frameBuffer[DISPLAY_MAXPLANES * (DISPLAY_WIDTH / 2) * DISPLAY_SCAN]; // each entry contains RGB data for 2 pixels on two HUB75 channels
#else
    #error "V2 board supports 64x64 or 128x128 layouts"
#endif
rgb_t* addrBuffer[(1<<DISPLAY_MAXPLANES)];
uint16_t  bcmCounter = 1;     // index in addrBuffer array

uint32_t ctrlBuffer[DISPLAY_MAXPLANES * DISPLAY_SCAN]; // N bit planes * # of scan lines

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


static void dma_hub75_handler()
{
    // Clear the interrupt request.
    if (dma_hw->ints0 & (1u << display_dma_chan))
    {
        dma_hw->ints0 = 1u << display_dma_chan;
        // start next display cycle
        dma_channel_set_read_addr(display_dma_chan, addrBuffer[bcmCounter], true);
        if (++bcmCounter >= (1 << bitPlanes))
        {
            gpio_xor_mask(1<<15);       // debug LED for frame time measurement
            bcmCounter = 1;
        }
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
        DISPLAY_SCAN * ((DISPLAY_WIDTH / 4)),     // complete frame buffer for 1 bit plane
#elif HUB75_SIZE == 8080
        DISPLAY_SCAN* ((DISPLAY_WIDTH / 2)),     // complete frame buffer for 1 bit plane
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
        DISPLAY_SCAN,     // complete frame buffer for 1 bit plane
        false
    );
    dma_channel_set_irq0_enabled(ctrl_dma_chan, true);

    irq_set_exclusive_handler(DMA_IRQ_0, dma_hub75_handler);
    irq_set_priority(DMA_IRQ_0, 1);
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


#if HUB75_SIZE == 4040
    memset(frameBuffer, 0, bitPlanes * (DISPLAY_WIDTH / 4) * DISPLAY_SCAN * sizeof(uint32_t));
#elif HUB75_SIZE == 8080
    memset(frameBuffer, 0, bitPlanes * (DISPLAY_WIDTH / 2) * DISPLAY_SCAN * sizeof(uint32_t));
#else
    #error "V2 board supports 64x64 or 128x128 layouts"
#endif
    memset(ctrlBuffer, 0, bitPlanes * DISPLAY_SCAN * sizeof(uint32_t));
    memset(addrBuffer, 0, (1<<bitPlanes) * sizeof(uint32_t*));
    
    for (int bPos = 0; bPos < bitPlanes; bPos++)
    {
        for (int i = 1; i < (1<<bitPlanes); i++)
        {
            if (i & (1 << bPos) && (addrBuffer[i] == 0))
            {
                LOG_DEBUG("addrBuffer[%3d] = plane %d\n", i, (bitPlanes - bPos));
#if HUB75_SIZE == 4040
                addrBuffer[i] = &frameBuffer[(bitPlanes - 1 - bPos) * (DISPLAY_WIDTH / 4) * DISPLAY_SCAN];
#elif HUB75_SIZE == 8080
                addrBuffer[i] = &frameBuffer[(bitPlanes - 1 - bPos) * (DISPLAY_WIDTH / 2) * DISPLAY_SCAN];
#endif
            }
        }
    }

    hub75_init();
    hub75_start();
}


void  hub75_set_masterbrightness(int brt)
{
    brt += 4;       // OE will be always HIGH during the last 4 pixels
    if (brt < 4) brt = 4;
    if (brt > (DISPLAY_WIDTH - 1)) brt = (DISPLAY_WIDTH - 1);
    masterBrightness = brt;
}



void hub75_set_overlaycolor(int index, rgb_t color)
{
    if (index < 1 || index > 15)    // index 0 is used internally for 'no overlay'
        return;
    overlayColors[index] = color;
}



#if HUB75_SIZE == 4040
int hub75_update(rgb_t *image, uint8_t *overlay)
{
    int x, y, b, plane;
    rgb_t* ip;
    rgb_t* fp, * cp;
    uint8_t flag = 0;
    uint8_t brtCnt = 0;

    for (b = (8 - bitPlanes); b < 8; b++)     // only MSB bits of RGB color
    {
        ip = image;
        fp = &frameBuffer[(b - (8 - bitPlanes)) * DISPLAY_SCAN * (DISPLAY_WIDTH / 4)];
        cp = &ctrlBuffer[(b - (8 - bitPlanes)) * DISPLAY_SCAN];

        for (y = 0; y < DISPLAY_SCAN; y++)
        {
            rgb_t* ip_uu = image + (y * DISPLAY_WIDTH);
            rgb_t* ip_lu = image + ((y + DISPLAY_SCAN) * DISPLAY_WIDTH);
            uint8_t* op_uu = overlay + (y * DISPLAY_WIDTH);
            uint8_t* op_lu = overlay + ((y + DISPLAY_SCAN) * DISPLAY_WIDTH);

            brtCnt = 0;
            for (x = 0; x < DISPLAY_WIDTH / 4; x++)     // 4 pixels per framebuffer word
            {
                rgb_t ipu = *ip_uu++;
                rgb_t ipl = *ip_lu++;

                if (*op_uu != 0)
                    ipu = overlayColors[*op_uu];
                op_uu++;
                if (*op_lu != 0)
                    ipl = overlayColors[*op_lu];
                op_lu++;
                
                rgb_t img = (((ipu & (1 << b)) >> b) << 2 |
                    (((ipu >> 8) & (1 << b)) >> b) << 1 |
                    ((ipu >> 16) & (1 << b)) >> b) |
                    ((((ipl & (1 << b)) >> b) << 2 |
                        (((ipl >> 8) & (1 << b)) >> b) << 1 |
                        (((ipl >> 16) & (1 << b))) >> b) << 3);
                if (++brtCnt > masterBrightness)
                    img |= (1 << 7);

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
                if (++brtCnt > masterBrightness)
                    img |= (1 << 15);

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
                if (++brtCnt > masterBrightness)
                    img |= (1 << 23);

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
                if (++brtCnt > masterBrightness)
                    img |= (1 << 31);
          
                *fp++ = img;
            }

            uint32_t ctrl = ((y) & 0x1F);                           // ADDR lines: bits 0..4

            *cp++ = ctrl;
        }
    }

    return 0;
}


#elif HUB75_SIZE == 8080
int hub75_update(rgb_t* image, uint8_t* overlay)
{
    int x, y, b, plane;
    rgb_t* ip;
    uint32_t* fp, * cp;
    uint8_t flag = 0;
    uint8_t brtCnt = 0;

    for (b = (8 - bitPlanes); b < 8; b++)     // only MSB bits of RGB color
    {
        ip = image;
        fp = &frameBuffer[(b - (8 - bitPlanes)) * DISPLAY_SCAN * (DISPLAY_WIDTH / 2)];
        cp = &ctrlBuffer[(b - (8 - bitPlanes)) * DISPLAY_SCAN];

        for (y = 0; y < DISPLAY_SCAN; y++)
        {
            rgb_t* ip_uu = image + (y * DISPLAY_WIDTH);
            rgb_t* ip_lu = image + ((y + DISPLAY_SCAN) * DISPLAY_WIDTH);
            rgb_t* ip_ul = image + ((y + DISPLAY_HEIGHT/2) * DISPLAY_WIDTH);
            rgb_t* ip_ll = image + (((y + DISPLAY_HEIGHT / 2) + DISPLAY_SCAN) * DISPLAY_WIDTH);
            uint8_t* op_uu = overlay + (y * DISPLAY_WIDTH);
            uint8_t* op_lu = overlay + ((y + DISPLAY_SCAN) * DISPLAY_WIDTH);
            uint8_t* op_ul = overlay + ((y + DISPLAY_HEIGHT / 2) * DISPLAY_WIDTH);
            uint8_t* op_ll = overlay + (((y + DISPLAY_HEIGHT / 2) + DISPLAY_SCAN) * DISPLAY_WIDTH);

            brtCnt = 0;
            for (x = 0; x < DISPLAY_WIDTH / 2; x++)     // 4 pixels per framebuffer word
            {
                rgb_t ipuu = *ip_uu++;
                rgb_t iplu = *ip_lu++;
                rgb_t ipul = *ip_ul++;
                rgb_t ipll = *ip_ll++;

                if (*op_uu != 0) ipuu = overlayColors[*op_uu];
                op_uu++;
                if (*op_lu != 0) iplu = overlayColors[*op_lu];
                op_lu++;
                if (*op_ul != 0) ipul = overlayColors[*op_ul];
                op_ul++;
                if (*op_ll != 0) ipll = overlayColors[*op_ll];
                op_ll++;

                rgb_t img = (((ipuu & (1 << b)) >> b) << 2 |
                        (((ipuu >> 8) & (1 << b)) >> b) << 1 |
                        ((ipuu >> 16) & (1 << b)) >> b) |
                    ((((iplu & (1 << b)) >> b) << 2 |
                        (((iplu >> 8) & (1 << b)) >> b) << 1 |
                        (((iplu >> 16) & (1 << b))) >> b) << 3) |
                    ((((ipul & (1 << b)) >> b) << 2 |
                        (((ipul >> 8) & (1 << b)) >> b) << 1 |
                        (((ipul >> 16) & (1 << b))) >> b) << 6) |
                    ((((ipll & (1 << b)) >> b) << 2 |
                        (((ipll >> 8) & (1 << b)) >> b) << 1 |
                        (((ipll >> 16) & (1 << b))) >> b) << 9);

                if (++brtCnt > masterBrightness) img |= (1 << 12);

                ipuu = *ip_uu++;
                iplu = *ip_lu++;
                ipul = *ip_ul++;
                ipll = *ip_ll++;

                if (*op_uu != 0) ipuu = overlayColors[*op_uu];
                op_uu++;
                if (*op_lu != 0) iplu = overlayColors[*op_lu];
                op_lu++;
                if (*op_ul != 0) ipul = overlayColors[*op_ul];
                op_ul++;
                if (*op_ll != 0) ipll = overlayColors[*op_ll];
                op_ll++;

                img |= ((((ipuu & (1 << b)) >> b) << 2 |
                        (((ipuu >> 8) & (1 << b)) >> b) << 1 |
                        ((ipuu >> 16) & (1 << b)) >> b) |
                    ((((iplu & (1 << b)) >> b) << 2 |
                        (((iplu >> 8) & (1 << b)) >> b) << 1 |
                        (((iplu >> 16) & (1 << b))) >> b) << 3) |
                    ((((ipul & (1 << b)) >> b) << 2 |
                        (((ipul >> 8) & (1 << b)) >> b) << 1 |
                        (((ipul >> 16) & (1 << b))) >> b) << 6) |
                    ((((ipll & (1 << b)) >> b) << 2 |
                        (((ipll >> 8) & (1 << b)) >> b) << 1 |
                        (((ipll >> 16) & (1 << b))) >> b) << 9)) << 16;

                if (++brtCnt > masterBrightness) img |= (1 << (16+12));

                *fp++ = img;
            }

            uint32_t ctrl = ((y) & 0x1F);                           // ADDR lines: bits 0..4

            *cp++ = ctrl;
        }
    }

    return 0;
}
#endif
