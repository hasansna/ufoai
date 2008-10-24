/**
 * @file m_node_map.c
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
#include "../cl_map.h"
#include "m_main.h"
#include "m_draw.h"
#include "m_parse.h"
#include "m_font.h"
#include "m_dragndrop.h"
#include "m_tooltip.h"
#include "m_nodes.h"
#include "m_node_model.h"
#include "../cl_global.h"

static void MN_DrawMapNode (menuNode_t *node)
{
	if (curCampaign) {
		/* don't run the campaign in console mode */
		if (cls.key_dest != key_console)
			CL_CampaignRun();	/* advance time */
		MAP_DrawMap(node); /* Draw geoscape */
	}
}

static void MN_NodeMapRightClick (menuNode_t *node, int x, int y)
{
	if (!gd.combatZoomOn || !gd.combatZoomedUfo) {
		if (!cl_3dmap->integer)
			mouseSpace = MS_SHIFTMAP;
		else
			mouseSpace = MS_SHIFT3DMAP;
		MAP_StopSmoothMovement();
	}
}

static void MN_NodeMapMiddleClick (menuNode_t *node, int x, int y)
{
	mouseSpace = MS_ZOOMMAP;
}

static void MN_NodeMapMouseWheel (menuNode_t *node, qboolean down, int x, int y)
{
	if (gd.combatZoomOn  && gd.combatZoomedUfo && !down) {
		return;
	} else if (gd.combatZoomOn && gd.combatZoomedUfo && down) {
		MAP_TurnCombatZoomOff();
		return;
	}
	ccs.zoom *= pow(0.995, (down ? 10: -10));
	if (ccs.zoom < cl_mapzoommin->value)
		ccs.zoom = cl_mapzoommin->value;
	else if (ccs.zoom > cl_mapzoommax->value) {
		ccs.zoom = cl_mapzoommax->value;
		if (!down) {
			MAP_TurnCombatZoomOn();
		}
	}

	if (!cl_3dmap->integer) {
		if (ccs.center[1] < 0.5 / ccs.zoom)
			ccs.center[1] = 0.5 / ccs.zoom;
		if (ccs.center[1] > 1.0 - 0.5 / ccs.zoom)
			ccs.center[1] = 1.0 - 0.5 / ccs.zoom;
	}
	MAP_StopSmoothMovement();
}

void MN_RegisterNodeMap (nodeBehaviour_t *behaviour)
{
	behaviour->name = "map";
	behaviour->draw = MN_DrawMapNode;
	behaviour->leftClick = MAP_MapClick;
	behaviour->rightClick = MN_NodeMapRightClick;
	behaviour->middleClick = MN_NodeMapMiddleClick;
	behaviour->mouseWheel = MN_NodeMapMouseWheel;
}
