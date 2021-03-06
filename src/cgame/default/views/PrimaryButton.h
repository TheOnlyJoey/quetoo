/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
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

#pragma once

#include <ObjectivelyMVC/Button.h>

/**
 * @file
 * @brief Primary Button.
 */

#define DEFAULT_PRIMARY_BUTTON_WIDTH 200

typedef struct PrimaryButton PrimaryButton;
typedef struct PrimaryButtonInterface PrimaryButtonInterface;

/**
 * @brief The PrimaryButton type.
 * @extends Button
 */
struct PrimaryButton {

	/**
	 * @brief The superclass.
	 * @private
	 */
	Button button;

	/**
	 * @brief The interface.
	 * @private
	 */
	PrimaryButtonInterface *interface;
};

/**
 * @brief The PrimaryButton interface.
 */
struct PrimaryButtonInterface {

	/**
	 * @brief The superclass interface.
	 */
	ButtonInterface buttonInterface;

	/**
	 * @fn PrimaryButton *PrimaryButton::initWithFrame(PrimaryButton *self, const SDL_Rect *frame, ControlStyle style)
	 * @brief Initializes this PrimaryButton with the specified frame and style.
	 * @param frame The frame.
	 * @param style The ControlStyle.
	 * @return The initialized PrimaryButton, or `NULL` on error.
	 * @memberof PrimaryButton
	 */
	PrimaryButton *(*initWithFrame)(PrimaryButton *self, const SDL_Rect *frame, ControlStyle style);
};

/**
 * @fn Class *PrimaryButton::_PrimaryButton(void)
 * @brief The PrimaryButton archetype.
 * @return The PrimaryButton Class.
 * @memberof PrimaryButton
 */
extern Class *_PrimaryButton(void);

