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


/////////////////////////////////////////////
//      Conways Game of Life
/////////////////////////////////////////////
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include <math.h>
#include "FreeRTOS.h"
#include "task.h"
#include "pico/stdlib.h"
#include "ps_debug.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hub75.h"
#include "LEDmx.h"

#define   MIN_X               0
#define   MIN_Y               0
#define   MAX_X               (LEDS_X-1)                   // Display Max Koords
#define   MAX_Y               (LEDS_Y-1)

uint32_t 	GolTimer = 5;		// 20 frames per second

static rgb_t	gol_ColorLiveCell = RGB(50, 255, 0);
static rgb_t	gol_ColorDeadCell = RGB(10,20,0);

uint16_t 	maxCells = 250;   // cells when game starts
uint16_t 	minCells = 100;
uint16_t 	newCells = 60;

uint16_t    totalCells;         // # of living cells
uint16_t 	lifeIndicatorMin = 50;   // at least this number of cells should be alive

uint16_t 	ageLevel1 = 10;
uint16_t 	ageLevel2 = 27;
uint16_t 	ageLevel3 = 51;
static uint32_t 	displayTimeout = 0;


#define DEAD    0
#define ALIVE   1


typedef struct playGround_s
{
    uint8_t last[MAX_X + 1][MAX_Y + 1];
    uint8_t next[MAX_X + 1][MAX_Y + 1];
} playGround_t;


playGround_t playGround;


/*
 23/3 rule:
 Birth when 3 neighbours
 Dead: lower than 2 neighbours
 Dead: too much neighbours
 Stay alive when 2 or 3 neighbours
 */


uint32_t GetRandomNumber(void)
{
    uint32_t r = random();
    return r;
}



// fill play ground with random cewlls
static void fillRandomField()
{
    uint16_t k;
    uint16_t x, y;

    k = maxCells;
    LEDmx_ClearScreen(gol_ColorDeadCell);
    do
    {
        x = GetRandomNumber() % (MAX_X - 1);
        y = GetRandomNumber() % (MAX_Y - 1);
        if (playGround.last[x][y] != ALIVE)
        {
            k--;
            playGround.last[x][y] = ALIVE;
            playGround.last[x + 1][y] = ALIVE;
            playGround.last[x + 1][y + 1] = ALIVE;
        }
    } while (k > 0);
}



// Berechnet die Alterung einer Zelle
static void showCellAge(uint8_t* ptr, int16_t x, int16_t y)
{
    if ((*ptr)++ >= ageLevel3)
    {
        *ptr = DEAD;
        LEDmx_DrawPixel(x, y, gol_ColorDeadCell);
    }
    else if (*ptr >= ageLevel2)
    {
        LEDmx_DrawPixel(x, y, RGB(255, (ageLevel3 - *ptr) * 10, 0));
    }
    else if (*ptr >= ageLevel1)
        LEDmx_DrawPixel(x, y, RGB(0, (*ptr - ageLevel1) * 10, 255));
}




static uint16_t getNextGeneration()
{
    int16_t x, y;
    uint16_t xpos, ypos;
    int16_t x1, y1;
    uint16_t lifeIndicator;
    uint8_t* i;

    memset(playGround.next, DEAD, sizeof(playGround.next));
    lifeIndicator = 0;
    totalCells = 0;

    for (xpos = MIN_X; xpos <= MAX_X; xpos++)
    {
        for (ypos = MIN_Y; ypos <= MAX_Y; ypos++)
        {
            if (playGround.last[xpos][ypos] >= ALIVE)
            {
                totalCells++;
                // count number of neighbours
                for (y = ypos - 1; y <= ypos + 1; y++)
                {
                    for (x = xpos - 1; x <= xpos + 1; x++)
                    {
                        x1 = x;
                        y1 = y;

                        if (x < 0)
                            x1 = MAX_X;   // field wrap when on left or right border
                        else if (x > MAX_X)
                            x1 = 0;

                        if (y < 0)
                            y1 = MAX_Y; 
                        else if (y > MAX_Y)
                            y1 = 0;

                        playGround.next[x1][y1]++;
                    }
                }
                playGround.next[xpos][ypos]--; // don't count myself
            }
        }
    }

 // apply GOL rules to our play ground

    for (x = MIN_X; x <= MAX_X; x++)
    {
        for (y = MIN_Y; y <= MAX_Y; y++)
        {
            i = &playGround.last[x][y];

            switch (playGround.next[x][y])
            {
            case 2:     // has 2 neighbours -> stay alive and calc age
                if (*i >= ALIVE)
                    showCellAge(i, x, y);

                break;
            case 3:     // has 3 neighbours -> get alive when was dead; keep alive 
                if (*i == DEAD)
                {
                    *i = ALIVE;
                    LEDmx_DrawPixel(x, y, gol_ColorLiveCell);

                    lifeIndicator++;
                }
                 else
                    showCellAge(i, x, y);

                break;
            default:            // more than 3 or lower than 2 neighbours  -> die
               if (*i >= ALIVE)
                {
                    *i = DEAD;
                    LEDmx_DrawPixel(x, y, gol_ColorDeadCell);
                    lifeIndicator++;
                }
            }
        }
    }

    return (lifeIndicator);

}


static void createNewCells(uint32_t lifeIndicator)
{
    uint16_t x, y, i;

    if (lifeIndicator < lifeIndicatorMin || totalCells < minCells)
    {
        i = newCells;
        do
        {
            x = GetRandomNumber() % (MAX_X - 1);
            y = GetRandomNumber() % (MAX_Y - 1);
            if (playGround.last[x][y] != ALIVE)
            {
                i--;
                playGround.last[x][y] = ALIVE;
                playGround.last[x + 1][y] = ALIVE;
                playGround.last[x + 1][y + 1] = ALIVE;
            }
        }
        while (i > 0);
    }

}




// Hauptroutine
void life(uint16_t cmd)		// 0 = init; >0 -> do 1 step
{
    unsigned int changes;

    if (cmd == 0)
    {
        short pit = 134;
        seed48(&pit);

        LEDmx_ClearScreen(gol_ColorDeadCell);
        
        // Feld 1 löschen
        memset(playGround.last, DEAD, sizeof(playGround.last));
        memset(playGround.next, DEAD, sizeof(playGround.next));
        fillRandomField();         //  Feld zufaellig bespielen
        displayTimeout = xTaskGetTickCount() + GolTimer;

        return;
    }

    changes = getNextGeneration();
    createNewCells(changes);
    LOG_DEBUG("life: changes=%d \n", changes);
}


