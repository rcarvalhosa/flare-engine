/*
Copyright © 2011-2012 Clint Bellanger
Copyright © 2012 Igor Paliychuk
Copyright © 2013 Kurt Rinnert
Copyright © 2014 Henrik Andersson
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
 * class MenuActionBar
 *
 * Handles the config, display, and usage of the 0-9 hotkeys, mouse buttons, and menu calls
 */

#include "Avatar.h"
#include "CommonIncludes.h"
#include "EngineSettings.h"
#include "FileParser.h"
#include "FontEngine.h"
#include "InputState.h"
#include "MapRenderer.h"
#include "Menu.h"
#include "MenuActionBar.h"
#include "MenuInventory.h"
#include "MenuManager.h"
#include "MenuPowers.h"
#include "MenuTouchControls.h"
#include "MessageEngine.h"
#include "Platform.h"
#include "PowerManager.h"
#include "RenderDevice.h"
#include "Settings.h"
#include "SharedGameResources.h"
#include "SharedResources.h"
#include "SoundManager.h"
#include "StatBlock.h"
#include "TooltipManager.h"
#include "UtilsParsing.h"
#include "WidgetLabel.h"
#include "WidgetSlot.h"
#include "CombatManager.h"

#include <climits>

MenuActionBar::MenuActionBar()
	: sprite_emptyslot(NULL)
	, end_turn_button(NULL)
	, sfx_unable_to_cast(0)
	, tooltip_length(MenuPowers::TOOLTIP_LONG_MENU)
	, powers_overlap_slots(false)
	, slots_count(0)
	, drag_prev_slot(-1)
	, updated(false)
	, twostep_slot(-1)
	, touch_slot(NULL) {

	menu_labels.resize(MENU_COUNT);

	tablist = TabList();
	tablist.setScrollType(Widget::SCROLL_TWO_DIRECTIONS);
	tablist.lock();

	for (unsigned i=0; i<MENU_COUNT; i++) {
		menus[i] = new WidgetSlot(WidgetSlot::NO_ICON, WidgetSlot::HIGHLIGHT_NORMAL);
		menus[i]->setHotkey(Input::CHARACTER + i);
		menus[i]->show_colorblind_highlight = true;

		// NOTE: This prevents these buttons from being clickable unless they get defined in the config file.
		// However, it doesn't prevent them from being added to the tablist, so they can still be activated there despite being invisible
		// Mods should be expected to define these slot positions. This just keeps things from getting *too* broken if they're not defined.
		menus[i]->pos.w = 0;
		menus[i]->pos.h = 0;
	}

	menu_titles[MENU_CHARACTER] = msg->get("Character");
	menu_titles[MENU_INVENTORY] = msg->get("Inventory");
	menu_titles[MENU_POWERS] = msg->get("Powers");
	menu_titles[MENU_LOG] = msg->get("Log");

	// Read data from config file
	FileParser infile;

	// @CLASS MenuActionBar|Description of menus/actionbar.txt
	if (infile.open("menus/actionbar.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL)) {
		while (infile.next()) {
			if (parseMenuKey(infile.key, infile.val))
				continue;

			// @ATTR slot|repeatable(int, int, int, bool) : Index, X, Y, Locked|Index (max 10) and position for power slot. If a slot is locked, its Power can't be changed by the player.
			if (infile.key == "slot") {
				unsigned index = Parse::popFirstInt(infile.val);
				if (index == 0 || index > 10) {
					infile.error("MenuActionBar: Slot index must be in range 1-10.");
				}
				else {
					int x = Parse::popFirstInt(infile.val);
					int y = Parse::popFirstInt(infile.val);
					std::string val = Parse::popFirstString(infile.val);
					bool is_locked = (val.empty() ? false : Parse::toBool(val));
					addSlot(index-1, x, y, is_locked);
				}
			}
			// @ATTR slot_M1|point, bool : Position, Locked|Position for the primary action slot. If the slot is locked, its Power can't be changed by the player.
			else if (infile.key == "slot_M1") {
				int x = Parse::popFirstInt(infile.val);
				int y = Parse::popFirstInt(infile.val);
				std::string val = Parse::popFirstString(infile.val);
				bool is_locked = (val.empty() ? false : Parse::toBool(val));
				addSlot(10, x, y, is_locked);
			}
			// @ATTR slot_M2|point, bool : Position Locked|Position for the secondary action slot. If the slot is locked, its Power can't be changed by the player.
			else if (infile.key == "slot_M2") {
				int x = Parse::popFirstInt(infile.val);
				int y = Parse::popFirstInt(infile.val);
				std::string val = Parse::popFirstString(infile.val);
				bool is_locked = (val.empty() ? false : Parse::toBool(val));
				addSlot(11, x, y, is_locked);
			}

			// @ATTR char_menu|point|Position for the Character menu button.
			else if (infile.key == "char_menu") {
				int x = Parse::popFirstInt(infile.val);
				int y = Parse::popFirstInt(infile.val);
				menus[MENU_CHARACTER]->setBasePos(x, y, Utils::ALIGN_TOPLEFT);
				menus[MENU_CHARACTER]->pos.w = menus[MENU_CHARACTER]->pos.h = eset->resolutions.icon_size;
			}
			// @ATTR inv_menu|point|Position for the Inventory menu button.
			else if (infile.key == "inv_menu") {
				int x = Parse::popFirstInt(infile.val);
				int y = Parse::popFirstInt(infile.val);
				menus[MENU_INVENTORY]->setBasePos(x, y, Utils::ALIGN_TOPLEFT);
				menus[MENU_INVENTORY]->pos.w = menus[MENU_INVENTORY]->pos.h = eset->resolutions.icon_size;
			}
			// @ATTR powers_menu|point|Position for the Powers menu button.
			else if (infile.key == "powers_menu") {
				int x = Parse::popFirstInt(infile.val);
				int y = Parse::popFirstInt(infile.val);
				menus[MENU_POWERS]->setBasePos(x, y, Utils::ALIGN_TOPLEFT);
				menus[MENU_POWERS]->pos.w = menus[MENU_POWERS]->pos.h = eset->resolutions.icon_size;
			}
			// @ATTR log_menu|point|Position for the Log menu button.
			else if (infile.key == "log_menu") {
				int x = Parse::popFirstInt(infile.val);
				int y = Parse::popFirstInt(infile.val);
				menus[MENU_LOG]->setBasePos(x, y, Utils::ALIGN_TOPLEFT);
				menus[MENU_LOG]->pos.w = menus[MENU_LOG]->pos.h = eset->resolutions.icon_size;
			}
			// @ATTR end_turn_menu|point|Position for the End Turn menu button.
			else if (infile.key == "end_turn_menu") {
				int x = Parse::popFirstInt(infile.val);
				int y = Parse::popFirstInt(infile.val);
				end_turn_button->setBasePos(x, y, Utils::ALIGN_BOTTOMRIGHT);
				end_turn_button->pos.w = end_turn_button->pos.h = eset->resolutions.icon_size;
			}
			// @ATTR tooltip_length|["short", "long_menu", "long_all"]|The length of power descriptions in tooltips. 'short' will display only the power name. 'long_menu' (the default setting) will display full tooltips, but only for powers that are in the Powers menu. 'long_all' will display full tooltips for all powers.
			else if (infile.key == "tooltip_length") {
				if (infile.val == "short")
					tooltip_length = MenuPowers::TOOLTIP_SHORT;
				else if (infile.val == "long_menu")
					tooltip_length = MenuPowers::TOOLTIP_LONG_MENU;
				else if (infile.val == "long_all")
					tooltip_length = MenuPowers::TOOLTIP_LONG_ALL;
				else
					infile.error("MenuActionBar: '%s' is not a valid tooltip_length setting.", infile.val.c_str());
			}
			// @ATTR powers_overlap_slots|bool|When true, the power icon is drawn on top of the empty slot graphic for any given slot. If false, the empty slot graphic will only be drawn if there's not a power in the slot. The default value is false.
			else if (infile.key == "powers_overlap_slots") {
				powers_overlap_slots = Parse::toBool(infile.val);
			}

			else infile.error("MenuActionBar: '%s' is not a valid key.", infile.key.c_str());
		}
		infile.close();
	}

	for (unsigned i=0; i<MENU_COUNT; i++) {
		tablist.add(menus[i]);
	}

	slots_count = static_cast<unsigned>(slots.size());

	hotkeys.resize(slots_count);
	hotkeys_temp.resize(slots_count);
	hotkeys_mod.resize(slots_count);
	locked.resize(slots_count);
	slot_item_count.resize(slots_count);
	slot_activated.resize(slots_count);
	slot_fail_cooldown.resize(slots_count);

	clear(!MenuActionBar::CLEAR_SKIP_ITEMS);

	loadGraphics();

	if (!eset->misc.sfx_unable_to_cast.empty())
		sfx_unable_to_cast = snd->load(eset->misc.sfx_unable_to_cast, "MenuActionBar unable to cast");

	align();

	menu_act = this;
}

void MenuActionBar::addSlot(unsigned index, int x, int y, bool is_locked) {
	if (index >= slots.size()) {
		labels.resize(index+1);
		slots.resize(index+1, NULL);
	}

	slots[index] = new WidgetSlot(WidgetSlot::NO_ICON, WidgetSlot::HIGHLIGHT_NORMAL);
	slots[index]->setBasePos(x, y, Utils::ALIGN_TOPLEFT);
	slots[index]->pos.w = slots[index]->pos.h = eset->resolutions.icon_size;
	slots[index]->continuous = true;

	if (index < 10)
		slots[index]->setHotkey(Input::BAR_1 + index);
	else if (index < 12)
		slots[index]->setHotkey(Input::MAIN1 + index - 10);

	prevent_changing.resize(slots.size());
	prevent_changing[index] = is_locked;

	tablist.add(slots[index]);
}

void MenuActionBar::align() {
	Menu::align();

	for (unsigned i = 0; i < slots_count; i++) {
		if (slots[i]) {
			slots[i]->setPos(window_area.x, window_area.y);
		}
	}
	for (unsigned i=0; i<MENU_COUNT; i++) {
		menus[i]->setPos(window_area.x, window_area.y);
	}

	// set keybinding labels
	for (unsigned i = 0; i < static_cast<unsigned>(SLOT_MAIN1); i++) {
		if (i < slots.size() && slots[i]) {
			labels[i] = msg->getv("Hotkey: %s", inpt->getBindingString(i + Input::BAR_1).c_str());
		}
	}

	for (unsigned i = SLOT_MAIN1; i < static_cast<unsigned>(SLOT_MAX); i++) {
		if (i < slots.size() && slots[i]) {
			if (settings->mouse_move && ((i == SLOT_MAIN2 && settings->mouse_move_swap) || (i == SLOT_MAIN1 && !settings->mouse_move_swap))) {
				labels[i] = msg->getv("Hotkey: %s", std::string(inpt->getBindingString(Input::SHIFT) + " + " + inpt->getBindingString(i - SLOT_MAIN1 + Input::MAIN1)).c_str());
			}
			else {
				labels[i] = msg->getv("Hotkey: %s", inpt->getBindingString(i - SLOT_MAIN1 + Input::MAIN1).c_str());
			}
		}
	}
	for (unsigned i=0; i<menu_labels.size(); i++) {
		menus[i]->setPos(window_area.x, window_area.y);
		menu_labels[i] = msg->getv("Hotkey: %s", inpt->getBindingString(i + Input::CHARACTER).c_str());
	}

	// position the end turn button in the bottom right
	if (end_turn_button) {
		end_turn_button->setBasePos(window_area.w - (eset->resolutions.icon_size * 2), 
								  window_area.h - (eset->resolutions.icon_size * 2),
								  Utils::ALIGN_TOPLEFT);
		end_turn_button->setPos(window_area.x, window_area.y);
	}
}

void MenuActionBar::clear(bool skip_items) {
	// clear action bar
	for (unsigned i = 0; i < slots_count; i++) {
		if (skip_items && powers && powers->isValid(hotkeys_mod[i])) {
			if (!powers->powers[hotkeys_mod[i]]->required_items.empty()) {
				continue;
			}
		}

		hotkeys[i] = 0;
		hotkeys_temp[i] = 0;
		hotkeys_mod[i] = 0;
		slot_item_count[i] = -1;
		locked[i] = false;
		slot_activated[i] = false;
		slot_fail_cooldown[i] = 0;

		if (slots[i])
			slots[i]->enabled = true;
	}

	// clear menu notifications
	for (unsigned i=0; i<MENU_COUNT; i++)
		requires_attention[i] = false;

	twostep_slot = -1;
}

void MenuActionBar::loadGraphics() {
	Image *graphics;

	if (!background)
		setBackground("images/menus/actionbar_trim.png");

	Rect icon_clip;
	icon_clip.w = icon_clip.h = eset->resolutions.icon_size;

	graphics = render_device->loadImage("images/menus/slot_empty.png", RenderDevice::ERROR_NORMAL);
	if (graphics) {
		sprite_emptyslot = graphics->createSprite();
		sprite_emptyslot->setClipFromRect(icon_clip);
		graphics->unref();
	}

	// Initialize end turn button
	end_turn_button = new WidgetSlot(WidgetSlot::NO_ICON, WidgetSlot::HIGHLIGHT_NORMAL);
	end_turn_button->pos.w = end_turn_button->pos.h = eset->resolutions.icon_size;
	end_turn_button->enabled = true;
	end_turn_button->show_colorblind_highlight = true;
	end_turn_button->continuous = false;  // single click activation
	tablist.add(end_turn_button);
}

void MenuActionBar::logic() {
	tablist.logic();
	if (!inpt->usingMouse() && inpt->pressing[Input::ACTIONBAR] && !inpt->lock[Input::ACTIONBAR]) {
		inpt->lock[Input::ACTIONBAR] = true;
		if (tablist.getCurrent() == -1) {
			tablist.unlock();
			if (menu->isDragging())
				tablist.getNext(!TabList::GET_INNER, TabList::WIDGET_SELECT_AUTO);
			else
				tablist.setCurrent(menus[MENU_INVENTORY]);
			menu->defocusLeft();
			menu->defocusRight();
		}
		else {
			tablist.defocus();
		}
	}
	if (tablist.getCurrent() == -1) {
		tablist.lock();
	}

	// Update End Turn button visibility and handle clicks
	if (combat_manager && combat_manager->isInCombat() && combat_manager->isPlayerTurn()) {
		end_turn_button->visible = true;
		end_turn_button->enabled = true;
		int click_result = end_turn_button->checkClick();
		// Accept both DRAG and ACTIVATE as valid click types
		if (click_result == WidgetSlot::DRAG || click_result == WidgetSlot::ACTIVATE) {
			combat_manager->endPlayerTurn();
		}
	}
	else {
		end_turn_button->visible = false;
		end_turn_button->enabled = false;
	}

	// hero has no powers
	if (pc->power_cast_timers.empty())
		return;

	for (unsigned i = 0; i < slots_count; i++) {
		if (!slots[i]) continue;

		if (powers->isValid(hotkeys_mod[i])) {
			const Power* power = powers->powers[hotkeys_mod[i]];

			if (power->required_items.empty()) {
				setItemCount(i, -1, !IS_EQUIPPED);
			}
			else {
				for (size_t j = 0; j < power->required_items.size(); ++j) {
					if (power->required_items[j].equipped) {
						if (!menu->inv->equipmentContain(power->required_items[j].id, 1))
							setItemCount(i, 0, IS_EQUIPPED);
						else
							setItemCount(i, 1, IS_EQUIPPED);
					}
					else {
						if (power->required_items[j].quantity == 0) {
							if (!menu->inv->inventory[MenuInventory::CARRIED].contain(power->required_items[j].id, 1))
								setItemCount(i, 0, IS_EQUIPPED);
							else
								setItemCount(i, 1, IS_EQUIPPED);
						}
						else {
							setItemCount(i, menu->inv->inventory[MenuInventory::CARRIED].count(power->required_items[j].id), !IS_EQUIPPED);
						}
					}

					if (power->required_items[j].quantity > 0)
						break;
				}
			}

			//see if the slot should be greyed out
			slots[i]->enabled = pc->power_cooldown_timers[hotkeys_mod[i]]->isEnd()
							  && pc->power_cast_timers[hotkeys_mod[i]]->isEnd()
							  && pc->stats.canUsePower(hotkeys_mod[i], !StatBlock::CAN_USE_PASSIVE)
							  && (twostep_slot == -1 || static_cast<unsigned>(twostep_slot) == i);

			slots[i]->setIcon(power->icon, WidgetSlot::NO_OVERLAY);

			if (!pc->power_cast_timers[hotkeys_mod[i]]->isEnd() && pc->power_cast_timers[hotkeys_mod[i]]->getDuration() > 0) {
				slots[i]->cooldown = static_cast<float>(pc->power_cast_timers[hotkeys_mod[i]]->getCurrent()) / static_cast<float>(pc->power_cast_timers[hotkeys_mod[i]]->getDuration());
			}
			else if (!pc->power_cooldown_timers[hotkeys_mod[i]]->isEnd() && pc->power_cooldown_timers[hotkeys_mod[i]]->getDuration() > 0) {
				slots[i]->cooldown = static_cast<float>(pc->power_cooldown_timers[hotkeys_mod[i]]->getCurrent()) / static_cast<float>(pc->power_cooldown_timers[hotkeys_mod[i]]->getDuration());
			}
			else {
				slots[i]->cooldown = 1;
			}
		}
		else {
			// no valid power, so treat the slot as empty
			slots[i]->enabled = true;
			slots[i]->cooldown = 0;
		}

		if (slot_fail_cooldown[i] > 0)
			slot_fail_cooldown[i]--;
	}

}

void MenuActionBar::render() {
	Menu::render();

	// draw hotkeyed icons
	for (unsigned i = 0; i < slots_count; i++) {
		if (!slots[i]) continue;

		if (hotkeys[i] == 0 || powers_overlap_slots) {
			// TODO move this to WidgetSlot?
			if (sprite_emptyslot) {
				sprite_emptyslot->setDestFromRect(slots[i]->pos);
				render_device->render(sprite_emptyslot);
			}
		}
		if (hotkeys[i] != 0) {
			slots[i]->render();
		}
	}

	// render primary menu buttons
	for (unsigned i=0; i<MENU_COUNT; i++) {
		menus[i]->highlight = (requires_attention[i] && !menus[i]->in_focus);
		menus[i]->render();
	}

	// render end turn button
	if (end_turn_button && end_turn_button->visible) {
		if (sprite_emptyslot) {
			sprite_emptyslot->setDestFromRect(end_turn_button->pos);
			render_device->render(sprite_emptyslot);
		}
		end_turn_button->render();
	}
}

/**
 * On mouseover, show tooltip for buttons
 */
void MenuActionBar::renderTooltips(const Point& position) {
	TooltipData tip_data;

	// menus
	for (unsigned i = 0; i < MENU_COUNT; ++i) {
		if (Utils::isWithinRect(menus[i]->pos, position)) {
			if (settings->colorblind && requires_attention[i])
				tip_data.addText(menu_titles[i] + " (*)");
			else
				tip_data.addText(menu_titles[i]);

			if (!menu_labels[i].empty()) {
				tip_data.addText(menu_labels[i]);
			}

			tooltipm->push(tip_data, position, TooltipData::STYLE_FLOAT);
			break;
		}
	}

	// end turn button tooltip
	if (end_turn_button && end_turn_button->visible && Utils::isWithinRect(end_turn_button->pos, position)) {
		tip_data.clear();
		tip_data.addText(msg->get("End Turn"));
		tooltipm->push(tip_data, position, TooltipData::STYLE_FLOAT);
	}

	tip_data.clear();

	for (unsigned i = 0; i < slots_count; i++) {
		if (slots[i] && Utils::isWithinRect(slots[i]->pos, position)) {
			if (hotkeys_mod[i] != 0) {
				menu->pow->createTooltipFromActionBar(&tip_data, i, tooltip_length);
			}
			tip_data.addText(labels[i]);
		}
	}

	tooltipm->push(tip_data, position, TooltipData::STYLE_FLOAT);
}

/**
 * After dragging a power or item onto the action bar, set as new hotkey
 */
void MenuActionBar::drop(const Point& mouse, PowerID power_index, bool rearranging) {
	if (!powers->isValid(power_index) || powers->powers[power_index]->no_actionbar)
		return;

	for (unsigned i = 0; i < slots_count; i++) {
		if (slots[i] && Utils::isWithinRect(slots[i]->pos, mouse)) {
			if (rearranging) {
				if (prevent_changing[i]) {
					actionReturn(power_index);
					return;
				}
				if ((locked[i] && !locked[drag_prev_slot]) || (!locked[i] && locked[drag_prev_slot])) {
					locked[i] = !locked[i];
					locked[drag_prev_slot] = !locked[drag_prev_slot];
				}
				hotkeys[drag_prev_slot] = hotkeys[i];
			}
			else if (locked[i] || prevent_changing[i]) return;
			hotkeys[i] = power_index;
			updated = true;
			return;
		}
	}
}

/**
 * Return the power to the last clicked on slot
 */
void MenuActionBar::actionReturn(PowerID power_index) {
	drop(last_mouse, power_index, !REORDER);
}

/**
 * CTRL-click a hotkey to clear it
 */
void MenuActionBar::remove(const Point& mouse) {
	for (unsigned i=0; i<slots_count; i++) {
		if (slots[i] && Utils::isWithinRect(slots[i]->pos, mouse)) {
			if (locked[i]) return;
			hotkeys[i] = 0;
			updated = true;
			return;
		}
	}
}
/**
 * Checks and processes action bar inputs (keyboard, mouse, touch) and adds valid actions to the queue
 * @param action_queue Vector to store triggered actions
 */
void MenuActionBar::checkAction(std::vector<ActionData>& action_queue) {
    // Configure input state based on settings
    bool enable_mouse_move = (!settings->mouse_move || inpt->pressing[Input::SHIFT] || inpt->usingTouchscreen());
    bool enable_main1 = (!inpt->usingTouchscreen() || (!menu->menus_open && menu->touch_controls->checkAllowMain1())) 
                       && (settings->mouse_move_swap || enable_mouse_move);
    bool enable_main2 = !settings->mouse_move_swap || enable_mouse_move;

    // Handle mouse movement targeting
    unsigned mouse_move_slot = settings->mouse_move_swap ? 11 : 10;
    bool has_mouse_move_target = checkMouseMoveTarget(mouse_move_slot);

    // Process each action bar slot
    for (unsigned i = 0; i < slots_count; i++) {
        if (!slots[i]) continue;

        ActionData action;
        action.hotkey = i;
        bool have_aim = false;
        slot_activated[i] = false;

        // Check if action was triggered
        if (!checkActionTrigger(i, mouse_move_slot, has_mouse_move_target, action, have_aim)) {
            clearQueuedAction(i, action_queue);
            continue;
        }

        // Validate and queue the action
        if (powers->isValid(action.power)) {
            processValidAction(i, action, have_aim, action_queue);
        }
    }
}

/**
 * Checks if there is a valid mouse move target and updates player state
 */
bool MenuActionBar::checkMouseMoveTarget(unsigned mm_slot) {
    if (!settings->mouse_move) return false;

    bool has_target = pc->mm_target_object == Avatar::MM_TARGET_ENTITY &&
                     powers->checkCombatRange(powers->checkReplaceByEffect(hotkeys_mod[mm_slot], &pc->stats), 
                                           &pc->stats, pc->mm_target_object_pos) &&
                     mapr->collider.lineOfSight(pc->stats.pos.x, pc->stats.pos.y, 
                                              pc->mm_target_object_pos.x, pc->mm_target_object_pos.y);

    // Update player state based on target
    if (has_target && pc->stats.cur_state == StatBlock::ENTITY_MOVE) {
        pc->stats.cur_state = StatBlock::ENTITY_STANCE;
    }
    else if (!has_target && pc->mm_target_object == Avatar::MM_TARGET_ENTITY && 
             pc->stats.cur_state == StatBlock::ENTITY_STANCE) {
        pc->stats.cur_state = StatBlock::ENTITY_MOVE;
    }

    return has_target;
}

/**
 * Checks various input methods to determine if an action was triggered
 */
bool MenuActionBar::checkActionTrigger(unsigned slot_index, unsigned mm_slot, bool has_mouse_target,
                                     ActionData& action, bool& have_aim) {
    // Mouse move targeting
    if (slot_index == mm_slot && has_mouse_target) {
        action.power = hotkeys_mod[slot_index];
        have_aim = true;
        return true;
    }

    // Two-step activation (for targeted powers)
    if (static_cast<unsigned>(twostep_slot) == slot_index && 
        inpt->pressing[Input::MAIN1] && !inpt->lock[Input::MAIN1]) {
        action.power = hotkeys_mod[slot_index];
        have_aim = true;
        twostep_slot = -1;
        inpt->lock[Input::MAIN1] = true;
        return true;
    }

    // Touch/mouse click
    if ((inpt->mode == InputState::MODE_TOUCHSCREEN && touch_slot == slots[slot_index]) ||
        (inpt->mode != InputState::MODE_TOUCHSCREEN && inpt->usingMouse() && 
         slots[slot_index]->checkClick() == WidgetSlot::ACTIVATE)) {
        return handleClickActivation(slot_index, action);
    }

    // Joystick/keyboard
    if (!inpt->usingMouse() && slots[slot_index]->checkClick() == WidgetSlot::ACTIVATE) {
        action.power = hotkeys_mod[slot_index];
        slot_activated[slot_index] = true;
        twostep_slot = -1;
        return true;
    }

    // Hotkey press
    return checkHotkeyPress(slot_index, action, have_aim);
}

bool MenuActionBar::checkHotkeyPress(unsigned slot_index, ActionData& action, bool& have_aim) {
    bool pressed = false;

    // Number bar (0-9)
    if (slot_index < 10) {
        pressed = inpt->pressing[slot_index + Input::BAR_1];
    }
    // Main1 slot
    else if (slot_index == 10) {
        bool enable_main1 = !settings->mouse_move_swap || settings->mouse_move;
        // Only trigger power if mouse movement is disabled or SHIFT is held
        pressed = inpt->pressing[Input::MAIN1] && 
                 !inpt->lock[Input::MAIN1] && 
                 !Utils::isWithinRect(window_area, inpt->mouse) && 
                 (!settings->mouse_move || inpt->pressing[Input::SHIFT]) &&
                 enable_main1;
    }
    // Main2 slot
    else if (slot_index == 11) {
        bool enable_main2 = !settings->mouse_move_swap || settings->mouse_move;
        pressed = inpt->pressing[Input::MAIN2] && 
                 !inpt->lock[Input::MAIN2] && 
                 !Utils::isWithinRect(window_area, inpt->mouse) && 
                 (!settings->mouse_move || inpt->pressing[Input::SHIFT]) &&
                 enable_main2;
    }

    if (pressed) {
        have_aim = inpt->usingMouse();
        action.power = hotkeys_mod[slot_index];
        twostep_slot = -1;
        return true;
    }

    return false;
}

/**
 * Handles click-based power activation and two-step targeting
 */
bool MenuActionBar::handleClickActivation(unsigned slot_index, ActionData& action) {
    touch_slot = nullptr;
    slot_activated[slot_index] = true;
    action.power = hotkeys_mod[slot_index];

    if (!powers->isValid(action.power)) return true;

    const Power* power = powers->powers[action.power];
    if (power->starting_pos == Power::STARTING_POS_TARGET || power->buff_teleport) {
        twostep_slot = slots[slot_index]->enabled ? slot_index : -1;
        action.power = 0;
    }
    else {
        twostep_slot = -1;
    }

    return true;
}

/**
 * Processes a valid action, checking resources and adding to queue if possible
 */
void MenuActionBar::processValidAction(unsigned slot_index, ActionData& action, bool have_aim,
                                     std::vector<ActionData>& action_queue) {
    const Power* power = powers->powers[action.power];

    // Check resource requirements
    if (!checkResourceRequirements(slot_index, power)) return;

    // Check combat turn state
    if (pc->stats.in_combat && combat_manager) {
        // Don't queue actions if it's not player's turn
        if (!combat_manager->isPlayerTurn()) {
            return;
        }

        // Don't queue if we're out of actions
        if (!combat_manager->canTakeAction()) {
            return;
        }

        // Count queued actions
        int queued_actions = 0;
        for (const ActionData& queued : action_queue) {
            if (!queued.activated_from_inventory) {
                queued_actions++;
            }
        }

        // Don't queue more actions than we have remaining
        if (queued_actions >= combat_manager->getTurnState().actions_remaining) {
            return;
        }
    }

    // Set cooldown
    slot_fail_cooldown[slot_index] = pc->power_cast_timers[action.power]->getDuration();

    // Setup action properties
    setupActionProperties(action, power);

    // Check if we have a valid mouse move target
    bool has_mouse_move_target = checkMouseMoveTarget(settings->mouse_move_swap ? 11 : 10);

    // Set target position
    setActionTarget(action, power, have_aim, has_mouse_move_target);

    // Check if power can be used and add to queue
	if (canUsePower(slot_index, power, action)) {
        if (slot_index != settings->mouse_move_swap ? 11 : 10 && !action.instant_item) {
            pc->mm_target_object = Avatar::MM_TARGET_NONE;
        }
        action_queue.push_back(action);
    }
}

void MenuActionBar::setupActionProperties(ActionData& action, const Power* power) {
    action.instant_item = false;
    if (power->new_state == Power::STATE_INSTANT) {
        for (size_t j = 0; j < power->required_items.size(); ++j) {
            if (power->required_items[j].id > 0 && !power->required_items[j].equipped) {
                action.instant_item = true;
                break;
            }
        }
    }
}


/**
 * Checks if the power can be used
 */
bool MenuActionBar::canUsePower(unsigned slot_index, const Power* power, const ActionData& action) {
    return slots[slot_index]->enabled && 
           (power->new_state == Power::STATE_INSTANT || 
            (pc->stats.cooldown.isEnd() && 
             pc->stats.cur_state != StatBlock::ENTITY_POWER && 
             pc->stats.cur_state != StatBlock::ENTITY_HIT)) &&
           powers->hasValidTarget(action.power, &pc->stats, action.target);
}

/**
 * Sets the target position for the action
 */
void MenuActionBar::setActionTarget(ActionData& action, const Power* power, bool have_aim, bool has_mouse_move_target) {
    // set the target depending on how the power was triggered
    if (have_aim && settings->mouse_aim && !inpt->usingTouchscreen()) {
        action.target = pc->stats.pos;

        if (power->target_nearest > 0) {
            if (!power->requires_corpse && powers->checkNearestTargeting(power, &pc->stats, false)) {
                action.target = pc->stats.target_nearest->pos;
            }
            else if (power->requires_corpse && powers->checkNearestTargeting(power, &pc->stats, true)) {
                action.target = pc->stats.target_nearest_corpse->pos;
            }
        }
        else if (has_mouse_move_target) {
            action.target = pc->mm_target_object_pos;
        }
        else {
            if (power->aim_assist)
                action.target = Utils::screenToMap(inpt->mouse.x,  inpt->mouse.y + eset->misc.aim_assist, 
                                                 mapr->cam.pos.x, mapr->cam.pos.y);
            else
                action.target = Utils::screenToMap(inpt->mouse.x,  inpt->mouse.y, 
                                                 mapr->cam.pos.x, mapr->cam.pos.y);
        }
    }
    else {
        action.target = Utils::calcVector(pc->stats.pos, pc->stats.direction, pc->stats.melee_range);
    }
}

/**
 * Checks if the player has enough resources to use the power
 */
bool MenuActionBar::checkResourceRequirements(unsigned slot_index, const Power* power) {
    if (slot_fail_cooldown[slot_index] > 0) return false;

    bool has_resources = true;
    
    if (pc->stats.mp < power->requires_mp) {
        pc->logMsg(msg->get("Not enough MP."), Avatar::MSG_NORMAL);
        has_resources = false;
    }

    for (size_t i = 0; i < eset->resource_stats.list.size(); ++i) {
        if (pc->stats.resource_stats[i] < power->requires_resource_stat[i]) {
            pc->logMsg(eset->resource_stats.list[i].text_log_low, Avatar::MSG_NORMAL);
            has_resources = false;
        }
    }

    if (!has_resources) {
        slot_fail_cooldown[slot_index] = settings->max_frames_per_sec;
        snd->play(sfx_unable_to_cast, "ACT_NO_MP", snd->NO_POS, !snd->LOOP);
    }

    return has_resources;
}

/**
 * Removes an action from the queue if it's no longer valid
 */
void MenuActionBar::clearQueuedAction(unsigned slot_index, std::vector<ActionData>& action_queue) {
    for (size_t i = action_queue.size(); i > 0; --i) {
        if (!action_queue[i-1].activated_from_inventory && action_queue[i-1].hotkey == slot_index) {
            action_queue.erase(action_queue.begin() + (i-1));
        }
    }
}

/**
 * If clicking while a menu is open, assume the player wants to rearrange the action bar
 */
PowerID MenuActionBar::checkDrag(const Point& mouse) {
	PowerID power_index;

	for (unsigned i=0; i<slots_count; i++) {
		if (slots[i] && Utils::isWithinRect(slots[i]->pos, mouse)) {
			if (prevent_changing[i])
				return 0;

			drag_prev_slot = i;
			power_index = hotkeys[i];
			hotkeys[i] = 0;
			last_mouse = mouse;
			updated = true;
			twostep_slot = -1;
			return power_index;
		}
	}

	return 0;
}

/**
 * if clicking a menu, act as if the player pressed that menu's hotkey
 */
void MenuActionBar::checkMenu(bool &menu_c, bool &menu_i, bool &menu_p, bool &menu_l) {
	if (menus[MENU_CHARACTER]->checkClick()) {
		menu_c = true;
		menus[MENU_CHARACTER]->deactivate();
		defocusTabLists();
	}
	else if (menus[MENU_INVENTORY]->checkClick()) {
		menu_i = true;
		menus[MENU_INVENTORY]->deactivate();
		defocusTabLists();
	}
	else if (menus[MENU_POWERS]->checkClick()) {
		menu_p = true;
		menus[MENU_POWERS]->deactivate();
		defocusTabLists();
	}
	else if (menus[MENU_LOG]->checkClick()) {
		menu_l = true;
		menus[MENU_LOG]->deactivate();
		defocusTabLists();
	}
}

/**
 * Set all hotkeys at once e.g. when loading a game
 */
void MenuActionBar::set(std::vector<PowerID> power_id, bool skip_empty) {
	for (unsigned i = 0; i < slots_count; i++) {
		if (!powers->isValid(power_id[i]))
			continue;

		if (!powers->powers[power_id[i]] || powers->powers[power_id[i]]->no_actionbar)
			continue;

		if (!skip_empty || hotkeys[i] == 0)
			hotkeys[i] = power_id[i];
	}
	updated = true;
}

void MenuActionBar::setItemCount(unsigned index, int count, bool is_equipped) {
	if (index >= slots_count || !slots[index]) return;

	slot_item_count[index] = count;
	if (count == 0) {
		if (slot_activated[index])
			slots[index]->deactivate();

		slots[index]->enabled = false;
	}

	if (is_equipped)
		// we don't care how many of an equipped item we're carrying
		slots[index]->setAmount(count, 0);
	else if (count >= 0)
		// we can always carry more than 1 of any item, so always display non-equipped item count
		slots[index]->setAmount(count, 2);
	else
		// slot contains a regular power, so ignore item count
		slots[index]->setAmount(0,0);
}

bool MenuActionBar::isWithinSlots(const Point& mouse) {
	for (unsigned i=0; i<slots_count; i++) {
		if (slots[i] && Utils::isWithinRect(slots[i]->pos, mouse))
			return true;
	}
	return false;
}

bool MenuActionBar::isWithinMenus(const Point& mouse) {
	for (unsigned i=0; i<MENU_COUNT; i++) {
		if (Utils::isWithinRect(menus[i]->pos, mouse))
			return true;
	}
	return false;
}

/**
 * Replaces the power(s) in slots that match the target_id with the power of id
 * So a target_id of 0 will place the power in an empty slot, if available
 */
void MenuActionBar::addPower(const PowerID id, const PowerID target_id) {
	if (!powers->isValid(id))
		return;

	// some powers are explicitly prevented from being placed on the actionbar
	if (powers->powers[id]->no_actionbar)
		return;

	// can't put passive powers on the action bar
	if (powers->powers[id]->passive)
		return;

	// if we're not replacing an existing power, avoid placing duplicate powers
	if (target_id == 0) {
		for (unsigned i = 0; i < static_cast<unsigned>(SLOT_MAX); ++i) {
			if (hotkeys[i] == id)
				return;
		}
	}

	// MAIN slots have priority
	for (unsigned i=10; i<12; ++i) {
		if (hotkeys[i] == target_id) {
			if (target_id == 0 && prevent_changing[i]) {
				continue;
			}
			hotkeys[i] = id;
			updated = true;
			if (target_id == 0)
				return;
		}
	}

	// now try 0-9 slots
	for (unsigned i=0; i<10; ++i) {
		if (hotkeys[i] == target_id) {
			if (target_id == 0 && prevent_changing[i]) {
				continue;
			}
			hotkeys[i] = id;
			updated = true;
			if (target_id == 0)
				return;
		}
	}
}

Point MenuActionBar::getSlotPos(int slot) {
	if (static_cast<unsigned>(slot) < slots.size()) {
		return Point(slots[slot]->pos.x, slots[slot]->pos.y);
	}
	else if (static_cast<unsigned>(slot) < slots.size() + MENU_COUNT) {
		int slot_size = static_cast<int>(slots.size());
		return Point(menus[slot - slot_size]->pos.x, menus[slot - slot_size]->pos.y);
	}
	return Point();
}

WidgetSlot* MenuActionBar::getSlotFromPosition(const Point& position) {
	for (size_t i = 0; i < slots.size(); ++i) {
		if (slots[i] && Utils::isWithinRect(slots[i]->pos, position))
			return slots[i];
	}
	return NULL;
}

MenuActionBar::~MenuActionBar() {
	menu_act = NULL;
	delete sprite_emptyslot;
	delete end_turn_button;

	labels.clear();
	menu_labels.clear();

	for (unsigned i = 0; i < slots_count; i++)
		delete slots[i];

	for (unsigned i=0; i<MENU_COUNT; i++)
		delete menus[i];

	snd->unload(sfx_unable_to_cast);
}
