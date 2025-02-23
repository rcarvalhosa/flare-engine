/*
Copyright © 2024 Flare Team
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

#include "CombatManager.h"
#include "Entity.h"
#include "EntityManager.h"
#include "MapRenderer.h"
#include "Menu.h"
#include "MenuManager.h"
#include "MenuHUDLog.h"
#include "MessageEngine.h"
#include "SharedGameResources.h"
#include "SharedResources.h"
#include "StatBlock.h"
#include "Settings.h"
#include <algorithm>
#include "UtilsMath.h"

CombatManager* combat_manager = NULL;

CombatManager::CombatManager()
    : current_state(COMBAT_INACTIVE)
    , current_turn_index(0)
    , current_round(0)
    , transition_timer(settings->max_frames_per_sec) {
}

CombatManager::~CombatManager() {
}

void CombatManager::logic() {
    if (current_state == COMBAT_TRANSITION) {
        transition_timer.tick();
        if (transition_timer.isEnd()) {
            current_state = COMBAT_ACTIVE;
            // Roll initiative when transitioning into active combat
            rollInitiative();
            // Reset turn state for first turn
            resetTurnState();
            // Log the start of the first round
            if (menu && menu->hudlog) {
                menu->hudlog->add(msg->get("Round 1 begins!"), MenuHUDLog::MSG_NORMAL);
            }
            // Announce first turn
            Entity* first = getCurrentTurnEntity();
            if (first && menu && menu->hudlog) {
                std::string name = first->stats.hero ? msg->get("Your") : first->stats.name + "'s";
                menu->hudlog->add(name + msg->get(" turn"), MenuHUDLog::MSG_NORMAL);
            }
        }
    }

    // Update combat entities list - remove dead entities
    if (current_state == COMBAT_ACTIVE) {
        combat_entities.erase(
            std::remove_if(
                combat_entities.begin(),
                combat_entities.end(),
                [](Entity* e) { return !e->stats.alive; }
            ),
            combat_entities.end()
        );

        // If no enemies remain, exit combat
        bool has_enemies = false;
        for (Entity* e : combat_entities) {
            if (!e->stats.hero && !e->stats.hero_ally) {
                has_enemies = true;
                break;
            }
        }

        if (!has_enemies) {
            exitCombat();
        }
        // Handle non-player turns automatically
        if (!isPlayerTurn()) {
            Entity* current = getCurrentTurnEntity();
            if (current) {
                // Let the AI take its turn, respecting action limits
                if (turn_state.actions_remaining > 0) {
                    current->logic();
                }
                // End turn after AI has used its actions or can't act
                if (turn_state.actions_remaining == 0) {
                    nextTurn();
                }
            }
        }
    }
}

void CombatManager::render() {
    // Visual transition effects will be implemented here
}

void CombatManager::enterCombat(Entity* initiator, Entity* target) {
    if (current_state != COMBAT_INACTIVE) return;

    // Reset combat state
    current_round = 1;
    current_turn_index = 0;
    combat_entities.clear();
    initiative_order.clear();

    // Add initial combatants
    combat_entities.push_back(initiator);
    combat_entities.push_back(target);

    // Check for other enemies in range that should join combat immediately
    for (size_t i = 0; i < entitym->entities.size(); ++i) {
        Entity* entity = entitym->entities[i];
        if (entity == initiator || entity == target) continue;
        if (!entity->stats.alive || entity->stats.hero_ally) continue;

        float dist = Utils::calcDist(target->stats.pos, entity->stats.pos);
        if (dist <= entity->stats.threat_range && 
            mapr->collider.lineOfSight(entity->stats.pos.x, entity->stats.pos.y, target->stats.pos.x, target->stats.pos.y)) {
            combat_entities.push_back(entity);
            entity->stats.in_combat = true;
        }
    }

    // Set combat flags for all initial combatants
    for (Entity* e : combat_entities) {
        e->stats.in_combat = true;
    }

    // Start transition
    startTransition();

    // Log combat start
    if (menu && menu->hudlog) {
        menu->hudlog->add(msg->get("Combat started!"), MenuHUDLog::MSG_NORMAL);
    }
}

void CombatManager::exitCombat() {
    if (current_state == COMBAT_INACTIVE) return;

    // Clear combat flags for all entities
    for (Entity* e : combat_entities) {
        e->stats.in_combat = false;
    }

    // Reset state
    combat_entities.clear();
    initiative_order.clear();
    current_state = COMBAT_INACTIVE;
    current_round = 0;
    current_turn_index = 0;

    // Log combat end
    if (menu && menu->hudlog) {
        menu->hudlog->add(msg->get("Combat ended."), MenuHUDLog::MSG_NORMAL);
    }
}

void CombatManager::startTransition() {
    current_state = COMBAT_TRANSITION;
    transition_timer.reset(Timer::BEGIN);
}

void CombatManager::rollInitiative() {
    initiative_order.clear();

    // Roll initiative for each entity in combat
    for (Entity* entity : combat_entities) {
        int initiative = rollInitiativeForEntity(entity);
        initiative_order.emplace_back(entity, initiative);
    }

    // Sort by initiative (highest first)
    std::sort(initiative_order.begin(), initiative_order.end());

    // Log initiative order
    if (menu && menu->hudlog) {
        menu->hudlog->add(msg->get("Initiative order:"), MenuHUDLog::MSG_NORMAL);
        for (const CombatEntity& ce : initiative_order) {
            std::string name = ce.entity->stats.hero ? msg->get("You") : ce.entity->stats.name;
            menu->hudlog->add(name + ": " + std::to_string(ce.initiative), MenuHUDLog::MSG_NORMAL);
        }
    }
}

int CombatManager::rollInitiativeForEntity(Entity* entity) const {
    // Base initiative is a random number between 1-20
    int initiative = Math::randBetween(1, 20);
    
    // Add modifiers based on entity stats
    // For now, we'll use speed as a simple initiative modifier
    initiative += static_cast<int>(entity->stats.speed);
    
    return initiative;
}

void CombatManager::addCombatant(Entity* entity) {
    if (!entity || current_state == COMBAT_INACTIVE) return;

    // Check if entity is already in combat
    if (std::find(combat_entities.begin(), combat_entities.end(), entity) != combat_entities.end())
        return;

    // Add to combat entities
    combat_entities.push_back(entity);
    entity->stats.in_combat = true;

    // Roll initiative for new combatant
    int initiative = rollInitiativeForEntity(entity);
    initiative_order.emplace_back(entity, initiative);

    // Re-sort initiative order
    std::sort(initiative_order.begin(), initiative_order.end());

    // Log new combatant
    if (menu && menu->hudlog) {
        menu->hudlog->add(msg->get(entity->stats.name + " joins the battle!"), MenuHUDLog::MSG_NORMAL);
    }
}

Entity* CombatManager::getCurrentTurnEntity() const {
    if (initiative_order.empty() || current_turn_index >= initiative_order.size())
        return nullptr;
    return initiative_order[current_turn_index].entity;
}

void CombatManager::nextTurn() {
    // Log turn end
    Entity* current = getCurrentTurnEntity();
    if (current && menu && menu->hudlog) {
        std::string name = current->stats.hero ? msg->get("Your") : current->stats.name + "'s";
        menu->hudlog->add(name + msg->get(" turn ends."), MenuHUDLog::MSG_NORMAL);
    }

    // Move to next entity in initiative order
    current_turn_index = (current_turn_index + 1) % initiative_order.size();

    // If we've completed a round, increment round counter
    if (current_turn_index == 0) {
        current_round++;
        if (menu && menu->hudlog) {
            menu->hudlog->add(msg->get("Round ") + std::to_string(current_round) + msg->get(" begins!"), MenuHUDLog::MSG_NORMAL);
        }
    }

    // Reset turn state for new turn
    resetTurnState();

    // Log whose turn it is
    current = getCurrentTurnEntity();
    if (current && menu && menu->hudlog) {
        std::string name = current->stats.hero ? msg->get("Your") : current->stats.name + "'s";
        menu->hudlog->add(name + msg->get(" turn"), MenuHUDLog::MSG_NORMAL);
    }
}

bool CombatManager::isPlayerTurn() const {
    Entity* current = getCurrentTurnEntity();
    return current && current->stats.hero;
}

void CombatManager::endPlayerTurn() {
    if (!isPlayerTurn())
        return;

    // Log turn end
    if (menu && menu->hudlog) {
        menu->hudlog->add(msg->get("You end your turn."), MenuHUDLog::MSG_NORMAL);
    }

    // Move to next turn
    nextTurn();
}

bool CombatManager::canEndTurn() const {
    return current_state == COMBAT_ACTIVE && isPlayerTurn();
}

void CombatManager::resetTurnState() {
    turn_state.reset();
    Entity* current = getCurrentTurnEntity();
    if (current) {
        turn_state.movement_start = current->stats.pos;
        turn_state.actions_remaining = 2;  // Reset to 2 actions per turn
        turn_state.last_action = ACTION_NONE;
    }
}

bool CombatManager::canTakeAction() const {
    return current_state == COMBAT_ACTIVE && 
           turn_state.actions_remaining > 0 &&
           getCurrentTurnEntity() != nullptr;
}

bool CombatManager::isValidMovement(const FPoint& dest) const {
    if (!canTakeAction()) return false;

    Entity* current = getCurrentTurnEntity();
    if (!current) return false;

    // Check if destination is within movement range from turn start position
    float dist = Utils::calcDist(current->stats.pos, dest);
    if (dist > getMovementRange()) {
        return false;
    }

    // Check if path is clear (no obstacles or other entities)
    if (!mapr->collider.lineOfMovement(
        turn_state.movement_start.x,
        turn_state.movement_start.y,
        dest.x,
        dest.y,
        current->stats.movement_type))
    {
        return false;
    }

    // Check if destination tile is valid for this entity's movement type
    if (!mapr->collider.isValidPosition(dest.x, dest.y, 
        current->stats.movement_type, 
        mapr->collider.getCollideType(current->stats.hero)))
    {
        return false;
    }

    return true;
}



void CombatManager::spendAction() {
    turn_state.actions_remaining--;
   if (turn_state.actions_remaining <= 0) {
        nextTurn();
   }
}

float CombatManager::getMovementRange() const {
    Entity* current = getCurrentTurnEntity();
    if (!current) return 0.0f;

    // TODO: Base movement range is 6 tiles, will be based on Race later. Should probably be a stat.
    return 6.0f;
} 