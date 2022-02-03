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
//      Run the pong game as an overlay
/////////////////////////////////////////////
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
#include "ps_debug.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hub75.h"
#include "LEDmx.h"

#define   MIN_X               0
#define   MIN_Y               0
#define   MAX_X               (LEDS_X-1)                   // Display Max Koords
#define   MAX_Y               (LEDS_Y-1)

/**********************************************************************************************/

typedef struct playGround_s
{
	int l, t, r, b;
} playGround_t;

static playGround_t playGround;

#define GAMELOOP_TICKS 3

static void init_game(void);
static int playGame(int para);

#define PLAYER_LEN 10
#define BALL_SIZE 5

typedef struct ball_s
{
	int x, y;	/* position on the screen */
	int w, h;	// ball width and height
	int dx, dy; /* movement vector */
} ball_t;

typedef struct player
{
	int x, y;
	int w, h;
} paddle_t;

// Program globals
static ball_t ball;
static paddle_t player[2];
int score[] = {0, 0};
int width, height; //used if fullscreen

// Check if balls collides with player >pid>
static int check_collision(ball_t ball, int pid)
{

	int b_l, p_l;
	int b_r, p_r;
	int b_t, p_t;
	int b_b, p_b;

	paddle_t *p = &player[pid];

	b_l = ball.x;
	b_r = ball.x + ball.w-1;
	b_t = ball.y;
	b_b = ball.y + ball.h-1;

	p_l = p->x;
	p_r = p->x + p->w-1;
	p_t = p->y;
	p_b = p->y + p->h-1;

	if (pid == 0 && (b_l > p_r))
		return 0;

	if (pid == 1 && (b_r < p_l))
	{
		return 0;
	}

	if (b_t > p_b)
	{
		return 0;
	}

	if (b_b < p_t)
	{
		return 0;
	}

	return 1;
}

/* This routine moves each ball by its motion vector. */
static void move_ball(int countDown)
{
	uint8_t coll = 0;

	if (ball.dx == 0 && ball.dy == 0)
		return;

	/* Move the ball by its motion vector. */
	ball.x += ball.dx;
	ball.y += ball.dy;

	if (ball.y < playGround.t || ball.y > (playGround.b - ball.h))
	{
		ball.dy = -ball.dy;
	}

	//check for collision with the player
	int i;

	for (i = 0; i < 2; i++)
	{
		//collision detected
		if (check_collision(ball, i) == 1)
		{
			//change ball direction
			ball.dx = (i == 0 ? 1 : -1) * (countDown < (8 * configTICK_RATE_HZ) ? 2 : ((random() % 2) + 1));
			ball.dy = (random() % 5) - 2;

			coll = 1 + i;
			LOG_DEBUG("Collision paddle=%d @ countdown=%d, speed=%d\n", i, countDown, ball.dx);
		}
	}

	if (coll == 0 && ball.x <= (player[0].x + 1)) // ball reaches left goal  -> stop game
	{
		if (countDown < (4 * configTICK_RATE_HZ))
		{
			ball.dx = 0; // stop ball
			ball.dy = 0;
			LOG_DEBUG("Game stopped @ countdown=%d\n", countDown);
		}
	}

	if (ball.x < playGround.l)
		ball.x = playGround.l;
	if (ball.x > (playGround.r - ball.w))
		ball.x = (playGround.r - ball.w);

	if (ball.y < playGround.t)
		ball.y = playGround.t;
	if (ball.y > (playGround.b - ball.h))
		ball.y = (playGround.b - ball.h);
}


