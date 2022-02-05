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
#ifndef LEDMX__H_
#define LEDMX__H_

#include "hub75.h"

#define	 LEDS_X		    DISPLAY_WIDTH	
#define	 LEDS_Y			DISPLAY_HEIGHT

extern uint16_t    bitPlanes;

#define BRT_TOP_LIMIT	(LEDS_X)
#define BRT_BOT_LIMIT	0

#define     RGB(r, g, b)                (((r & 0xFF) << 16) | ((g & 0xFF) << 8) | ((b & 0xFF)))
#define 	MAP_888_to_565(c)			(uint16_t)(((c >> 8) & 0xF8) | ((c >> 5) & 0x7E) | ((c >> 3) & 0x1F))
#define 	MAP_565_to_888(c)			(rgb_t)((((rgb_t)(c) & 0xf800) << 8) | ((c & 0x03E0) << 5) | ((c & 0x1f) << 3))

#define 	MAP_888_R_TO_PWM(r)			((((uint32_t)(r) >> 16) & 0xff) >> (8-bitPlanes))
#define 	MAP_888_G_TO_PWM(g)			((((uint32_t)(g) >> 8 ) & 0xff) >> (8-bitPlanes))
#define 	MAP_888_B_TO_PWM(b)			((((uint32_t)(b)      ) & 0xff) >> (8-bitPlanes))

#define 	MAP_888_R(r)			((((uint32_t)(r) >> 16) & 0xff))
#define 	MAP_888_G(g)			((((uint32_t)(g) >> 8 ) & 0xff))
#define 	MAP_888_B(b)			((((uint32_t)(b)      ) & 0xff))

#define     NONE_COLOR                  RGB(0x60, 0x78, 0x58)
#define     BLACK                       RGB(0x00, 0x00, 0x00)
#define     DKGRAY                      RGB(0x40, 0x40, 0x40)
#define     GRAY                        RGB(0x80, 0x80, 0x80)
#define     LTGRAY                      RGB(0xA0, 0xA0, 0xA0)
#define     WHITE                       RGB(0xFF, 0xFF, 0xFF)
#define     DKRED                       RGB(0x60, 0x00, 0x00)
#define     RED                         RGB(0xFF, 0x00, 0x00)
#define     LTRED                       RGB(0xFF, 0x40, 0x40)
#define     DKGREEN                     RGB(0x00, 0x70, 0x00)
#define     GREEN                       RGB(0x00, 0xEF, 0x00)
#define     LTGREEN                     RGB(0x50, 0xFF, 0x50)
#define     DKBLUE                      RGB(0x00, 0x00, 0x60)
#define     BLUE                        RGB(0x10, 0x10, 0xFF)
#define     LTBLUE                      RGB(0x60, 0x60, 0xFF)
#define     DKYELLOW                    RGB(0x80, 0x80, 0x00)
#define     YELLOW                      RGB(0xFF, 0xFF, 0x00)
#define     LTYELLOW                    RGB(0xFF, 0xFF, 0x80)
#define     DKMAGENTA                   RGB(0x60, 0x00, 0x60)
#define     MAGENTA                     RGB(0xFF, 0x00, 0xFF)
#define     LTMAGENTA                   RGB(0xFF, 0x60, 0xFF)
#define     DKCYAN                      RGB(0x00, 0x60, 0x60)
#define     CYAN                        RGB(0x00, 0xB0, 0xB0)
#define     LTCYAN                      RGB(0x60, 0xFF, 0xFF)
#define     GOLD                        RGB(0x90, 0x90, 0x30)

// pitschu: some very useful and often used macros
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

#define constrain(x,low,high) (((x)<=(low))?(low):(((x)>=(high))?(high):(x)))
#define round(x)     ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))
#define putInRange(V,VMIN,VMAX) 		max(VMIN,min(VMAX,V))
#define mapToRange(V,VMIN0,VMAX0,VMIN1,VMAX1) ((VMIN1) + (putInRange(V,VMIN0,VMAX0) - (VMIN0)) * ((VMAX1) - (VMIN1)) / ((VMAX0) - (VMIN0)))



typedef struct alpha_s {
    uint8_t		active;				// alpha channel is active
    uint8_t		r, g, b;
} alpha_t;

extern uint32_t display_buffers[DISPLAY_FRAMEBUFFER_SIZE];
extern uint32_t* display_front_buf;
extern uint32_t* display_back_buf;

extern uint8_t  overlayBuffer[DISPLAY_FRAMEBUFFER_SIZE];

void LEDmx_getFlushSemaphore(void);
void LEDmx_putFlushSemaphore(void);

void LEDmx_start();
void LEDmx_SetMasterBrightness(int brt);

void LEDmx_SetPixel(int x, int y, rgb_t color);
void LEDmx_SetPixelRGB(int x, int y, uint8_t R, uint8_t G, uint8_t B);
void LEDmx_DrawPixel(int16_t x, int16_t y, rgb_t color);
void LEDmx_Rect(int16_t l, int16_t t, int16_t r, int16_t b, rgb_t color, bool overlay);
void LEDmx_setAlphaRGB(uint8_t r, uint8_t g, uint8_t b);
void LEDmx_setAlpha(rgb_t color);
void LEDmx_setAlphaDisabled(void);
void LEDmx_ClearScreen(rgb_t color);
void LEDmx_BlankScreen(void);
void LEDmx_SetClip(int16_t l, int16_t r, int16_t t, int16_t b);
uint8_t LEDmx_IsClipped(int16_t x, int16_t y);

void LEDmx_DrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, rgb_t color, bool overlay);

void LEDmx_ClearOverlay (void);
void LEDmx_SetOverlayPixel(int x, int y, int color);
void LEDmx_SetOverlayColor(int index, rgb_t color);

uint32_t LEDmx_565toRGB(uint16_t pix);
#endif