/*
Copyright © 2011-2012 Clint Bellanger and kitano
Copyright © 2012-2015 Justin Jacobs

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
 * class Entity
 *
 * An Entity represents any character in the game - the player, allies, enemies
 * This base class handles logic common to all of these child classes
 */


#ifndef ENTITY_H
#define ENTITY_H

#include "CommonIncludes.h"
#include "StatBlock.h"
#include "Utils.h"

class Animation;
class AnimationSet;
class EntityBehavior;

class Entity {
protected:
	Image *sprites;

	void move_from_offending_tile();
	void resetActiveAnimation();
	uint8_t getRenderableType();

public:
	class Layer_gfx {
	public:
		std::string gfx;
		std::string type;
	};

	enum {
		SOUND_HIT = 0,
		SOUND_DIE = 1,
		SOUND_CRITDIE = 2,
		SOUND_BLOCK = 3
	};

	Entity();
	Entity(const Entity& e);
	Entity& operator=(const Entity& e);
	virtual ~Entity();

	void logic();
	void loadSounds();
	void loadSoundsFromStatBlock(StatBlock *src_stats);
	void unloadSounds();
	void playAttackSound(const std::string& attack_name);
	void playSound(int sound_type);
	virtual bool move();
	bool takeHit(Hazard &h);

	// sound effects
	std::vector<std::pair<std::string, std::vector<SoundID> > > sound_attack;
	std::vector<SoundID> sound_hit;
	std::vector<SoundID> sound_die;
	std::vector<SoundID> sound_critdie;
	std::vector<SoundID> sound_block;
	SoundID sound_levelup;
	SoundID sound_lowhp;

	void setAnimation(const std::string& animation);
	Animation *activeAnimation;
	AnimationSet *animationSet;
	std::vector<AnimationSet*> animsets; // hold the animations for all equipped items in the right order of drawing.
	std::vector<Animation*> anims; // hold the animations for all equipped items in the right order of drawing.

	StatBlock stats;

	unsigned char faceNextBest(float mapx, float mapy);
	Rect getRenderBounds(const FPoint& cam) const;

	std::string type_filename;

	EntityBehavior *behavior;

	void loadAnimations();
	virtual std::string getGfxFromType(const std::string& gfx_type);
	void addRenders(std::vector<Renderable> &r);
};

extern const int directionDeltaX[];
extern const int directionDeltaY[];
extern const float speedMultiplyer[];

#endif

