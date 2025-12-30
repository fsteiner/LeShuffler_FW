/*
 * games.h
 *
 *  Created on: Aug 20, 2025
 *      Author: fs
 */

#ifndef INC_GAMES_H_
#define INC_GAMES_H_

#include <stdint.h>
#include <stdbool.h>
#include <basic_operations.h>

//distribution_mode_t
typedef enum
{
	DIST_MODE_BY_PLAYER = 0, // Distribuer toutes les cartes d'un joueur avant le suivant
	DIST_MODE_ROUND_ROBIN = 1 // Distribuer à tour de rôle (une carte par joueur)
} distribution_mode_t;

// deal_destination_t
typedef enum
{
	DEST_INTERNAL_TRAY = 0,     // Distribuer dans le tiroir interne
	DEST_TABLE = 1       // Distribuer directement sur la table
} deal_destination_t;

//recycle_mode_t
typedef enum
{
	RECYCLE_OFF = 0,          // Pas de recyclage automatique
	RECYCLE_ON = 1,     		// Recyclage possible à tout moment
} recycle_mode_t;

// community_timing_t
typedef enum
{
	COMMUNITY_TIMING_AFTER_HOLES = 0, // Community cards après les hole cards (standard)
	COMMUNITY_TIMING_BEFORE_HOLES = 1 // Community cards avant les hole cards
} community_timing_t;

// stage_code_t
typedef enum
{
	NO_GAME_RUNNING,
	LOADING,
	RANDOM_PLAYER,
	HOLE_CARDS,
	CC_1,
	CC_2,
	CC_3,
	CC_4,
	CC_5,
	PICK,
	RECYCLE,
	DEALING_N_CARDS,
	EMPTYING,
	SAFE_EMPTYING,
	SHUFFLING,
	END_GAME,
} stage_code_t;

// game_state_t
union game_state_t
{
	struct
	{
		item_code_t game_code :7;
		uint16_t n_players :4;              // Number of players (0-15)
		stage_code_t current_stage :4;      // Current stage (used 0-11/15)
		uint16_t reserved_1 :1;

		uint16_t n_cards_dealt :6; // Cards fully dealt in stage/player (excl. burn and extra cards)
		uint16_t n_players_dealt :4;        // Players fully dealt in stage/card
		uint16_t n_pick :3;                // Number of cards to be picked (0-7)
		uint16_t reserved_2 :3;
	};

	uint8_t bytes[4];
};

// cc_stage_t
typedef struct
{
	char name[STAGE_NAME_MAX_CHAR + 1];			// Stage name
	uint8_t n_cards;    					// Number of cards in stage, max 6 ?
} cc_stage_t;

// game_rules_t
typedef union
{
	struct
	{
		item_code_t game_code :7;
		uint8_t stud_structure :1; 				// true or false

		uint16_t hole_sequence;  				// intervals between pauses coded on 2 bits

		uint8_t n_cc_stages :3;     			// Number of CC stages (0-5 = MAX_CC_STAGES)
		uint8_t random_player :1;				// Is drawing a player at random required?
		uint8_t recycle_mode :1;        		// Recycling mode (0-1)
		uint8_t allows_burn_cards :1;   		// Possibility to burn cards?
		uint8_t allows_xtra_cards :1; 			// Possibility to get extra cards?
		uint8_t reserved_1 :1;

		uint8_t deck_size :6;   				// Deck size (0-63, in fact 54)
		uint8_t reserved_2 :2;

		uint8_t min_players :4;					// Min number of players (0-15)
		uint8_t max_players :4;   				// Max number of players (0-15)

		uint8_t n_hole_cards :5;    			// Number of cards per player (0-31)
		uint8_t max_pick :3; 					// Max number of cards to pick at one time (0 = no picking, 1-7)

		cc_stage_t cc_stages[MAX_CC_STAGES];	// Definition of CC stages
	};

	uint8_t bytes[E_GAME_RULES_SIZE];

} game_rules_t;

// user_prefs_t
typedef union
{
	struct
	{
		// Byte 0
		item_code_t game_code :7;
		uint8_t activated :1;		// Game activated - shows up in menus or not

		// Byte 1
		uint8_t dist_mode :1;           // Mode de distribution des cartes
		uint8_t hole_cards_dest :1;     // Destination des hole cards
		uint8_t community_cards_dest :1;     // Destination des community cards
		uint8_t community_timing :1;    // Timing des community cards
		uint8_t empty_machine_after :1; // Vider la machine après chaque tour
		uint8_t burn_cards :1;          // Burn cards (auto memory set)
		uint8_t reserved_2 :2;

		// Byte 2
		uint8_t default_n_players :4;  // Last number of players (0-15)
		uint8_t reserved_3 :4;
	};

	uint8_t bytes[3];

} user_prefs_t;

// gen_prefs_t
typedef union
{
	struct
	{
		uint8_t deal_gap_index :4;	// 0-15
		uint8_t cut_card_flag :1;   // using cut card for shuffle
		uint8_t reserved :3;
	};

	uint8_t byte;

} gen_prefs_t;

return_code_t reset_content(void);
return_code_t reset_user_prefs(void);
return_code_t reset_custom_games(void);
void prompt_discharged_cards(game_rules_t, user_prefs_t);
void clear_discharged_cards(void);
return_code_t discharge_cards(game_rules_t);
return_code_t deal_hole_cards(game_rules_t, user_prefs_t);
return_code_t shuffle(uint16_t*);
return_code_t get_nx(uint16_t*, uint16_t, uint16_t, const char*);
return_code_t get_n_players(game_rules_t, user_prefs_t);
return_code_t get_n_pick_or_deal(game_rules_t, uint16_t*);
return_code_t get_rules(game_rules_t*, item_code_t);
return_code_t run_game(void);
return_code_t run_stage(game_rules_t, user_prefs_t*);
return_code_t init_game_state(game_rules_t, user_prefs_t);
return_code_t game_load(game_rules_t, user_prefs_t);
return_code_t random_player(uint16_t);
return_code_t read_user_prefs(user_prefs_t*, item_code_t);
return_code_t write_user_prefs(user_prefs_t*, item_code_t);
return_code_t set_user_prefs(item_code_t);
return_code_t adjust_deal_gap(void);
return_code_t set_cut_card_flag(void);
return_code_t set_custom_game_rules(item_code_t);
return_code_t read_custom_game_name(char text[GAME_NAME_MAX_CHAR + 1],
		item_code_t);
return_code_t write_custom_game_name(char text[GAME_NAME_MAX_CHAR + 1],
		item_code_t);
return_code_t deal_n_cards(void);
return_code_t shufflers_choice(void);

#endif /* INC_GAMES_H_ */
