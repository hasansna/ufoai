/**
 * @file m_node_button.c
 * @todo add an icon if possible.
 * @todo implement clicked button when its possible.
 * @todo allow autosize (use the size need looking string, problem when we change langage)
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "../client.h"
#include "m_node_button.h"
#include "m_node_custombutton.h"
#include "m_main.h"
#include "m_parse.h"
#include "m_font.h"
#include "m_input.h"
#include "m_drawutil.h"

static const int TILE_SIZE = 64;

static const int CORNER_SIZE = 17;
static const int MID_SIZE = 1;
static const int MARGE = 3;

/**
 * @brief Handled alfer the end of the load of the node from script (all data and/or child are set)
 */
static void MN_ButtonNodeLoaded (menuNode_t *node) {
#ifdef DEBUG
	if (node->size[0] < CORNER_SIZE + MID_SIZE + CORNER_SIZE || node->size[1] < CORNER_SIZE + MID_SIZE + CORNER_SIZE)
		Com_DPrintf(DEBUG_CLIENT, "Node '%s.%s' too small. It can create graphical bugs\n", node->menu->name, node->name);
#endif
}

/**
 * @brief Handles Button clicks
 * @sa MN_BUTTON
 * @todo node->disabled is not need, a disabled node must not receive
 * any input event; but it dont work like that for the moment
 */
static void MN_ButtonNodeClick (menuNode_t * node, int x, int y)
{
	if (node->click && !node->disabled) {
		MN_ExecuteActions(node->menu, node->click);
	}
}

/**
 * @brief Handles Button draw
 */
static void MN_ButtonNodeDraw (menuNode_t *node)
{
	const char *text;
	const char *font;
	int texX, texY;
	const float *textColor;
	const char *image;
	vec2_t pos;
	static vec4_t disabledColor = {0.5, 0.5, 0.5, 1.0};

	if (!node->click || node->disabled) {
		/** @todo need custom color when button is disabled */
		textColor = disabledColor;
		texX = TILE_SIZE;
		texY = TILE_SIZE;
	} else if (node->state) {
		textColor = node->selectedColor;
		texX = TILE_SIZE;
		texY = 0;
	} else {
		textColor = node->color;
		texX = 0;
		texY = 0;
	}

	MN_GetNodeAbsPos(node, pos);

	image = MN_GetReferenceString(node->menu, node->dataImageOrModel);
	if (image) {
		MN_DrawPanel(pos, node->size, image, node->blend, texX, texY, CORNER_SIZE, MID_SIZE, MARGE);
	}

	text = MN_GetReferenceString(node->menu, node->text);
	if (text != NULL && *text != '\0') {
		font = MN_GetFont(node->menu, node);
		R_ColorBlend(textColor);
		/** @todo remove the *1.5, only here because R_FontDrawString ius buggy */
		R_FontDrawString(font, ALIGN_CC, pos[0] + (node->size[0] / 2), pos[1] + (node->size[1] / 2),
			pos[0], pos[1], node->size[0] * 1.5, node->size[1],
			0, _(text), 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
		R_ColorBlend(NULL);
	}
}

/**
 * @brief Handles Button before loading. Used to init node attributes
 * @todo remove the align comment when we update the event handler (problem with the current one)
 */
static void MN_ButtonNodeLoading (menuNode_t *node)
{
	/* default text align */
	/* node->align = ALIGN_CC; */
}

void MN_RegisterNodeButton (nodeBehaviour_t *behaviour)
{
	behaviour->name = "button";
	behaviour->draw = MN_ButtonNodeDraw;
	behaviour->loaded = MN_ButtonNodeLoaded;
	behaviour->leftClick = MN_ButtonNodeClick;
	behaviour->loading = MN_ButtonNodeLoading;
}
