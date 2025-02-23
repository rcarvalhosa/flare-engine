/*
Copyright © 2011-2012 Clint Bellanger
Copyright © 2012 Igor Paliychuk
Copyright © 2013 Henrik Andersson
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
 * class Avatar
 *
 * Contains logic and rendering routines for the player avatar.
 */

#ifndef AVATAR_H
#define AVATAR_H

#include "CommonIncludes.h"
#include "Entity.h"
#include "Utils.h"
#include <memory>
#include "PowerManager.h"

class Entity;
class StatBlock;

class ActionData {
public:
	PowerID power;
	unsigned hotkey;
	bool instant_item;
	bool activated_from_inventory;
	FPoint target;

	ActionData()
		: power(0)
		, hotkey(0)
		, instant_item(false)
		, activated_from_inventory(false)
		, target(FPoint()) {
	}
};

class Avatar : public Entity {
private:
	class Step_sfx {
	public:
		std::string id;
		std::vector<std::string> steps;
	};

	static const int PATH_FOUND_FAIL_THRESHOLD = 1;
	static const int PATH_FOUND_FAIL_WAIT_SECONDS = 2;

	void loadLayerDefinitions();
	bool pressing_move();
	void set_direction();
	void transform();
	void untransform();
	void beginPower(PowerID power_id, FPoint* target);
	
	void initializeBasicStats();
	void initializePosition();
	void initializePowerState();
	void initializeTransformState();
	void initializePowers();
	
	void loadStepSoundDefinitions();

	void handleMouseMoveDirection();
	void handleKeyboardDirection();
	void updateDirectionTimer(int old_dir);
	bool isValidCombatMove(const FPoint& target);
	void handlePathfinding();
	void recalculatePath();
	bool shouldRecalculatePath();
	void updatePathTarget();

	void handlePowerRestrictions();
	void handleBasicState();
	void handleLowHealthEffects();
	void handleLevelUp();
	void handleMouseMovement();
	void handleAnimations();
	void handleTransformState();
	void handleStateChanges();
	void handleCameraAndCooldowns();
	void resetBlockState();
	void handleLowHpSound();
	void updateLowHpSound();
	bool shouldLevelUp();
	void performLevelUp();
	void handleMouseTargeting();
	void updateMouseDistance();
	void handleMouseLock();
	void updateLockedEnemy();
	void handleActionQueue();
	void handleCurrentState();
	void handleReplacedPower(PowerID replaced_id, const ActionData& action);
	void handleInstantPower(PowerID replaced_id, const ActionData& action);
	void handleBlockingPower(PowerID replaced_id, const ActionData& action, Power* power);
	void handleStanceMovePower(PowerID replaced_id, const ActionData& action, Power* power);
	void handleStanceState();
	void handleMoveState();
	void handlePowerState();
	void handleBlockState();
	void handleHitState();
	void handleDeadState();

	void initializeBlockingState(const Power* power);
	void handleMeleeTargeting(const Power* power, FPoint* target);
	void updateDirectionAndTimers(const Power* power, const FPoint* target);
	void activateChainPowers(const Power* power);

	void handlePowerCursor(const Power* power);
	void initializePowerExecution(const Power* power);
	void executePower(const Power* power);
	void checkStateTransition();

	std::vector<Step_sfx> step_def;

	std::vector<SoundID> sound_steps;

	short body;

	bool transform_triggered;
	std::string last_transform;


	int mm_key; // mouse movement key
	bool mm_is_distant;

	Timer set_dir_timer;

	//variables for patfinding
	std::vector<FPoint> path;
	FPoint prev_target;
	bool collided;
	bool path_found;
	int chance_calc_path;
	int path_found_fails;
	Timer path_found_fail_timer;

	bool restrict_power_use = false;

	FPoint mm_target;
	FPoint mm_target_desired;

	bool isDroppedToLowHp();

	std::vector<PowerID> power_cooldown_ids;

	bool was_in_combat = false; // Tracks if avatar was previously in combat

public:
	enum {
		MSG_NORMAL = 0,
		MSG_UNIQUE = 1
	};

	enum {
		MM_TARGET_NONE = 0,
		MM_TARGET_EVENT,
		MM_TARGET_LOOT,
		MM_TARGET_ENTITY,
	};

	Avatar();
	~Avatar();

	void init();
	void handleNewMap();
	void loadGraphics(std::vector<Entity::Layer_gfx> _img_gfx);
	void loadStepFX(const std::string& stepname);

	void logic();
	bool move() override;
	void checkTransform();

	void logMsg(const std::string& str, int type);

	bool isLowHp();
	bool isLowHpMessageEnabled();
	bool isLowHpSoundEnabled();
	bool isLowHpCursorEnabled();

	std::string getGfxFromType(const std::string& gfx_type) override;

	std::vector<FPoint>& getPath() { return path; }
	FPoint& getMMTarget() { return mm_target; };
	bool isNearMMtarget();
	void setDesiredMMTarget(FPoint& target);

	std::queue<std::pair<std::string, int> > log_msg;

	std::string attack_anim;
	bool setPowers;
	bool revertPowers;
	PowerID untransform_power;
	StatBlock *hero_stats;
	StatBlock *charmed_stats;
	FPoint transform_pos;
	std::string transform_map;

	// vars
	PowerID current_power;
	PowerID current_power_original;
	FPoint act_target;
	bool drag_walking;
	bool newLevelNotification;
	bool respawn;
	bool close_menus;
	bool allow_movement;
	std::vector<std::unique_ptr<Timer>> power_cooldown_timers;
	std::vector<std::unique_ptr<Timer>> power_cast_timers;
	Entity* cursor_enemy; // enemy selected with the mouse cursor
	Entity* lock_enemy;
	unsigned long time_played;
	bool questlog_dismissed;
	bool using_main1;
	bool using_main2;
	float prev_hp;
	bool playing_lowhp;
	bool teleport_camera_lock;
	int feet_index;
	int mm_target_object;
	FPoint mm_target_object_pos;

	std::vector<ActionData> action_queue;
};

#endif