static void move_paddle(int padID, int countDown)
{

	int pc = player[padID].y + player[padID].h / 2;
	int sc = (playGround.b - playGround.t) / 2 + playGround.t;
	int ball_speed = abs(ball.dx) + 1;

	if (ball.dx == 0 && ball.dy == 0) // game stopped when left player got goal
		return;

	int fault = (padID == 0 && ball.dx < 0 && countDown < (4 * configTICK_RATE_HZ)) ? 1 : 0;

	//ball moving right
	if ((ball.dx > 0 && padID == 0) || (ball.dx < 0 && padID == 1))
	{
		//return to pc position
		if (pc < (sc - 3))
		{
			player[padID].y += 3;
		}
		if (pc > (sc + 3))
		{
			player[padID].y -= 3;
		}
	}
	else
	{
		if (!fault)
		{
			//ball moving down
			if (ball.dy > 0)
			{
				if (ball.y > (pc - ball_speed))
				{
					player[padID].y += ball_speed;
				}
				if (ball.y < (pc + ball_speed))
				{
					player[padID].y -= ball_speed;
				}
			}

			//ball moving up
			if (ball.dy < 0)
			{
				if (ball.y < (pc + ball_speed))
				{
					player[padID].y -= ball_speed;
				}
				if (ball.y > (pc - ball_speed))
				{
					player[padID].y += ball_speed;
				}
			}

			//ball moving stright across
			if (ball.dy == 0)
			{
				if (ball.y < (pc + 1))
				{
					player[padID].y -= 1;
				}
				if (ball.y > (pc - 1))
				{
					player[padID].y += 1;
				}
			}
		}
		else
		{
			//ball moving down
			if (ball.dy > 0)
			{
				if (ball.y > sc)
					player[padID].y -= 2;
				else
					player[padID].y += 2;
			}

			//ball moving up
			if (ball.dy < 0)
			{
				if (ball.y > sc)
					player[padID].y += 2;
				else
					player[padID].y -= 2;
			}

			//ball moving stright across
			if (ball.dy == 0)
			{
				if (ball.y > sc)
				{
					if ((player[padID].y + player[padID].h) > ball.y)
						player[padID].y -= 2;
				}
				else
				{
					if (player[padID].y < (ball.y + ball.h))
						player[padID].y += 2;
				}
			}
		}
	}

	if (player[padID].y < playGround.t)
		player[padID].y = playGround.t;
	if (player[padID].y > (playGround.b - player[padID].h))
		player[padID].y = (playGround.b - player[padID].h);
}



void initPongGame(void)
{
	playGround.l = 10;
	playGround.r = 50;
	playGround.t = 10;
	playGround.b = 50;

	LEDmx_ClearOverlay();
	LEDmx_SetOverlayColor(1, YELLOW);
	LEDmx_SetOverlayColor(2, RED);
	LEDmx_SetOverlayColor(3, LTGREEN);

	LEDmx_DrawLine(playGround.l - 1, playGround.t - 1, playGround.r + 1, playGround.t - 1, 1, true);
	LEDmx_DrawLine(playGround.l - 1, playGround.b + 1, playGround.r + 1, playGround.b + 1, 1, true);

	LEDmx_DrawLine(playGround.l - 1, playGround.t - 1, playGround.l - 1, playGround.b + 1, 1, true);
	LEDmx_DrawLine(playGround.r + 1, playGround.t - 1, playGround.r + 1, playGround.b + 1, 1, true);

	LEDmx_DrawLine(playGround.l + (playGround.r - playGround.l) / 2, playGround.t - 1,
		playGround.l + (playGround.r - playGround.l) / 2, playGround.b + 1, 1, true);

	ball.x = playGround.r / 2;
	ball.y = playGround.b / 2;
	ball.w = BALL_SIZE;
	if (ball.w < 3)
		ball.w = 3;
	if (ball.w > 10)
		ball.w = 10;
	ball.h = ball.w;
	ball.dy = 1;
	ball.dx = 1;

	player[0].x = playGround.l;
	player[0].y = playGround.b / 2;
	player[0].w = 2;
	player[0].h = PLAYER_LEN;

	player[1].x = playGround.r - 1;
	player[1].y = playGround.b / 2;
	player[1].w = 2;
	player[1].h = PLAYER_LEN;

}


/*
 * PlayGame() runs in the task context of LED matrix driver
 */
int playPongGame(int countDown)
{
	int i, x, y;

	LEDmx_getFlushSemaphore();

	LEDmx_Rect(playGround.l, playGround.t, playGround.r, playGround.b, 0, true);
	LEDmx_DrawLine(playGround.l + (playGround.r - playGround.l) / 2, playGround.t - 1,
		playGround.l + (playGround.r - playGround.l) / 2, playGround.b + 1, 1, true);
	move_ball(countDown);
	move_paddle(0, countDown);
	move_paddle(1, countDown);

	for (i = 0; i < 2; i++)
	{
		for (y = 0; y < player[i].h; y++)
		{
			for (x = 0; x < player[i].w; x++)
			{
				LEDmx_SetOverlayPixel(player[i].x + x, player[i].y + y, 2);
			}
		}
	}

	for (y = 0; y < ball.h; y++)
	{
		for (x = 0 + (y == 0 || y == (ball.h - 1) ? 1 : 0); x < ball.w - (y == 0 || y == (ball.h - 1) ? 1 : 0); x++)
		{
			LEDmx_SetOverlayPixel(ball.x + x, ball.y + y, 3);
		}
	}
	LEDmx_putFlushSemaphore();

	return 0;
}



