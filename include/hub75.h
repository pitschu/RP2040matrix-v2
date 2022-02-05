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

#pragma once

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "math.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/dma.h"


// Integer between 1 and 8
// Lower numbers cause LSBs to be skipped
#define DISPLAY_MAXPLANES 8

extern uint16_t    bitPlanes;

#ifdef PCB_LAYOUT_V1

#if HUB75_SIZE == 4040
#ifdef HUB75_BCM
#include "ps_hub75_64_BCM.pio.h"       // generated by pioasm (in build dir)
#else
#include "ps_hub75_64.pio.h"       // generated by pioasm (in build dir)
#endif

// Size of the display
#define DISPLAY_WIDTH   64
#define DISPLAY_HEIGHT  64

// Amount of pixels per framebuffer
#define DISPLAY_FRAMEBUFFER_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT)

//; 11 = LATCH, 12 = OE, side set 13 = CLK
//; Data pins are 0..5: R0, G0, B0, R1, G1, B1
//; Row sel pins are : 6..10 : A ..E

// R0, G0, B0, R1, G1, B1 pins, consecutive
#define DISPLAY_DATAPINS_BASE 0
#define DISPLAY_DATAPINS_COUNT 6

// A, B, C, D. E pins for row selection, consecutive
#define DISPLAY_ROWSEL_BASE 6
#define DISPLAY_ROWSEL_COUNT 5

// Must be consecutive
#define DISPLAY_LATCHPIN 11
#define DISPLAY_OENPIN 12
#define DISPLAY_CLKPIN 13

#define PIO_DATA_OUT_BASE       DISPLAY_DATAPINS_BASE
#define PIO_DATA_OUT_CNT        DISPLAY_DATAPINS_COUNT
#define PIO_DATA_SET_BASE       DISPLAY_LATCHPIN
#ifdef HUB75_BCM
#define PIO_DATA_SET_CNT        2   // LATCH and OE in BCM version
#else
#define PIO_DATA_SET_CNT        1   // only LATCH in PWM version
#endif
#define PIO_DATA_SIDE_BASE      DISPLAY_CLKPIN 
#define PIO_DATA_SIDE_CNT       1

#define PIO_CTRL_OUT_BASE       DISPLAY_ROWSEL_BASE
#define PIO_CTRL_OUT_CNT        DISPLAY_ROWSEL_COUNT
#define PIO_CTRL_SET_BASE       DISPLAY_LATCHPIN
#define PIO_CTRL_SET_CNT        2
#define PIO_CTRL_SIDE_BASE      0
#define PIO_CTRL_SIDE_CNT       0
#else
    #error "V1 board only supports 64x64 display"
#endif

#endif


#ifdef PCB_LAYOUT_V2
#if HUB75_SIZE == 4040
#ifdef HUB75_BCM
#include "ps_hub75_64_BCM.pio.h"       // generated by pioasm (in build dir)
#else
#include "ps_hub75_64.pio.h"       // generated by pioasm (in build dir)
#endif

// Size of the display
#define DISPLAY_WIDTH   64
#define DISPLAY_HEIGHT  64

// Scan factor of the display
#define DISPLAY_SCAN 32

// 26 = LATCH, 27 = OE, side set 28 = CLK
// Data pins are 12..17: R0, G0, B0, R1, G1, B1
// Data pins are 6..11: R2, G2, B2, R3, G3, B3
// Row sel pins are : 18..22 : A ..E
// R0, G0, B0, R1, G1, B1 pins, consecutive

#define DISPLAY_DATAPINS_BASE 6
#define DISPLAY_DATAPINS_COUNT 6

// A, B, C, D. E pins for row selection, consecutive
#define DISPLAY_ROWSEL_BASE 18
#define DISPLAY_ROWSEL_COUNT 5

// Must be consecutive
#define DISPLAY_LATCHPIN 26
#define DISPLAY_OENPIN 27
#define DISPLAY_CLKPIN 28

#define PIO_DATA_OUT_BASE       DISPLAY_DATAPINS_BASE
#define PIO_DATA_OUT_CNT        DISPLAY_DATAPINS_COUNT
#define PIO_DATA_SET_BASE       DISPLAY_LATCHPIN
#ifdef HUB75_BCM
#define PIO_DATA_SET_CNT        2   // LATCH and OE in BCM version
#else
#define PIO_DATA_SET_CNT        1   // only LATCH in PWM version
#endif
#define PIO_DATA_SIDE_BASE      DISPLAY_CLKPIN 
#define PIO_DATA_SIDE_CNT       1

