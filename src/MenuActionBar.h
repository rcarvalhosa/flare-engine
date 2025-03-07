/*
Copyright © 2011-2012 Clint Bellanger
Copyright © 2012 Igor Paliychuk
Copyright © 2013 Kurt Rinnert
Copyright © 2012-2016 Justin Jacobs

This file is part of FLARE.

FLARE is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

FLARE is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
FLARE.  If not, see http://www.gnu.org/licenses/
*/

/**
 * class ActionBar
 *
 * Handles the config, display, and usage of the 0-9 hotkeys, mouse buttons, and menu calls
 */

#ifndef MENU_ACTION_BAR_H
#define MENU_ACTION_BAR_H

#include "CommonIncludes.h"
#include "Menu.h"
#include "Utils.h"

class ActionData;
class Power;
class StatBlock;
class WidgetLabel;
class WidgetSlot;

class MenuActionBar : public Menu {
private:
	static const bool IS_EQUIPPED = true;

	void addSlot(unsigned index, int x, int y, bool is_locked);
	void setItemCount(unsigned index, int count, bool is_equipped);

	bool checkMouseMoveTarget(unsigned slot);
	bool checkActionTrigger(unsigned slot, unsigned mouse_move_slot, bool has_mouse_move_target, ActionData& action, bool& have_aim);
	void clearQueuedAction(unsigned slot, std::vector<ActionData>& action_queue);
	void processValidAction(unsigned slot, ActionData& action, bool have_aim, std::vector<ActionData>& action_queue);
	bool handleClickActivation(unsigned slot_index, ActionData& action);
	bool checkHotkeyPress(unsigned slot, ActionData& action, bool& have_aim);
	bool checkResourceRequirements(unsigned slot_index, const Power* power);
	void setupActionProperties(ActionData& action, const Power* power);
	void setActionTarget(ActionData& action, const Power* power, bool have_aim, bool has_mouse_move_target);
	bool canUsePower(unsigned slot_index, const Power* power, const ActionData& action);


	Sprite *sprite_emptyslot;
	WidgetSlot *end_turn_button;  // dedicated end turn button

	Rect src;

	std::vector<std::string> labels;
	std::vector<std::string> menu_labels;

	Point last_mouse;

	std::vector<int> slot_fail_cooldown;

	SoundID sfx_unable_to_cast;

	int tooltip_length;
	bool powers_overlap_slots;

public:
	enum {
		MENU_CHARACTER = 0,
		MENU_INVENTORY = 1,
		MENU_POWERS = 2,
		MENU_LOG = 3
	};
	static const unsigned MENU_COUNT = 4;

	static const int SLOT_MAIN1 = 10;
	static const int SLOT_MAIN2 = 11;
	static const int SLOT_MAX = 12; // maximum number of slots in MenuActionBar

	static const int USE_EMPTY_SLOT = 0;

	static const bool REORDER = true;
	static const bool CLEAR_SKIP_ITEMS = true;
	static const bool SET_SKIP_EMPTY = true;

	MenuActionBar();
	~MenuActionBar();
	void align();
	void loadGraphics();
	void logic();
	void render();
	void checkAction(std::vector<ActionData> &action_queue);
	void pushAction(ActionData& action, std::vector<ActionData>& action_queue, const FPoint* target, bool have_aim);
	PowerID checkDrag(const Point& mouse);
	void checkMenu(bool &menu_c, bool &menu_i, bool &menu_p, bool &menu_l);
	void drop(const Point& mouse, PowerID power_index, bool rearranging);
	void actionReturn(PowerID power_index);
	void remove(const Point& mouse);
	void set(std::vector<PowerID> power_id, bool skip_empty);
	void clear(bool skip_items);
	Point getSlotPos(int slot);

	void renderTooltips(const Point& position);
	bool isWithinSlots(const Point& mouse);
	bool isWithinMenus(const Point& mouse);
	void addPower(const PowerID id, const PowerID target_id);

	WidgetSlot* getSlotFromPosition(const Point& position);

	unsigned slots_count;
	std::vector<PowerID> hotkeys; // refer to power_index in PowerManager
	std::vector<PowerID> hotkeys_temp; // temp for shapeshifting
	std::vector<PowerID> hotkeys_mod; // hotkeys can be changed by items
	std::vector<bool> locked; // if slot is locked, you cannot drop it
	std::vector<bool> prevent_changing;
	std::vector<WidgetSlot *> slots; // hotkey slots
	WidgetSlot *menus[MENU_COUNT]; // menu buttons
	std::string menu_titles[MENU_COUNT];
	std::vector<int> slot_item_count; // -1 means this power isn't item based.  0 means out of items.  1+ means sufficient items.
	bool requires_attention[MENU_COUNT];
	std::vector<bool> slot_activated;

	int drag_prev_slot;
	bool updated;
	int twostep_slot;

	WidgetSlot* touch_slot;
};

#endif
