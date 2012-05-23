/**
 * @file
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "../../cl_shared.h"
#include "../ui_nodes.h"
#include "../ui_parse.h"
#include "../ui_actions.h"
#include "../ui_behaviour.h"
#include "ui_node_window.h"
#include "ui_node_special.h"
#include "ui_node_abstractnode.h"

/**
 * @brief Call after the script initialized the node
 * @todo special cases should be managed as a common property event of the parent node
 */
void uiFuncNode::onLoaded (uiNode_t *node)
{
	/** @todo move this code into the parser (it should not create a node) */
	const value_t *prop = UI_GetPropertyFromBehaviour(node->parent->behaviour, node->name);
	if (prop && prop->type == V_UI_ACTION) {
		if (uiAction_t*& value = Com_GetValue<uiAction_t*>(node->parent, prop)) {
			Com_Printf("UI_FuncNodeLoaded: '%s' already defined. Second function ignored (\"%s\")\n", prop->string, UI_GetPath(node));
		} else {
			value = node->onClick;
		}
	}
}

void UI_RegisterFuncNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "func";
	behaviour->isVirtual = qtrue;
	behaviour->isFunction = qtrue;
	behaviour->manager = new uiFuncNode();
}

void UI_RegisterNullNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "";
	behaviour->isVirtual = qtrue;
}

/**
 * @brief Callback to execute a confunc
 */
static void UI_ConfuncCommand_f (void)
{
	uiNode_t *node = static_cast<uiNode_t*>(Cmd_Userdata());
	assert(node);
	assert(UI_NodeInstanceOf(node, "confunc"));
	UI_ExecuteConFuncActions(node, node->onClick);
}

/**
 * @brief Checks whether the given node is a virtual confunc that can be overridden from inheriting nodes.
 * @param node The node to check (must be a confunc node).
 * @return @c true if the given node is a dummy node, @c false otherwise.
 */
static qboolean UI_ConFuncIsVirtual (const uiNode_t *const node)
{
	/* magic way to know if it is a dummy node (used for inherited confunc) */
	const uiNode_t *dummy = static_cast<const uiNode_t*>(Cmd_GetUserdata(node->name));
	assert(node);
	assert(UI_NodeInstanceOf(node, "confunc"));
	return (dummy != NULL && dummy->parent == NULL);
}

/**
 * @brief Call after the script initialized the node
 */
void uiConFuncNode::onLoaded (uiNode_t *node)
{
	/* register confunc non inherited */
	if (node->super == NULL) {
		/* don't add a callback twice */
		if (!Cmd_Exists(node->name)) {
			Cmd_AddCommand(node->name, UI_ConfuncCommand_f, "Confunc callback");
			Cmd_AddUserdata(node->name, node);
		} else {
			Com_Printf("UI_ParseNodeBody: Command name for confunc '%s' already registered\n", UI_GetPath(node));
		}
	} else {
		uiNode_t *dummy;

		/* convert a confunc to an "inherited" confunc if it is possible */
		if (Cmd_Exists(node->name)) {
			if (UI_ConFuncIsVirtual(node))
				return;
		}

		dummy = UI_AllocNode(node->name, "confunc", qfalse);
		Cmd_AddCommand(node->name, UI_ConfuncCommand_f, "Inherited confunc callback");
		Cmd_AddUserdata(dummy->name, dummy);
	}
}

void uiConFuncNode::clone (const uiNode_t *source, uiNode_t *clone)
{
	onLoaded(clone);
}

/**
 * @brief Callback every time the parent window is opened (pushed into the active window stack)
 */
void uiConFuncNode::onWindowOpened (uiNode_t *node, linkedList_t *params)
{
	if (UI_ConFuncIsVirtual(node)) {
		const value_t *property = UI_GetPropertyFromBehaviour(node->behaviour, "onClick");
		uiNode_t *userData = static_cast<uiNode_t*>(Cmd_GetUserdata(node->name));
		UI_AddListener(userData, property, node);
	}
}

/**
 * @brief Callback every time the parent window is closed (pop from the active window stack)
 */
void uiConFuncNode::onWindowClosed (uiNode_t *node)
{
	if (UI_ConFuncIsVirtual(node)) {
		const value_t *property = UI_GetPropertyFromBehaviour(node->behaviour, "onClick");
		uiNode_t *userData = static_cast<uiNode_t*>(Cmd_GetUserdata(node->name));
		UI_RemoveListener(userData, property, node);
	}
}

void UI_RegisterConFuncNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "confunc";
	behaviour->isVirtual = qtrue;
	behaviour->isFunction = qtrue;
	behaviour->manager = new uiConFuncNode();
}

static void UI_CvarListenerNodeCallback (const char *cvarName, const char *oldValue, const char *newValue, void *data)
{
	linkedList_t *list = static_cast<linkedList_t*>(data);
	linkedList_t *params = NULL;
	LIST_AddString(&params, oldValue);
	LIST_AddString(&params, newValue);
	while (list) {
		uiNode_t *node = static_cast<uiNode_t*>(list->data);
		UI_ExecuteEventActionsEx(node, node->onClick, params);
		list = list->next;
	}
}

/**
 * @brief Callback every time the parent window is opened (pushed into the active window stack)
 */
static void UI_CvarListenerNodeBind (uiNode_t *node)
{
	cvarChangeListener_t *l;
	l = Cvar_RegisterChangeListener(node->name, UI_CvarListenerNodeCallback);
	if (l == NULL) {
		Com_Printf("Node %s is not binded: cvar %s not found\n", UI_GetPath(node), node->name);
		return;
	}
	linkedList_t *list = static_cast<linkedList_t*>(l->data);
	if (LIST_GetPointer(list, node) == NULL) {
		LIST_AddPointer(&list, node);
	}
}

/**
 * @brief Callback every time the parent window is closed (pop from the active window stack)
 */
void uiCvarNode::onWindowClosed (uiNode_t *node)
{
	cvar_t *var;

	var = Cvar_FindVar(node->name);
	if (var == NULL)
		return;

	cvarChangeListener_t *l = var->changeListener;
	while (l) {
		if (l->exec == UI_CvarListenerNodeCallback) {
			linkedList_t *list = static_cast<linkedList_t*>(l->data);
			LIST_Remove(&list, node);
			if (LIST_IsEmpty(list)) {
				Cvar_UnRegisterChangeListener(node->name, UI_CvarListenerNodeCallback);
			}
			break;
		}
		l = l->next;
	}
}

void uiCvarNode::onWindowOpened (uiNode_t *node, linkedList_t *params)
{
	UI_CvarListenerNodeBind(node);
}

void uiCvarNode::deleteNode (uiNode_t *node)
{
	onWindowClosed(node);
}

void uiCvarNode::clone (const uiNode_t *source, uiNode_t *clone)
{
	UI_CvarListenerNodeBind(clone);
}

static void UI_CvarListenerNodeForceBind (uiNode_t *node, const uiCallContext_t *context)
{
	UI_CvarListenerNodeBind(node);
}

void UI_RegisterCvarFuncNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "cvarlistener";
	behaviour->isVirtual = qtrue;
	behaviour->isFunction = qtrue;
	behaviour->manager = new uiCvarNode();
	/* Force to bind the node to the cvar */
	UI_RegisterNodeMethod(behaviour, "forceBind", UI_CvarListenerNodeForceBind);
}