#define PIO_CTRL_OUT_BASE       DISPLAY_ROWSEL_BASE
#define PIO_CTRL_OUT_CNT        DISPLAY_ROWSEL_COUNT
#define PIO_CTRL_SET_BASE       DISPLAY_LATCHPIN
#define PIO_CTRL_SET_CNT        2
#define PIO_CTRL_SIDE_BASE      0
#define PIO_CTRL_SIDE_CNT       0

#elif HUB75_SIZE == 8080
#ifdef HUB75_BCM
#include "ps_hub75_128_BCM.pio.h"       // generated by pioasm (in build dir)
#else
#include "ps_hub75_128.pio.h"       // generated by pioasm (in build dir)
#endif

// Size of the display
#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  128

// Scan factor of the display
#define DISPLAY_SCAN 32

// 26 = LATCH, 27 = OE, side set 28 = CLK
// Data pins are 12..17: R0, G0, B0, R1, G1, B1
// Data pins are 6..11: R2, G2, B2, R3, G3, B3
// Row sel pins are : 18..22 : A ..E

#define DISPLAY_DATAPINS_BASE 6
#define DISPLAY_DATAPINS_COUNT 12

// A, B, C, D. E pins for row selection, consecutive
#define DISPLAY_ROWSEL_BASE 18
#define DISPLAY_ROWSEL_COUNT 5

// Must be consecutive
#define DISPLAY_LATCHPIN 26
#define DISPLAY_OENPIN 27
#define DISPLAY_CLKPIN 28

#define PIO_DATA_OUT_BASE       DISPLAY_DATAPINS_BASE
#define PIO_DATA_OUT_CNT        DISPLAY_DATAPINS_COUNT
#define PIO_DATA_SET_BASE       DISPLAY_LATCHPIN
#ifdef HUB75_BCM
#define PIO_DATA_SET_CNT        2   // LATCH and OE in BCM version
#else
#define PIO_DATA_SET_CNT        1   // only LATCH in PWM version
#endif
#define PIO_DATA_SIDE_BASE      DISPLAY_CLKPIN 
#define PIO_DATA_SIDE_CNT       1

#define PIO_CTRL_OUT_BASE       DISPLAY_ROWSEL_BASE
#define PIO_CTRL_OUT_CNT        DISPLAY_ROWSEL_COUNT
#define PIO_CTRL_SET_BASE       DISPLAY_LATCHPIN
#define PIO_CTRL_SET_CNT        2
#define PIO_CTRL_SIDE_BASE      0
#define PIO_CTRL_SIDE_CNT       0

#else
    #error "V2 board supports 64x64 or 128x128 layouts"
#endif

#endif

// -- generic code ---------------------------------------------------------

//the pixel image in RGB
typedef struct rgbValue_s {
    uint8_t		R;
    uint8_t		G;
    uint8_t		B;
} rgbValue_t;

typedef uint32_t	rgb_t;

// Amount of pixels per framebuffer
#define DISPLAY_FRAMEBUFFER_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT)

// Scan factor of the display
#define DISPLAY_SCAN 32


/*! \brief Configure and start the HUB75 driver hardware
 *  \ingroup HUB75
 *
 * \param bpp Sets the number of bit planes to be used
 * The driver supports from 4 up to 8 pixel planes. This function first stops all driver operation
 * currently running and then reconfigures all required PIO and DMA devices. 
 */
void hub75_config(int bpp);



/*! \brief Update the LED matrix screen buffer
 *  \ingroup HUB75
 *
 * \param image Pointer to image to be displayed
 * \param overlay Pointer to image to be displayed as overlay 
 * This function transfers the given image and overlay into the framebuffer used by the driver 
 * to control the PIO and DMA devices.
 */
int hub75_update(rgb_t* image, uint8_t* overlay);


/*! \brief Set master brightness value
 *  \ingroup HUB75
 *
 * \param brt New brightness value
 * The brt value can range from 0 to DISPLAY_WIDTH-4, where 0 is OFF
 */
void    hub75_set_masterbrightness(int brt);

/*! \brief Set overlay color with index 
 *  \ingroup HUB75
 *
 * \param index Index in overlay color table (range 1..15)
 * \param color Color to be set (RGB value)
 * Set a color in the overlay color lookup table. This table has 15 entries for 15 colors
 */
void    hub75_set_overlaycolor(int index, rgb_t color);
