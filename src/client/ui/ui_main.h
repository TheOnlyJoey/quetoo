/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __UI_MAIN_H__
#define __UI_MAIN_H__

#include "ui_types.h"

void Ui_Draw(void);
bool Ui_Event(SDL_Event *event);
void Ui_Init(void);
void Ui_Shutdown(void);

#ifdef __UI_LOCAL_H__

typedef struct ui_s {
	TwType OffOrOn;
	TwType OffLowMediumHigh;

	TwBar *root;
	TwBar *servers;
	TwBar *controls;
	TwBar *player;
	TwBar *system;

	TwBar *top;
} ui_t;

extern ui_t ui;

#endif /* __UI_LOCAL_H__ */

#endif /* __UI_MAIN_H__ */
