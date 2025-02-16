#ifndef COMBAT_MANAGER_H
#define COMBAT_MANAGER_H

#include "CommonIncludes.h"
#include "Utils.h"

class Entity;
class StatBlock;

/**
 * class CombatManager
 *
 * Handles the global combat state and turn-based combat mechanics.
 * Manages transitions between exploration and combat modes.
 */
class CombatManager {
public:
    enum CombatState {
        COMBAT_INACTIVE,    // Normal exploration mode
        COMBAT_ACTIVE,      // Turn-based combat mode
        COMBAT_TRANSITION   // Visual transition between modes
    };

    enum ActionType {
        ACTION_NONE,
        ACTION_MOVE,
        ACTION_POWER,
        ACTION_ITEM
    };

    struct TurnState {
        ActionType last_action;
        FPoint movement_start;
        int actions_remaining;      // Number of actions remaining this turn
        
        TurnState() 
            : last_action(ACTION_NONE)
            , movement_start(FPoint())
            , actions_remaining(2) {}
            
        void reset() {
            last_action = ACTION_NONE;
            movement_start = FPoint();
            actions_remaining = 2;  // Each entity gets 2 actions per turn
        }
    };

    // Structure to track initiative order
    struct CombatEntity {
        Entity* entity;
        int initiative;
        
        CombatEntity(Entity* e, int i) : entity(e), initiative(i) {}
        
        // Sort by initiative (highest first)
        bool operator<(const CombatEntity& other) const {
            return initiative > other.initiative;
        }
    };

    CombatManager();
    ~CombatManager();

    // Core functions
    void logic();
    void render();

    // Combat state management
    void enterCombat(Entity* initiator, Entity* target);
    void exitCombat();
    bool isInCombat() const { return current_state == COMBAT_ACTIVE; }
    
    // Visual transition handling
    void startTransition();
    bool isTransitioning() const { return current_state == COMBAT_TRANSITION; }

    // Initiative and turn management
    void rollInitiative();
    void addCombatant(Entity* entity);
    Entity* getCurrentTurnEntity() const;
    void nextTurn();
    int getCurrentRound() const { return current_round; }

    // Turn control
    bool isPlayerTurn() const;
    void endPlayerTurn();
    bool canEndTurn() const;

    // Turn flow
    bool canTakeAction() const;
    bool isValidMovement(const FPoint& dest) const;
    void startMovement();
    void completeMovement(const FPoint& dest);
    bool performAction(ActionType action);
    float getMovementRange() const;
    const TurnState& getCurrentTurnState() const { return turn_state; }
    const TurnState& getTurnState() const { return turn_state; }
    TurnState& getTurnState() { return turn_state; }
    void spendAction();

private:
    CombatState current_state;
    std::vector<Entity*> combat_entities;      // All entities in combat
    std::vector<CombatEntity> initiative_order; // Entities sorted by initiative
    size_t current_turn_index;                 // Index of current entity in initiative order
    int current_round;                         // Current combat round number
    Timer transition_timer;                    // For visual transition effects
    TurnState turn_state;                      // Current turn's state

    int rollInitiativeForEntity(Entity* entity) const;
    void resetTurnState();
};

extern CombatManager* combat_manager;

#endif // COMBAT_MANAGER_H 