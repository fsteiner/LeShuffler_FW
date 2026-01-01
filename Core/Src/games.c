//=============================================================================
// Fonctions de jeux
//=============================================================================

#include <basic_operations.h>
#include <buttons.h>
#include <games.h>
#include <interface.h>
#include <ili9488.h>
#include <rng.h>
#include <servo_motor.h>
#include <string.h>
#include <TMC2209.h>
#include <utilities.h>

extern char display_buf[];

return_code_t get_n_x(uint16_t*, uint16_t, uint16_t, const char*, const char*);

// Global variables game_state and machine state
union game_state_t game_state;
union machine_state_t machine_state;

return_code_t valid_user_inputs[] =
{ LS_OK, LS_ESC, BURN_CARD, DRAW_CARD, EXTRA_CARD };

const uint16_t n_valid_user_inputs = sizeof(valid_user_inputs)
		/ sizeof(return_code_t);

const cc_stage_t flop_stage =
{ .name = "Flop", .n_cards = 3 };

const cc_stage_t turn_stage =
{ .name = "Turn", .n_cards = 1 };

const cc_stage_t river_stage =
{ .name = "River", .n_cards = 1 };

const cc_stage_t _3st_stage =
{ .name = "3rd Street", .n_cards = 1 };

const cc_stage_t _4st_stage =
{ .name = "4th Street", .n_cards = 1 };

const cc_stage_t _5st_stage =
{ .name = "5th Street", .n_cards = 1 };

const cc_stage_t cut_card_stage =
{ .name = "Cut Card", .n_cards = 1 };

const cc_stage_t flop_x2_stage =
{ .name = "Flop x 2", .n_cards = 6 };

const cc_stage_t turn_x2_stage =
{ .name = "Turn x 2", .n_cards = 2 };

const cc_stage_t river_x2_stage =
{ .name = "River x 2", .n_cards = 2 };

const cc_stage_t landlord_stage =
{ .name = "Landlord cards", .n_cards = 3 };

const cc_stage_t dealer_stage =
{ .name = "Dealer card", .n_cards = 1 };

const cc_stage_t starter_stage =
{ .name = "Starter card", .n_cards = 1 };

const cc_stage_t _2_stage =
{ .name = "Community cards", .n_cards = 2 };

// Void rules and preferences
game_rules_t void_game_rules =
{ .game_code = 0 };
user_prefs_t void_user_prefs =
{ .game_code = 0 };

// Preset and custom games, both lists should match content of games_list.items in interface.c + DEAL_N_CARDS
const game_rules_t rules_list[] =
{
// DEAL_N_CARDS not run by run_game
		{ .game_code = DEAL_N_CARDS, .deck_size = 0, .min_players = 0,
				.max_players = 0, .n_hole_cards = 1, .stud_structure =
				false, .hole_sequence = 0, .n_cc_stages = 0, .cc_stages =
				{ }, .allows_burn_cards = false, .allows_xtra_cards = false,
				.max_pick = 0, .recycle_mode = RECYCLE_ON, .random_player =
				false },
		{ .game_code = _5_CARD_DRAW, .deck_size = 52, .min_players = 2,
				.max_players = 6, .n_hole_cards = 5, .stud_structure =
				false, .hole_sequence = 0, .n_cc_stages = 0, .cc_stages =
				{ }, .allows_burn_cards = false, .allows_xtra_cards = false,
				.max_pick = 3, .recycle_mode = RECYCLE_OFF, .random_player =
				false },
		{ .game_code = _5_CARD_STUD, .deck_size = 52, .min_players = 2,
				.max_players = 10, .n_hole_cards = 5, .stud_structure =
				true, .hole_sequence = 0x56, .n_cc_stages = 0, .cc_stages =
				{ }, .allows_burn_cards = false, .allows_xtra_cards = true,
				.max_pick = 0, .recycle_mode = RECYCLE_OFF, .random_player =
				false },
		{ .game_code = _6_CARD_STUD, .deck_size = 52, .min_players = 2,
				.max_players = 8, .n_hole_cards = 6, .stud_structure =
				true, .hole_sequence = 0x156, .n_cc_stages = 0, .cc_stages =
				{ }, .allows_burn_cards = false, .allows_xtra_cards = true,
				.max_pick = 0, .recycle_mode = RECYCLE_OFF, .random_player =
				false },
		{ .game_code = _7_CARD_DRAW, .deck_size = 52, .min_players = 2,
				.max_players = 6, .n_hole_cards = 7, .stud_structure =
				false, .hole_sequence = 0, .n_cc_stages = 0, .cc_stages =
				{ }, .allows_burn_cards = false, .allows_xtra_cards = false,
				.max_pick = 3, .recycle_mode = RECYCLE_OFF, .random_player =
				false },
		{ .game_code = _7_CARD_STUD, .deck_size = 52, .min_players = 2,
				.max_players = 7, .n_hole_cards = 7, .stud_structure =
				true, .hole_sequence = 0x157, .n_cc_stages = 0, .cc_stages =
				{ }, .allows_burn_cards = false, .allows_xtra_cards = true,
				.max_pick = 0, .recycle_mode = RECYCLE_OFF, .random_player =
				false },
		{ .game_code = CHINESE_POKER, .deck_size = 52, .min_players = 2,
				.max_players = 4, .n_hole_cards = 13, .stud_structure =
				false, .hole_sequence = 0, .n_cc_stages = 0, .cc_stages =
				{ }, .allows_burn_cards = false, .allows_xtra_cards = false,
				.max_pick = 0, .recycle_mode = RECYCLE_OFF, .random_player =
				false },
		{ .game_code = DBL_BRD_HOLDEM, .deck_size = 52, .min_players = 2,
				.max_players = 10, .n_hole_cards = 2, .stud_structure =
				false, .hole_sequence = 0, .n_cc_stages = 3, .cc_stages =
				{ flop_x2_stage, turn_x2_stage, river_x2_stage },
				.allows_burn_cards = true, .allows_xtra_cards = true,
				.max_pick = 0, .recycle_mode = RECYCLE_OFF, .random_player =
				false },
		{ .game_code = IRISH_POKER, .deck_size = 52, .min_players = 2,
				.max_players = 10, .n_hole_cards = 4, .stud_structure =
				false, .hole_sequence = 0, .n_cc_stages = 3, .cc_stages =
				{ flop_stage, turn_stage, river_stage }, .allows_burn_cards =
				true, .allows_xtra_cards = true, .max_pick = 0, .recycle_mode =
						RECYCLE_OFF, .random_player = false },
		{ .game_code = OMAHA, .deck_size = 52, .min_players = 2, .max_players =
				10, .n_hole_cards = 4, .stud_structure =
		false, .hole_sequence = 0, .n_cc_stages = 3, .cc_stages =
		{ flop_stage, turn_stage, river_stage }, .allows_burn_cards =
		true, .allows_xtra_cards = true, .max_pick = 0, .recycle_mode =
				RECYCLE_OFF, .random_player = false },
		{ .game_code = PINEAPPLE, .deck_size = 52, .min_players = 2,
				.max_players = 10, .n_hole_cards = 3, .stud_structure = false,
				.hole_sequence = 0, .n_cc_stages = 3, .cc_stages =
				{ flop_stage, turn_stage, river_stage }, .allows_burn_cards =
				true, .allows_xtra_cards = true, .max_pick = 0, .recycle_mode =
						RECYCLE_OFF, .random_player = false },
		{ .game_code = TEXAS_HOLDEM, .deck_size = 52, .min_players = 2,
				.max_players = 10, .n_hole_cards = 2, .stud_structure = false,
				.hole_sequence = 0, .n_cc_stages = 3, .cc_stages =
				{ flop_stage, turn_stage, river_stage }, .allows_burn_cards =
				true, .allows_xtra_cards = true, .max_pick = 0, .recycle_mode =
						RECYCLE_OFF, .random_player = false },
		{ .game_code = _3_CD_POKER, .deck_size = 52, .min_players = 2,
				.max_players = 8, .n_hole_cards = 3, .stud_structure =
				false, .hole_sequence = 0, .n_cc_stages = 0, .cc_stages =
				{ }, .allows_burn_cards = false, .allows_xtra_cards = true,
				.max_pick = 0, .recycle_mode = RECYCLE_OFF, .random_player =
				false },
		{ .game_code = _4_CD_POKER, .deck_size = 52, .min_players = 2,
				.max_players = 8, .n_hole_cards = 5, .stud_structure =
				false, .hole_sequence = 0, .n_cc_stages = 1, .cc_stages =
				{ dealer_stage }, .allows_burn_cards = false,
				.allows_xtra_cards = true, .max_pick = 0, .recycle_mode =
						RECYCLE_OFF, .random_player =
				false },
		{ .game_code = BACCARAT, .deck_size = 52, .min_players = 2,
				.max_players = 2, .n_hole_cards = 2, .stud_structure =
				false, .hole_sequence = 0, .n_cc_stages = 0, .cc_stages =
				{ }, .allows_burn_cards = false, .allows_xtra_cards = false,
				.max_pick = 1, .recycle_mode = RECYCLE_OFF, .random_player =
				false },
		{ .game_code = BLACKJACK, .deck_size = 52, .min_players = 2,
				.max_players = 8, .n_hole_cards = 2, .stud_structure =
				false, .hole_sequence = 0, .n_cc_stages = 0, .cc_stages =
				{ }, .allows_burn_cards = false, .allows_xtra_cards = false,
				.max_pick = 1, .recycle_mode = RECYCLE_OFF, .random_player =
				false },
		{ .game_code = LETEM_RIDE, .deck_size = 52, .min_players = 2,
				.max_players = 7, .n_hole_cards = 3, .stud_structure =
				false, .hole_sequence = 0, .n_cc_stages = 1, .cc_stages =
				{ _2_stage }, .allows_burn_cards = true, .allows_xtra_cards =
				true, .max_pick = 0, .recycle_mode = RECYCLE_OFF,
				.random_player =
				false },
		{ .game_code = MAXIMUM_FLUSH, .deck_size = 52, .min_players = 2,
				.max_players = 8, .n_hole_cards = 7, .stud_structure =
				false, .hole_sequence = 0, .n_cc_stages = 0, .cc_stages =
				{ }, .allows_burn_cards = false, .allows_xtra_cards = true,
				.max_pick = 0, .recycle_mode = RECYCLE_OFF, .random_player =
				false },
		{ .game_code = PAIGOW, .deck_size = 53, .min_players = 2, .max_players =
				7, .n_hole_cards = 7, .stud_structure = false, .hole_sequence =
				0, .n_cc_stages = 0, .cc_stages =
		{ }, .allows_burn_cards = false, .allows_xtra_cards = true, .max_pick =
				0, .recycle_mode = RECYCLE_OFF, .random_player = true },
		{ .game_code = SOUTHERN_STUD, .deck_size = 52, .min_players = 2,
				.max_players = 8, .n_hole_cards = 2, .stud_structure =
				false, .hole_sequence = 0, .n_cc_stages = 3, .cc_stages =
				{ _3st_stage, _4st_stage, _5st_stage }, .allows_burn_cards =
				true, .allows_xtra_cards = true, .max_pick = 0, .recycle_mode =
						RECYCLE_OFF, .random_player = false },
		{ .game_code = BRIDGE, .deck_size = 52, .min_players = 4, .max_players =
				4, .n_hole_cards = 13, .stud_structure = false, .hole_sequence =
				0, .n_cc_stages = 0, .cc_stages =
		{ }, .allows_burn_cards = false, .allows_xtra_cards = false, .max_pick =
				0, .recycle_mode = RECYCLE_OFF, .random_player =
		false },
		{ .game_code = CRIBBAGE, .deck_size = 52, .min_players = 2,
				.max_players = 4, .n_hole_cards = 6, .stud_structure =
				false, .hole_sequence = 0, .n_cc_stages = 1, .cc_stages =
				{ starter_stage }, .allows_burn_cards = false,
				.allows_xtra_cards = true, .max_pick = 0, .recycle_mode =
						RECYCLE_OFF, .random_player =
				false },
		{ .game_code = GIN_RUMMY, .deck_size = 52, .min_players = 2,
				.max_players = 2, .n_hole_cards = 10, .stud_structure =
				false, .hole_sequence = 0, .n_cc_stages = 0, .cc_stages =
				{ }, .allows_burn_cards = false, .allows_xtra_cards = false,
				.max_pick = 1, .recycle_mode = RECYCLE_ON, .random_player =
				false },
		{ .game_code = HEARTS, .deck_size = 52, .min_players = 4, .max_players =
				4, .n_hole_cards = 13, .stud_structure = false, .hole_sequence =
				0, .n_cc_stages = 0, .cc_stages =
		{ }, .allows_burn_cards = false, .allows_xtra_cards = false, .max_pick =
				0, .recycle_mode = RECYCLE_OFF, .random_player =
		false },
		{ .game_code = LANDLORD, .deck_size = 54, .min_players = 3,
				.max_players = 3, .n_hole_cards = 17, .stud_structure =
				false, .hole_sequence = 0, .n_cc_stages = 1, .cc_stages =
				{ landlord_stage }, .allows_burn_cards = false,
				.allows_xtra_cards = false, .max_pick = 0, .recycle_mode =
						RECYCLE_OFF, .random_player =
				false },
		{ .game_code = MATATU, .deck_size = 52, .min_players = 2, .max_players =
				5, .n_hole_cards = 7, .stud_structure = false, .hole_sequence =
				0, .n_cc_stages = 1, .cc_stages =
		{ cut_card_stage }, .allows_burn_cards = false, .allows_xtra_cards =
		false, .max_pick = 2, .recycle_mode = RECYCLE_ON, .random_player =
		false },
		{ .game_code = RUMMY, .deck_size = 52, .min_players = 3, .max_players =
				6, .n_hole_cards = 7, .stud_structure = false, .hole_sequence =
				0, .n_cc_stages = 0, .cc_stages =
		{ }, .allows_burn_cards = false, .allows_xtra_cards = false, .max_pick =
				0, .recycle_mode = RECYCLE_OFF, .random_player =
		false },
		{ .game_code = SPADES, .deck_size = 52, .min_players = 4, .max_players =
				4, .n_hole_cards = 13, .stud_structure = false, .hole_sequence =
				0, .n_cc_stages = 0, .cc_stages =
		{ }, .allows_burn_cards = false, .allows_xtra_cards = false, .max_pick =
				0, .recycle_mode = RECYCLE_OFF, .random_player =
		false },
		{ .game_code = TIEN_LEN, .deck_size = 52, .min_players = 4,
				.max_players = 4, .n_hole_cards = 13, .stud_structure = false,
				.hole_sequence = 0, .n_cc_stages = 0, .cc_stages =
				{ }, .allows_burn_cards = false, .allows_xtra_cards = false,
				.max_pick = 0, .recycle_mode = RECYCLE_OFF, .random_player =
				false },

		// CUSTOM GAMES pro forma

		{ .game_code = CUSTOM_1 },
		{ .game_code = CUSTOM_2 },
		{ .game_code = CUSTOM_3 },
		{ .game_code = CUSTOM_4 },
		{ .game_code = CUSTOM_5 }

};

const user_prefs_t default_prefs_list[] =
		{
		// DEAL_N_CARDS not run by run_game
				{ .game_code = DEAL_N_CARDS, .activated = true, .dist_mode = 0,
						.hole_cards_dest = 0, .community_cards_dest = 0,
						.burn_cards = 0, .community_timing = 0,
						.empty_machine_after = true, .default_n_players = 0 },

				{ .game_code = _5_CARD_DRAW, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = 0, .burn_cards = false,
						.community_timing = 0, .empty_machine_after =
						false, .default_n_players = 4 },

				{ .game_code = _5_CARD_STUD, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = 0, .burn_cards = 0,
						.community_timing = 0, .empty_machine_after = false,
						.default_n_players = 4 },

				{ .game_code = _6_CARD_STUD, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = 0, .burn_cards = 0,
						.community_timing = 0, .empty_machine_after = false,
						.default_n_players = 4 },

				{ .game_code = _7_CARD_DRAW, .activated = true, .dist_mode =
						DIST_MODE_BY_PLAYER, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = 0, .burn_cards = 0,
						.community_timing = 0, .empty_machine_after = false,
						.default_n_players = 4 },

				{ .game_code = _7_CARD_STUD, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = 0, .burn_cards = 0,
						.community_timing = 0, .empty_machine_after = false,
						.default_n_players = 4 },

				{ .game_code = CHINESE_POKER, .activated = true, .dist_mode =
						DIST_MODE_BY_PLAYER, .hole_cards_dest =
						DEST_INTERNAL_TRAY, .community_cards_dest = 0,
						.burn_cards = 0, .community_timing = 0,
						.empty_machine_after = false, .default_n_players = 4 },

				{ .game_code = DBL_BRD_HOLDEM, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = DEST_TABLE, .burn_cards = 0,
						.community_timing = COMMUNITY_TIMING_AFTER_HOLES,
						.empty_machine_after = false, .default_n_players = 4 },

				{ .game_code = IRISH_POKER, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = DEST_TABLE, .burn_cards = 0,
						.community_timing = COMMUNITY_TIMING_AFTER_HOLES,
						.empty_machine_after = false, .default_n_players = 4 },

				{ .game_code = OMAHA, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = DEST_TABLE, .burn_cards = 0,
						.community_timing = COMMUNITY_TIMING_AFTER_HOLES,
						.empty_machine_after = false, .default_n_players = 4 },

				{ .game_code = PINEAPPLE, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = DEST_TABLE, .burn_cards = 0,
						.community_timing = COMMUNITY_TIMING_AFTER_HOLES,
						.empty_machine_after = false, .default_n_players = 4 },

				{ .game_code = TEXAS_HOLDEM, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = DEST_TABLE, .burn_cards = false,
						.community_timing = COMMUNITY_TIMING_AFTER_HOLES,
						.empty_machine_after = false, .default_n_players = 6 },

				{ .game_code = _3_CD_POKER, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = DEST_TABLE, .burn_cards = 0,
						.community_timing = 0, .empty_machine_after = false,
						.default_n_players = 4 },

				{ .game_code = _4_CD_POKER, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = DEST_TABLE, .burn_cards = 0,
						.community_timing = COMMUNITY_TIMING_AFTER_HOLES,
						.empty_machine_after = false, .default_n_players = 4 },

				{ .game_code = BACCARAT, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = 0, .burn_cards = 0,
						.community_timing = 0, .empty_machine_after = false,
						.default_n_players = 2 },

				{ .game_code = BLACKJACK, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = 0, .burn_cards = 0,
						.community_timing = 0, .empty_machine_after = false,
						.default_n_players = 4 },

				{ .game_code = LETEM_RIDE, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = DEST_TABLE, .burn_cards = 0,
						.community_timing = COMMUNITY_TIMING_AFTER_HOLES,
						.empty_machine_after = false, .default_n_players = 4 },

				{ .game_code = MAXIMUM_FLUSH, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = 0, .burn_cards = 0,
						.community_timing = 0, .empty_machine_after = false,
						.default_n_players = 4 },

				{ .game_code = PAIGOW, .activated = true, .dist_mode =
						DIST_MODE_BY_PLAYER, .hole_cards_dest =
						DEST_INTERNAL_TRAY, .community_cards_dest = 0,
						.burn_cards = false, .community_timing = 0,
						.empty_machine_after = true, .default_n_players = 5 },

				{ .game_code = SOUTHERN_STUD, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = DEST_TABLE, .burn_cards = 0,
						.community_timing = COMMUNITY_TIMING_AFTER_HOLES,
						.empty_machine_after = false, .default_n_players = 4 },

				{ .game_code = BRIDGE, .activated = true, .dist_mode =
						DIST_MODE_BY_PLAYER, .hole_cards_dest =
						DEST_INTERNAL_TRAY, .community_cards_dest = 0,
						.burn_cards = 0, .community_timing = 0,
						.empty_machine_after = false, .default_n_players = 4 },

				{ .game_code = CRIBBAGE, .activated = true, .dist_mode =
						DIST_MODE_BY_PLAYER, .hole_cards_dest =
						DEST_INTERNAL_TRAY, .community_cards_dest = DEST_TABLE,
						.burn_cards = 0, .community_timing =
								COMMUNITY_TIMING_AFTER_HOLES,
						.empty_machine_after = false, .default_n_players = 4 },

				{ .game_code = GIN_RUMMY, .activated = true, .dist_mode =
						DIST_MODE_BY_PLAYER, .hole_cards_dest =
						DEST_INTERNAL_TRAY, .community_cards_dest = 0,
						.burn_cards = 0, .community_timing = 0,
						.empty_machine_after = false, .default_n_players = 4 },

				{ .game_code = HEARTS, .activated = true, .dist_mode =
						DIST_MODE_BY_PLAYER, .hole_cards_dest =
						DEST_INTERNAL_TRAY, .community_cards_dest = 0,
						.burn_cards = 0, .community_timing = 0,
						.empty_machine_after = false, .default_n_players = 4 },

				{ .game_code = LANDLORD, .activated = true, .dist_mode =
						DIST_MODE_BY_PLAYER, .hole_cards_dest =
						DEST_INTERNAL_TRAY, .community_cards_dest = DEST_TABLE,
						.burn_cards = 0, .community_timing = 0,
						.empty_machine_after = false, .default_n_players = 4 },

				{ .game_code = MATATU, .activated = true, .dist_mode =
						DIST_MODE_BY_PLAYER, .hole_cards_dest =
						DEST_INTERNAL_TRAY, .community_cards_dest = DEST_TABLE,
						.burn_cards = 0, .community_timing =
								COMMUNITY_TIMING_AFTER_HOLES,
						.empty_machine_after =
						true, .default_n_players = 2 },

				{ .game_code = RUMMY, .activated = true, .dist_mode =
						DIST_MODE_BY_PLAYER, .hole_cards_dest =
						DEST_INTERNAL_TRAY, .community_cards_dest = 0,
						.burn_cards = 0, .community_timing = 0,
						.empty_machine_after = false, .default_n_players = 4 },

				{ .game_code = SPADES, .activated = true, .dist_mode =
						DIST_MODE_BY_PLAYER, .hole_cards_dest =
						DEST_INTERNAL_TRAY, .community_cards_dest = 0,
						.burn_cards = 0, .community_timing = 0,
						.empty_machine_after = false, .default_n_players = 4 },

				{ .game_code = TIEN_LEN, .activated = true, .dist_mode =
						DIST_MODE_BY_PLAYER, .hole_cards_dest =
						DEST_INTERNAL_TRAY, .community_cards_dest = 0,
						.burn_cards = 0, .community_timing = 0,
						.empty_machine_after = false, .default_n_players = 4 },

				// CUSTOM GAMES

				{ .game_code = CUSTOM_1, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = DEST_TABLE, .burn_cards = false,
						.community_timing = COMMUNITY_TIMING_AFTER_HOLES,
						.empty_machine_after =
						false, .default_n_players = 6 },
				{ .game_code = CUSTOM_2, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = DEST_TABLE, .burn_cards = false,
						.community_timing = COMMUNITY_TIMING_AFTER_HOLES,
						.empty_machine_after =
						false, .default_n_players = 6 },
				{ .game_code = CUSTOM_3, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = DEST_TABLE, .burn_cards = false,
						.community_timing = COMMUNITY_TIMING_AFTER_HOLES,
						.empty_machine_after =
						false, .default_n_players = 6 },
				{ .game_code = CUSTOM_4, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = DEST_TABLE, .burn_cards = false,
						.community_timing = COMMUNITY_TIMING_AFTER_HOLES,
						.empty_machine_after =
						false, .default_n_players = 6 },
				{ .game_code = CUSTOM_5, .activated = true, .dist_mode =
						DIST_MODE_ROUND_ROBIN, .hole_cards_dest = DEST_TABLE,
						.community_cards_dest = DEST_TABLE, .burn_cards = false,
						.community_timing = COMMUNITY_TIMING_AFTER_HOLES,
						.empty_machine_after =
						false, .default_n_players = 6 }

		};

// Both should be equal, checked upon start
uint16_t n_presets = sizeof(rules_list) / sizeof(game_rules_t);
uint16_t n_user_prefs = sizeof(default_prefs_list) / sizeof(user_prefs_t);

bool is_valid_user_input(return_code_t return_code)
{
	bool found = false;

	for (uint16_t i = 0; i < n_valid_user_inputs; i++)
		if (return_code == valid_user_inputs[i])
		{
			found = true;
			break;
		}

	return found;
}

// read_user_prefs finds user preferences from EERAM
return_code_t read_user_prefs(user_prefs_t *p_prefs, item_code_t game_code)
{
	return_code_t ret_val;
	uint16_t game_index;

	// Set all other parameters from EERAM
	if ((ret_val = get_game_index(&game_index, game_code)) != LS_OK)
		goto _EXIT;
	ret_val = read_eeram(
	EERAM_USER_PREFS + game_index * E_USER_PREFS_SIZE, p_prefs->bytes,
	E_USER_PREFS_SIZE);

	// Set game code
	p_prefs->game_code = game_code;

	_EXIT:

	return ret_val;
}

// write_user_prefs writes updated user preferences to EERAM
// GAME_CODE COULD BE TAKEN DIRECTLY FROM USER_PRES
return_code_t write_user_prefs(user_prefs_t *p_prefs, item_code_t game_code)
{
	return_code_t ret_val;
	uint16_t game_index;

	if ((ret_val = get_game_index(&game_index, game_code)) != LS_OK)
		goto _EXIT;
	ret_val = write_eeram(
	EERAM_USER_PREFS + game_index * E_USER_PREFS_SIZE, p_prefs->bytes,
	E_USER_PREFS_SIZE);

	_EXIT:

	return ret_val;
}

// reset_user_prefs resets user preferences in EERAM
return_code_t reset_user_prefs(void)
{
	return_code_t ret_val = LS_OK;
	extern uint16_t n_presets;
	uint8_t w_data;

	clear_message(TEXT_ERROR);
	prompt_message("\nPreferences reset in progress");

	// Reset LCD history
	if ((ret_val = reset_eeram(EERAM_LCD_SCROLL, E_LCD_SCROLL_SIZE)))
		goto _EXIT;
	if ((ret_val = reset_eeram(EERAM_LCD_ROW, E_LCD_ROW_SIZE)))
		goto _EXIT;
	if ((ret_val = reset_eeram(EERAM_CLL_MEN, E_CLL_MEN_SIZE)))
		goto _EXIT;

	// Set calling menu for secret menu
	w_data = SETTINGS;
	if ((ret_val = write_eeram(EERAM_CLL_MEN + MAINTENANCE_LEVEL_2, &w_data, 1))
			!= LS_OK)
		goto _EXIT;

	// Reset dynamic menus
	if ((ret_val = reset_eeram(EERAM_FAVS, E_FAVS_SIZE)))
		goto _EXIT;
	if ((ret_val = reset_eeram(EERAM_DLRS, E_DLRS_SIZE)))
		goto _EXIT;

	// Reset user preferences
	for (uint16_t i = 0; i < n_presets; i++)
	{
		user_prefs_t user_preferences = default_prefs_list[i];
		if ((ret_val = write_user_prefs(&user_preferences,
				user_preferences.game_code)) != LS_OK)
			goto _EXIT;
	}

	// Set default deck
	if ((ret_val = write_default_n_deck(N_DEFAULT_DECK)) != LS_OK)
		goto _EXIT;
	// Set default double deck
	if ((ret_val = write_default_n_double_deck(N_DEFAULT_DOUBLE_DECK)) != LS_OK)
		goto _EXIT;
	// Set default deal gap index and cut card flag
	extern gen_prefs_t gen_prefs;
	gen_prefs.byte = 0;
	gen_prefs.deal_gap_index = DEFAULT_DEAL_GAP_INDEX;
	gen_prefs.cut_card_flag = DEFAULT_CUT_CARD_FLAG;
	ret_val = write_eeram(EERAM_GEN_PREFS, &gen_prefs.byte, E_GEN_PREFS_SIZE);

	_EXIT:

	HAL_Delay(M_WAIT_DELAY);
	clear_message(TEXT_ERROR);
	beep(SHORT_BEEP);

	return ret_val;
}

// read_custom_game_name gets them from EERAM
return_code_t read_custom_game_name(char text[GAME_NAME_MAX_CHAR + 1],
		item_code_t custom_game_code)
{
	return_code_t ret_val = read_eeram(
			EERAM_CUST_GAMES_NM
					+ (custom_game_code - CUSTOM_1) * (GAME_NAME_MAX_CHAR + 1),
			(uint8_t*) text, (GAME_NAME_MAX_CHAR + 1));

	return ret_val;
}

return_code_t write_custom_game_name(char text[GAME_NAME_MAX_CHAR + 1],
		item_code_t custom_game_code)
{
	return_code_t ret_val = write_eeram(
			EERAM_CUST_GAMES_NM
					+ (custom_game_code - CUSTOM_1) * (GAME_NAME_MAX_CHAR + 1),
			(uint8_t*) text, (GAME_NAME_MAX_CHAR + 1));

	return ret_val;
}

return_code_t read_custom_game_rule(game_rules_t *p_game_rules,
		item_code_t custom_game_code)
{
	return_code_t ret_val = read_eeram(
	EERAM_CUST_GAMES_RLS + (custom_game_code - CUSTOM_1) * E_GAME_RULES_SIZE,
			p_game_rules->bytes, E_GAME_RULES_SIZE);

	return ret_val;
}

return_code_t write_custom_game_rule(game_rules_t *p_game_rules,
		item_code_t custom_game_code)
{
	return_code_t ret_val = write_eeram(
	EERAM_CUST_GAMES_RLS + (custom_game_code - CUSTOM_1) * E_GAME_RULES_SIZE,
			p_game_rules->bytes,
			E_GAME_RULES_SIZE);

	return ret_val;
}

// reset_custom_games reset the rules and preferences of custom games to standard names and Bridge rules
return_code_t reset_custom_games(void)
{
	return_code_t ret_val = LS_OK;
	extern char custom_game_names[N_CUSTOM_GAMES][GAME_NAME_MAX_CHAR + 1];
	char text[GAME_NAME_MAX_CHAR + 1];
	game_rules_t game_rules;
	user_prefs_t user_preferences;

	clear_message(TEXT_ERROR);
	prompt_message("\nCustom games reset in progress");

	for (item_code_t custom_game_code = CUSTOM_1;
			custom_game_code < CUSTOM_1 + N_CUSTOM_GAMES; custom_game_code++)
	{
		// Define custom name as Custom N
		snprintf(text, GAME_NAME_MAX_CHAR, "Custom %u",
				custom_game_code + 1 - CUSTOM_1);

		// Reset game name in memory
		strncpy(custom_game_names[custom_game_code - CUSTOM_1], text,
		GAME_NAME_MAX_CHAR);

		// Reset game name in EERAM
		if ((ret_val = write_custom_game_name(text, custom_game_code)) != LS_OK)
			goto _EXIT;

		// Reset game rules to Texas HoldEm
		get_rules(&game_rules, TEXAS_HOLDEM);
		if ((ret_val = write_custom_game_rule(&game_rules, custom_game_code))
				!= LS_OK)
			goto _EXIT;

		// Reset user preferences to Texas HoldEm
		read_user_prefs(&user_preferences, TEXAS_HOLDEM);
		if ((ret_val = write_user_prefs(&user_preferences, custom_game_code))
				!= LS_OK)
			goto _EXIT;
	}

	_EXIT:

	HAL_Delay(M_WAIT_DELAY);
	clear_message(TEXT_ERROR);
	beep(SHORT_BEEP);

	return ret_val;
}

return_code_t reset_content(void)
{
	return_code_t ret_val = LS_OK;
	extern uint8_t n_cards_in;

	clear_message(TEXT_ERROR);
	prompt_message("\nContent reset in progress");

	// Set carousel empty
	if ((ret_val = reset_carousel()) != LS_OK)
		goto _EXIT;
	// Update n_cards_in
	if ((ret_val = get_n_cards_in()) != LS_OK)
		goto _EXIT;
	// Check no logical error
	if (n_cards_in != 0)
	{
		ret_val = LS_ERROR;
		goto _EXIT;
	}

	// Reset machine state
	if ((ret_val = reset_machine_state()) != LS_OK)
		goto _EXIT;

	// Reset game state
	if ((ret_val = reset_game_state()) != LS_OK)
		goto _EXIT;

	_EXIT:

	ret_val = home_carousel();
	clear_message(TEXT_ERROR);
	beep(SHORT_BEEP);

	return ret_val;
}

// Prompts the card just discharged
void prompt_discharged_cards(game_rules_t game_rules, user_prefs_t user_prefs)
{
	uint16_t x_pos;
	uint16_t y_pos;
	uint16_t number_width = 45;

	x_pos = (BSP_LCD_GetXSize() - number_width) / 2;
	y_pos = LCD_TOP_ROW + LCD_ROW_HEIGHT * (MSG_ROW + 1) + V_ADJUST; // + 1 because of LF in text

	// If community cards stage and dealt first,generic prompt
	if (game_state.current_stage >= CC_1 && game_state.current_stage <= CC_5
			&& user_prefs.community_timing == COMMUNITY_TIMING_BEFORE_HOLES)
	{
		uint16_t n_cards = 0;
		for (uint16_t i = 0; i < game_rules.n_cc_stages; i++)
			n_cards += game_rules.cc_stages[i].n_cards;
		prompt_com_cards(game_state.n_cards_dealt, n_cards);
	}
	else
		// Used for hole cards, if distribution per player only
		switch (game_state.current_stage)
		{
			extern icon_set_t icon_set_void;
		case SHUFFLING:
			// Display icon
			display_escape_icon(ICON_CROSS);
			// Clear number area
			BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
			BSP_LCD_FillRect(x_pos, y_pos, number_width, LCD_ROW_HEIGHT);
			BSP_LCD_SetTextColor(LCD_COLOR_TEXT);
			// Show number
			atomic_prompt(CUSTOM_MESSAGE, "Card", MSG_ROW, NO_HOLD,
			LCD_REGULAR_FONT,
			LCD_COLOR_TEXT, false);
			snprintf(display_buf, N_DISP_MAX, "# %2u",
					game_state.n_cards_dealt);
			atomic_prompt(CUSTOM_MESSAGE, display_buf, MSG_ROW + 1, NO_HOLD,
			LCD_REGULAR_FONT,
			LCD_COLOR_RED_SHFLR, false);
			break;

		case DEALING_N_CARDS: // Same as SHUFFLING
			// Display icon
			display_escape_icon(ICON_CROSS);
			// Clear number area
			BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
			BSP_LCD_FillRect(x_pos, y_pos, number_width, LCD_ROW_HEIGHT);
			BSP_LCD_SetTextColor(LCD_COLOR_TEXT);
			// Show number
			atomic_prompt(CUSTOM_MESSAGE, "Card", MSG_ROW, NO_HOLD,
			LCD_REGULAR_FONT,
			LCD_COLOR_TEXT, false);
			snprintf(display_buf, N_DISP_MAX, "# %2u",
					game_state.n_cards_dealt);
			atomic_prompt(CUSTOM_MESSAGE, display_buf, MSG_ROW + 1, NO_HOLD,
			LCD_REGULAR_FONT,
			LCD_COLOR_RED_SHFLR, false);
			break;

		case EMPTYING:
			prompt_interface(MESSAGE, CUSTOM_MESSAGE, "Emptying...",
					icon_set_void, ICON_CROSS, NO_HOLD);
			break;

		case SAFE_EMPTYING:
			prompt_interface(MESSAGE, CUSTOM_MESSAGE, "Secure emptying...",
					icon_set_void, ICON_VOID, NO_HOLD);
			break;

		case CC_1:
			prompt_com_cards(game_state.n_cards_dealt,
					game_rules.cc_stages[0].n_cards);
			break;

		case CC_2:
			prompt_com_cards(game_state.n_cards_dealt,
					game_rules.cc_stages[1].n_cards);
			break;

		case CC_3:
			prompt_com_cards(game_state.n_cards_dealt,
					game_rules.cc_stages[2].n_cards);
			break;

		case CC_4:
			prompt_com_cards(game_state.n_cards_dealt,
					game_rules.cc_stages[3].n_cards);
			break;

		case CC_5:
			prompt_com_cards(game_state.n_cards_dealt,
					game_rules.cc_stages[4].n_cards);
			break;

		case PICK:
			prompt_com_cards(game_state.n_cards_dealt, game_state.n_pick);
			break;

		default:

		}
}

// Clears discharged cars display
void clear_discharged_cards(void)
{
	prompt_menu_item("                         ", MSG_ROW);
}

/**
 * @brief  discharge_cards:    Eject n_cards - actions latch if random,
 *         just monitors cards passing if sequential
 *         need to close latch at the end if SEQ_MODE or SAFE_MODE
 *         Used by shuffle, deal_hole_cards (when dealing by player), cc stages
 *         and emptying
 *         Updates game_state
 *         MAY LEAVE THE LATCH OPEN
 * @param  n_cards:        number of cards to discharge
 * @param  safe_mode:      SAFE_MODE or NON_SAFE_MODE
 * @param  rand_mode:      SEQ_MODE or RAND_MODE
 * @param  p_card_count:   number of cards discharged (pointer)
 * @retval LS_OK, any error from goToPosition (ALIGNMENT), SLOT_IS_EMPTY, CARD_STUCK_ON_EXIT
 *         also LS_ERROR, TRNG_ERROR and INVALID_CHOICE also NOT_ENOUGH_CARDS_IN_SHUFFLER
 */
return_code_t discharge_cards(game_rules_t game_rules)
{
	return_code_t ret_val = LS_OK;
	extern uint8_t n_cards_in;
	extern int8_t carousel_pos;
	extern icon_set_t icon_set_void;
	int8_t targets[N_SLOTS];
	uint16_t n_cards;
	safe_mode_t safe_mode;
	rand_mode_t rand_mode;
	user_prefs_t user_prefs;

// Get user prefs
	if (is_proper_game(game_rules.game_code)
			&& (ret_val = read_user_prefs(&user_prefs, game_rules.game_code))
					!= LS_OK)
		goto _EXIT;

// Get machine state, game_state and n_cards_in
	if ((ret_val = read_machine_state()) != LS_OK || (ret_val =
			read_game_state()) != LS_OK
			|| (ret_val = get_n_cards_in()) != LS_OK)
		goto _EXIT;

	// Initialise safe mode and random mode
	safe_mode = NON_SAFE_MODE;
	rand_mode =
			(machine_state.random_in == RANDOM_STATE ? SEQ_MODE : RAND_MODE);

	// Set n_cards, and (if emptying) safe mode and random mode
	// If community cards stage and dealt first, deal them all in one go
	if (game_state.current_stage >= CC_1 && game_state.current_stage <= CC_5
			&& user_prefs.community_timing == COMMUNITY_TIMING_BEFORE_HOLES)
	{
		n_cards = 0;
		for (uint16_t i = 0; i < game_rules.n_cc_stages; i++)
			n_cards += game_rules.cc_stages[i].n_cards;
	}
	// Otherwise case by case process
	else
		switch (game_state.current_stage)
		{

			case HOLE_CARDS:
				n_cards = game_rules.n_hole_cards - game_state.n_cards_dealt;

				break;

			case CC_1:
				n_cards = game_rules.cc_stages[0].n_cards
						- game_state.n_cards_dealt;
				break;

			case CC_2:
				n_cards = game_rules.cc_stages[1].n_cards
						- game_state.n_cards_dealt;
				break;

			case CC_3:
				n_cards = game_rules.cc_stages[2].n_cards
						- game_state.n_cards_dealt;
				break;

			case CC_4:
				n_cards = game_rules.cc_stages[3].n_cards
						- game_state.n_cards_dealt;
				break;

			case CC_5:
				n_cards = game_rules.cc_stages[4].n_cards
						- game_state.n_cards_dealt;
				break;

			case PICK:
				n_cards = game_state.n_pick - game_state.n_cards_dealt;
				break;

			case DEALING_N_CARDS:
				n_cards = game_rules.n_hole_cards - game_state.n_cards_dealt;
				break;

			case EMPTYING:
				n_cards = n_cards_in;
				rand_mode = SEQ_MODE;
				break;

			case SAFE_EMPTYING:
				n_cards = N_SLOTS;
				safe_mode = SAFE_MODE;
				rand_mode = SEQ_MODE;
				break;

			case SHUFFLING:
				n_cards = n_cards_in;
				break;

			default:
				ret_val = INVALID_CHOICE;
				goto _EXIT;

		}

	// Check consistency: n_cards must be > 0 in non-safe mode
	if (n_cards == 0 && safe_mode != SAFE_MODE)
	{
		ret_val = LS_ERROR;
		goto _EXIT;
	}

	// In non-safe mode there should be enough cards
	if (safe_mode == NON_SAFE_MODE && n_cards > n_cards_in)
	{
		ret_val = (n_cards_in == 0) ? SENCIT : NOT_ENOUGH_CARDS_IN_SHUFFLER;
		goto _EXIT;
	}

	/* SET TARGETS */
	// If in safe mode empty sequential from current position
	if (safe_mode == SAFE_MODE)
	{
		// Sequential targets starting at current position
		for (uint8_t i = 0; i < N_SLOTS; i++)
			targets[i] = (carousel_pos + i) % N_SLOTS;
	}

	/*
	 * Discharge modes based on internal state:
	 * if emptying       -> seq mode
	 * else if random state
	 *      shuffle     -> seq mode   + cut
	 *      h/c/p 1st   -> seq mode   + cut
	 *      h/c/p 2+    -> seq mode
	 * else if sequential state
	 *      shuffle     -> rand mode
	 *      h/c/p       -> rand mode  + optimise
	 *
	 */
	// Else if in non-safe mode, select process as above
	// If sequential mode, optional cut
	else if (rand_mode == SEQ_MODE)
	{
		// Flag sequential discharge
		if ((write_flag(SEQ_DSCHG_FLAG, SEQUENTIAL_STATE)) != LS_OK)
			goto _EXIT;
		// Set sequential targets
		if ((ret_val = read_slots_w_offset(targets, FULL_SLOT)) != LS_OK)
			goto _EXIT;
		// Cut deck if needed
		int8_t starting_position = carousel_pos;

		// Get number of non-empty compartments
		carousel_t carousel;
		if ((ret_val = read_eeram(EERAM_CRSL, carousel.bytes, E_CRSL_SIZE))
				!= LS_OK)
			goto _EXIT;
		uint8_t n_slots_with_cards = 0;
		while (carousel.drum)
		{
			n_slots_with_cards += carousel.drum & 1;
			carousel.drum = carousel.drum >> 1;
		}

		// If we are not emptying and deck not cut, cut
		if (game_state.current_stage
				!= EMPTYING&& machine_state.cut == NOT_CUT_STATE)
		{
			uint8_t rdm;
			if (bounded_random(&rdm, n_slots_with_cards) != HAL_OK)
			{
				ret_val = TRNG_ERROR;
				goto _EXIT;
			}
			starting_position = (int8_t) rdm;
			// Update cut flag
			if ((write_flag(CUT_FLAG, CUT_STATE)) != LS_OK)
				goto _EXIT;

			// Make sure latch is closed
			set_latch(LATCH_CLOSED);

		}

		// Order (sequential) positions from starting position
		order_positions(targets, n_slots_with_cards, starting_position,
				ASCENDING_ORDER);
	}
	// Else if random mode, deal randomly with optimisation if game stage
	else if (rand_mode == RAND_MODE)
	{
		// Set random targets
		if ((ret_val = read_random_slots_w_offset(targets, FULL_SLOT)) != LS_OK)
			goto _EXIT;
		// If in a game phase, optimise order of distribution of first n_cards
		if (game_state.current_stage >= HOLE_CARDS
				&& game_state.current_stage <= PICK)
			order_positions(targets, n_cards, carousel_pos, ASCENDING_ORDER);
		// Make sure latch is closed
		set_latch(LATCH_CLOSED);
	}
// Error catch
	else
	{
		ret_val = INVALID_CHOICE;
		goto _EXIT;
	}

// Manage flap
	if (game_state.game_code == DEAL_N_CARDS)
		flap_open();
	else if (is_proper_game(game_state.game_code))
	{
		if (game_state.current_stage == HOLE_CARDS
				&& user_prefs.hole_cards_dest == DEST_TABLE)
			flap_open();
		else if (game_state.current_stage >= CC_1
				&& game_state.current_stage <= CC_5
				&& user_prefs.community_cards_dest == DEST_TABLE)
			flap_open();
		else if (game_state.current_stage == PICK)
			flap_open();
	}
	else
		flap_close();

	/* DISCHARGE n_cards */
	reset_btns();
	bool first_deal = false;
	uint8_t n_cards_in_initial = n_cards_in;
	bool paused = n_cards_in_initial > N_SLOTS ? false : true;
	clear_text();
	for (uint16_t i = 0; i < n_cards; i++)
	{
		// Manage ESC except in safe mode
		if (safe_mode != SAFE_MODE && (ret_val = safe_abort()) == LS_ESC)
			goto _EXIT;
		else if (ret_val == CONTINUE)
		{
			ret_val = LS_OK;
			first_deal = true;
		}

		// If double deck pause half-way
		if (machine_state.double_deck == DOUBLE_DECK_STATE && paused == false
				&& n_cards_in <= n_cards_in_initial / 2)
		{
			snprintf(display_buf, N_DISP_MAX, "Empty exit tray");
			flap_mid();
			beep(MEDIUM_BEEP);
			prompt_interface(MESSAGE, CUSTOM_MESSAGE, display_buf,
					icon_set_void, ICON_VOID, SHOE_CLEAR);
			HAL_Delay(M_WAIT_DELAY); // Time to pick up cards
			clear_message(TEXT_ERROR);
			flap_close();
			paused = true;
		}

		// Move
		if ((ret_val = go_to_position(targets[i])) != LS_OK)
			goto _EXIT;

		// Eject card
		ret_val = eject_one_card(safe_mode, rand_mode, NO_DEAL_GAP);

		// if OK update game state
		if (ret_val == LS_OK)
		{
			game_state.n_cards_dealt++;
			if ((ret_val = write_game_state()) != LS_OK)
				goto _EXIT;
		}
		// if not (except slot empty in safe mode) abort
		else if (!(ret_val == SLOT_IS_EMPTY && safe_mode == SAFE_MODE))
		{
			hide_n_cards_in();
			goto _EXIT;
		}

		// Update display of n_cards_in accordingly
		if (safe_mode == SAFE_MODE)
			hide_n_cards_in();
		else
			prompt_n_cards_in();

		// Prompt discharged cards (game_state is up to date)
		if (i == 0)
			first_deal = true;
		if (game_state.current_stage == HOLE_CARDS)
		{
			hole_change_t hole_change =
					(game_state.n_cards_dealt == 1) ? BOTH_CHANGE : CARD_CHANGE;

			// Prompting by player
			prompt_hole_cards(game_state.n_players_dealt + 1,
					game_state.n_players, game_state.n_cards_dealt,
					game_rules.n_hole_cards, hole_change, first_deal);
			first_deal = false;
		}
		else
			prompt_discharged_cards(game_rules, user_prefs);

	}

// EXIT
	_EXIT:

// Prompt
	clear_message(TEXT_ERROR);

// Manage latch
	bool do_close_latch = true;
// Don't close if card stuck
	if (ret_val == CARD_STUCK_ON_EXIT)
		do_close_latch = false;
// Also don't close if sequential and we are between hole cards and pick
	else if (rand_mode == SEQ_MODE)
	{
		if (game_state.current_stage >= HOLE_CARDS
				&& game_state.current_stage <= PICK)
			do_close_latch = false;
//		//... more hole cards dealt by players
//		if (game_state.current_stage == HOLE_CARDS
//				&& user_prefs.dist_mode == DIST_MODE_BY_PLAYER
//				&& game_state.n_players_dealt
//						< user_prefs.default_n_players - 1)
//			do_close_latch = false;
//
//		//... more hole cards dealt in round robin
//		else if (game_state.current_stage == HOLE_CARDS
//				&& user_prefs.dist_mode == DIST_MODE_ROUND_ROBIN
//				&& game_state.n_cards_dealt < game_rules.n_hole_cards)
//			do_close_latch = false;
//
//		//... more cc cards
//		else if (game_rules.n_cc_stages > 0 && game_state.current_stage >= CC_1
//				&& game_state.current_stage < CC_1 + game_rules.n_cc_stages - 1)
//			do_close_latch = false;
//
//		//... or picking cards
//		else if (game_state.current_stage == PICK)
//			do_close_latch = false;
	}

	if (do_close_latch)
		set_latch(LATCH_CLOSED);

// In safe mode, neutralise SLOT_IS_EMPTY and reset carousel contents
	if (safe_mode == SAFE_MODE
			&& (ret_val == SLOT_IS_EMPTY || ret_val == LS_OK))
	{
// Reset carousel map in memory
		if ((ret_val = reset_carousel()) != LS_OK)
			goto _EXIT_2;
// Reset n_cards_in
		n_cards_in = 0;
// Reset machine state
		if ((ret_val = reset_machine_state()) != LS_OK)
			goto _EXIT_2;
	}

// If  shuffler empty, reset machine state
	if (n_cards_in == 0)
		ret_val = reset_machine_state();

	_EXIT_2:

	return ret_val;
}

// Round robin
static return_code_t deal_round_robin(game_rules_t game_rules)
{
	return_code_t ret_val;
	int8_t target;
	int8_t targets[N_SLOTS];
	int8_t **deal_lists = NULL;
	uint8_t deal_size = game_rules.n_hole_cards - game_state.n_cards_dealt;
	const uint16_t n_pauses = 8;
	extern uint8_t n_cards_in;
	extern int8_t carousel_pos;

	// Check if there are pauses
	uint16_t pause_value(uint16_t pause_index)
	{
		uint16_t X = game_rules.hole_sequence;
		uint16_t Y = 0;
		uint16_t counter = 0;

		do
		{
			Y += (X & 0x03);
			X >>= 2;
			counter++;
		}
		while (X != 0 && counter <= pause_index);

		return Y;
	}

	bool is_pause(uint16_t N)
	{
		bool found = false;
		for (uint16_t i = 0; i < n_pauses; i++)
		{
			if (N == pause_value(i))
			{
				found = true;
				break;
			}
		}

		return found;
	}

// Make sure latch is closed
	set_latch(LATCH_CLOSED);

// Set up deal_lists, array of arrays holding players' remaining hands carousel positions
// Size: n_players x deal_size
	deal_lists = (int8_t**) malloc(game_state.n_players * sizeof(int8_t*)); // size of any pointer will do
	if (deal_lists == NULL)
	{
		ret_val = POINTER_ERROR;
		goto _EXIT;
	}
	for (uint16_t i = 0; i < game_state.n_players; i++)
	{
		deal_lists[i] = (int8_t*) malloc(deal_size * sizeof(int8_t));
		if (deal_lists[i] == NULL)
		{
			ret_val = POINTER_ERROR;
			goto _EXIT;
		}
	}

// In general the carousel is not in random state, randomise cards
	if (machine_state.random_in == SEQUENTIAL_STATE)
	{
		// Randomise cards - we will got from the end of the list targets[n_cards_in-1]
		if ((ret_val = read_random_slots_w_offset(targets, FULL_SLOT)) != LS_OK)
			goto _EXIT;
	}
// If carousel in random state, cut and sequential
	else
	{
		uint8_t rdm;
		bounded_random(&rdm, N_SLOTS);
		if ((ret_val = read_slots_w_offset(targets, FULL_SLOT)) != LS_OK)
			goto _EXIT;
		order_positions(targets, n_cards_in, (int8_t) rdm, DESCENDING_ORDER);
		write_flag(CUT_FLAG, CUT_STATE);
	}

	// Initialise deal_lists from the end, starting by the end of targets list
	// Invalid values (-1) will be at the end of each target list
	uint8_t counter = n_cards_in;
	for (uint16_t card = game_state.n_cards_dealt;
			card < game_rules.n_hole_cards; card++)
	{
		// Identify and manage current player (if mid-card)
		if (card == game_state.n_cards_dealt && game_state.n_cards_dealt != 0)
		{
			// Fill invalid values (-1) for current card's already dealt players
			for (uint16_t player = 0; player < game_state.n_players_dealt;
					player++)
				deal_lists[player][deal_size - 1] = -1;
		}

		// Fill targets for players' remaining hands (from the end of list)
		for (uint16_t player = game_state.n_players_dealt;
				player < game_state.n_players; player++)
			deal_lists[player][deal_size - (card - game_state.n_cards_dealt) - 1] =
					targets[--counter];
	}

	// Deal round robin (by card, then by player)
	uint16_t initial_card = game_state.n_cards_dealt;
	uint16_t initial_player = game_state.n_players_dealt;
	uint16_t updated_n_players = game_state.n_players;
	for (uint16_t card = initial_card; card < game_rules.n_hole_cards; card++)
	{
		// Calculate n_cards_remaining at this stage for undealt players
		uint16_t n_cards_remaining = game_rules.n_hole_cards - card;

		// Deal remaining players
		bool first_deal;
		for (uint16_t player = initial_player; player < updated_n_players;
				player++)
		{
			// Prompt
			hole_change_t hole_change =
					(player == 0) ? BOTH_CHANGE : PLAYER_CHANGE;

			// Manage first deal
			if ((card == initial_card) && (player == initial_player))
				first_deal = true;

			// If there are sequences and we have to pause, pause and update number of players
			if (game_rules.stud_structure && player == 0 && is_pause(card))
			{
				// Display icons
				display_encoder_icon(ICON_CHECK);
				display_escape_icon(ICON_RED_CROSS);

				// Get number of players
				clear_message(TEXT_ERROR);
				uint16_t NP = updated_n_players;
				return_code_t user_input = get_n_x(&NP, 2, updated_n_players,
						"", "remaining player");

				if (user_input == LS_OK)
				{
					// Display icons
					display_encoder_icon(ICON_VOID);
					display_escape_icon(ICON_CROSS);
					updated_n_players = NP;
				}

				// Manage prompt display
				first_deal = true;
			}

			// Manage ESC = direct exit if pauses and ESC in later hole cards
			if (game_rules.stud_structure && card >= pause_value(0))
			{
				extern button escape_btn;
				if (escape_btn.interrupt_press)
				{
					clear_message(TEXT_ERROR);
					ret_val = LS_OK; // To abort hole sequence but not full game
					goto _EXIT;
				}
			}
			else
			{
				if ((ret_val = safe_abort()) == LS_ESC)
					goto _EXIT;
				else if (ret_val == CONTINUE)
				{
					ret_val = LS_OK;
					first_deal = true;
				}
			}

			prompt_hole_cards(player + 1, updated_n_players, card + 1,
					game_rules.n_hole_cards, hole_change, first_deal);
			first_deal = false;

			// Order targets of current player based on carousel position
			// (We order only the undealt players, the initial -1 will disappear next time)
			order_positions(deal_lists[player], n_cards_remaining, carousel_pos,
					DESCENDING_ORDER);
			// Take the last one as target
			target = deal_lists[player][n_cards_remaining - 1];

			// MOVE
			if ((ret_val = go_to_position(target)) != LS_OK)
				goto _EXIT;

			// EJECT
			if ((ret_val = eject_one_card(NON_SAFE_MODE,
					machine_state.random_in == SEQUENTIAL_STATE ?
							RAND_MODE : SEQ_MODE,
					cards_in_shoe() ? NO_DEAL_GAP : DEAL_GAP)) != LS_OK)
				goto _EXIT;

			// Update game state
			game_state.n_players_dealt++;
			if ((ret_val = write_game_state()) != LS_OK)
				goto _EXIT;

			// prompt n_cards_in
			prompt_n_cards_in();
		}

		// Update game state
		game_state.n_cards_dealt++;
		game_state.n_players_dealt = 0;
		initial_player = 0;
		if ((ret_val = write_game_state()) != LS_OK)
			goto _EXIT;
	}

	wait_pickup_shoe(0);

	_EXIT:

// Free pointers
	for (uint16_t i = 0; i < game_state.n_players; i++)
	{
		free(deal_lists[i]);
		deal_lists[i] = NULL;
	}
	free(deal_lists);
	deal_lists = NULL;

	return ret_val;

}

// Hole cards distribution
return_code_t deal_hole_cards(game_rules_t game_rules, user_prefs_t user_prefs)
{
	return_code_t ret_val;
	extern uint8_t n_cards_in;
	extern int8_t carousel_pos;
	extern icon_set_t icon_set_void;

// Get machine state
	if ((ret_val = read_machine_state()) != LS_OK)
		goto _EXIT;

// Override distribution mode to round robin if hole sequences
	if (game_rules.stud_structure)
		user_prefs.dist_mode = DIST_MODE_ROUND_ROBIN;

//// Override hole cards destination if round robin (always on table)
//	if (user_prefs.dist_mode == DIST_MODE_ROUND_ROBIN)
//		user_prefs.hole_cards_dest = DEST_TABLE;

	// Save user preferences
	if ((ret_val = write_user_prefs(&user_prefs, game_rules.game_code))
			!= LS_OK) // NECESSARY?
		goto _EXIT;

// Manage flap
	if (user_prefs.hole_cards_dest == DEST_INTERNAL_TRAY)
		flap_close();
	else
		flap_open();

// Select distribution mode
	if (user_prefs.dist_mode == DIST_MODE_BY_PLAYER)
	{
		//Update initial player
		uint16_t initial_player = game_state.n_players_dealt;

		// Finish dealing cards
		reset_btns();
		for (uint16_t player = initial_player; player < game_state.n_players;
				player++)
		{
			if ((ret_val = discharge_cards(game_rules)) != LS_OK)
				goto _EXIT;

			// Wait for pick up or mark a pause
			if (user_prefs.hole_cards_dest == DEST_INTERNAL_TRAY)
				wait_pickup_shoe(game_state.n_cards_dealt);
			else
			{
				extern gen_prefs_t gen_prefs;
				HAL_Delay(
						(uint32_t) gen_prefs.deal_gap_index
								* DEAL_GAP_MULTIPLIER);
			}
			// Update game state
			game_state.n_players_dealt++;
			game_state.n_cards_dealt = 0;
			if ((ret_val = write_game_state()) != LS_OK)
				goto _EXIT;

		}
	}
	else if (user_prefs.dist_mode == DIST_MODE_ROUND_ROBIN)
		ret_val = deal_round_robin(game_rules);

	_EXIT:

	return ret_val;

}

// Shuffle
return_code_t shuffle(uint16_t *pNloaded)
{
	const bool show_time = false;
	extern uint8_t n_cards_in;
	extern icon_set_t icon_set_void;
	return_code_t ret_val;
	uint8_t n_deck;             // working n_deck
	uint8_t n_deck_ref; // initial (and maybe final) n_deck, used for comparison
	bool excess_on_start = false;
	uint32_t start_time;
	uint32_t elapsed_time;
	bool right_first_time = true;

// Get default deck
	if ((ret_val = read_default_n_deck(&n_deck_ref)) != LS_OK)
		goto _EXIT;
	n_deck = n_deck_ref;

	_RESTART:

// Get machine state and game state
	if ((ret_val = read_machine_state()) != LS_OK || (ret_val =
			read_game_state()) != LS_OK)
		goto _EXIT;

// Update game_state
// CHANGE HERE IF INTERRUPTED GAME
	game_state.game_code = SHUFFLE;
	game_state.current_stage = NO_GAME_RUNNING;
	if ((ret_val = write_game_state()) != LS_OK)
		goto _EXIT;

	if (show_time)
	{
		// Start timer
		start_time = HAL_GetTick();
		elapsed_time = 0;
	}

	right_first_time = true;
	*pNloaded = 0;

// If there are more cards inside already than Ndck, adjust n_deck to N_SLOTS to enter [while] loop
	if (n_cards_in > n_deck)
	{
		n_deck = N_SLOTS;
		excess_on_start = true;
	}
// From here, n_cards_in < n_deck or n_cards_in == n_deck == N_SLOTS

// First stage: random loading until full deck or ESC (unless sequential contents => sequential loading)
// Update game_state
	game_state.current_stage = LOADING;
	if ((ret_val = write_game_state()) != LS_OK)
		goto _EXIT;
	while (n_cards_in < n_deck)
	{
		extern icon_set_t icon_set_check;

		// Load carousel
		if (!right_first_time || cards_in_tray() || n_cards_in == 0) // no load if tray empty and cards in at start
			if ((ret_val = load_carousel(machine_state.random_in, pNloaded))
					!= LS_OK)
				goto _EXIT;
		// Check # of cards in deck after loading is n_deck, adjust otherwise
		if (n_cards_in != n_deck)
		{
			right_first_time = false;
			// If more cards inside already, adjust n_deck to N_SLOTS to keep in [while] loop
			if (n_cards_in > n_deck)
				n_deck = N_SLOTS;
			// In any case (if more than one card in) ask for confirmation
			if (n_cards_in > 1)
			{
				warn_n_cards_in();
				snprintf(display_buf, N_DISP_MAX, "\n%d cards in, OK?   ",
						n_cards_in);
				clear_message(TEXT_ERROR);
				return_code_t user_input = prompt_interface(MESSAGE,
						CUSTOM_MESSAGE, display_buf, icon_set_check, ICON_CROSS,
						TRAY_LOAD);

				// If cards in tray, load them (keep in loop)
				if (cards_in_tray())
					;
				// Else if OK update n_deck, this will exit the [while] loop
				else if (user_input == LS_OK)
				{
					n_deck = n_cards_in;
					if ((ret_val = write_default_n_deck(n_deck)) != LS_OK)
						goto _EXIT;
					n_deck_ref = n_deck;
					// if ESC, exit with various return values depending on carousel and tray content
				}
				else if (user_input == LS_ESC)
				{
					if (n_cards_in == N_SLOTS && cards_in_tray())
						ret_val = SFCIT;
					else
						ret_val =
								(n_cards_in > n_deck_ref || excess_on_start) ?
										DO_EMPTY : LS_ESC;
					goto _EXIT;
				}

			}
			else if (n_cards_in == 1)
			{
				snprintf(display_buf, N_DISP_MAX, "\nOnly one card in");
				warn_n_cards_in();
				clear_message(TEXT_ERROR);
				prompt_interface(MESSAGE, CUSTOM_MESSAGE, display_buf,
						icon_set_check, ICON_VOID, BUTTON_PRESS);
				ret_val = LS_ESC;
				goto _EXIT;
			}
		}
	}
	/* [while] loop analysis
	 *
	 * if   n_cards_in == n_deck        =>  moves to next stage
	 * elif n_cards_in > 1 and ENTER    =>  moves to next stage
	 * elif n_cards_in == N_SLOTS       =>  EXIT => SFCIT
	 * elif ESC                         =>  EXIT => ESC
	 * elif n_cards_in < n_deck         =>  awaits OK (except if 1 card only), load or ESC
	 * elif n_cards_in > n_deck         =>  awaits OK, load (resets n_deck to N_SLOTS) or ESC
	 *
	 * */

// if cards in shoe, wait for removal (option to ESC)
	extern gen_prefs_t gen_prefs;
	if (gen_prefs.cut_card_flag == false)
		wait_pickup_shoe_or_ESC();

	/*
	 * Second stage: cut and sequential empty unless random content was broken
	 */

// Update game state
	game_state.current_stage = SHUFFLING;
	game_state.n_cards_dealt = 0;
	if ((ret_val = write_game_state()) != LS_OK)
		goto _EXIT;

// Discharge cards
	extern game_rules_t void_game_rules;
	ret_val = discharge_cards(void_game_rules);
	if (ret_val != CARD_STUCK_ON_EXIT)
		set_latch(LATCH_CLOSED);
	if (ret_val != LS_OK)
		goto _EXIT;

	if (show_time && right_first_time)
	{
		// Get time
		elapsed_time = (HAL_GetTick() - start_time) / 1000UL;
		snprintf(display_buf, N_DISP_MAX, "\nShuffled %d cards in %lu:%02lu",
				game_state.n_cards_dealt, elapsed_time / 60UL,
				elapsed_time % 60UL);
	}
	else
		snprintf(display_buf, N_DISP_MAX, "\nShuffled %d cards",
				game_state.n_cards_dealt);

// Present cards
	flap_mid();

// Prompt (if not about to reload)
	if (!cards_in_tray())
		prompt_interface(MESSAGE, CUSTOM_PICK_UP, display_buf, icon_set_void,
				ICON_CROSS, SHOE_CLEAR_OR_TRAY_LOAD);

// If cards have been picked up
	if (!cards_in_shoe())
	{
		HAL_Delay(SHOE_DELAY);
		// Clears icons, message and picture associated with prompt when not managed by prompt dynamic buttons
		// because of custom pick up
		clear_message(GRAPHIC_ERROR);
		clear_icons();
		clear_picture();
		flap_close();
	}
// If load, restart, else exit
	if (cards_in_tray())
	{
		clear_message(GRAPHIC_ERROR);
		clear_icons();
		clear_picture();
	}
	goto _RESTART;

	_EXIT:

// Update game_state
	game_state.current_stage = NO_GAME_RUNNING;
	return_code_t secondary_ret_val;
	if ((secondary_ret_val = write_game_state()) != LS_OK)
		ret_val = secondary_ret_val;

	return ret_val;
}  // shuffle()

/*GET NUMBERS OF "label" between min_n_x and max_n_x */
return_code_t get_n_x(uint16_t *p_nx, uint16_t min_n_x, uint16_t max_n_x,
		const char *label_1, const char *label_2)
{
	extern button encoder_btn;
	extern button escape_btn;
	return_code_t ret_val = LS_OK;
	uint16_t NX = *p_nx;
	int16_t increment;
	const uint16_t number_width = 30;
	uint16_t label_1_width;
	uint16_t label_2_width;
	bool change = false;
	bool tray_empty_at_start = !cards_in_tray();
	bool plural = (NX > 1);

	label_1_width = calculate_width(label_1, LCD_BOLD_FONT);
	label_2_width = calculate_width(label_2, LCD_BOLD_FONT);

// Consistency check
	if (min_n_x > max_n_x)
	{
		ret_val = INVALID_CHOICE;
		goto _EXIT;
	}

// Prompt labels and icons
	prompt_labels(label_1, label_2, NX, number_width);
	display_encoder_icon(ICON_CHECK);
	display_escape_icon(ICON_CROSS);

// Calculate centering margin
	uint16_t x_pos = (BSP_LCD_GetXSize()
			- (label_1_width + PRE_NUMBER_SPACE + number_width
					+ POST_NUMBER_SPACE + label_2_width)) / 2;
// Derive x_pos
	x_pos += label_1_width + PRE_NUMBER_SPACE;

// Show number in red
	show_num(NX, LCD_COLOR_RED_SHFLR, x_pos, number_width);

	reset_btns();
	while (!escape_btn.interrupt_press && !encoder_btn.interrupt_press
			&& !(tray_empty_at_start && cards_in_tray()))
	{
		if ((increment = read_encoder(CLK_WISE)))
		{
			// Detect CW turn of encoder
			if (increment > 0 && NX < max_n_x)
			{
				NX = min(max_n_x, NX + (uint16_t ) increment);
				change = true;
			}
			// Detect CCW turn of encoder
			else if (increment < 0 && NX > min_n_x)
			{
				increment = max(increment, (int16_t ) min_n_x - (int16_t ) NX);
				NX -= (uint16_t) (-increment);
				change = true;
			}
		}
		if (change)
		{
			// Update labels if necessary
			if (plural != (NX > 1))
			{
				prompt_labels(label_1, label_2, NX, number_width);
				plural = (NX > 1);
			}
			// Show number in red
			show_num(NX, LCD_COLOR_RED_SHFLR, x_pos, number_width);
			change = false;
		}
		update_btns();
	}

// If escape button press, return ESC without saving

	if (escape_btn.interrupt_press)
		ret_val = LS_ESC;

// Otherwise save and return OK
	else
	{
		*p_nx = (uint8_t) NX;
// Show final value in white
		show_num(*p_nx, LCD_COLOR_TEXT, x_pos, number_width);

		HAL_Delay(S_WAIT_DELAY);
		ret_val = LS_OK;
	}

	_EXIT:

	clear_icons();

	return ret_val;
}

return_code_t get_n_players(game_rules_t game_rules, user_prefs_t user_prefs)
{
	return_code_t ret_val = LS_OK;
	const char *label = "player";
// Get default n players
	uint16_t n_players = user_prefs.default_n_players;

	if (game_rules.min_players == game_rules.max_players)
		game_state.n_players = game_rules.min_players;
	else
	{
		clear_message(TEXT_ERROR);
		// Get number of players, jump if ESCAPE
		if ((ret_val = get_n_x(&n_players, game_rules.min_players,
				game_rules.max_players, "", label)) != LS_OK)
			goto _EXIT;

		// If ENTER save current n_players value as current
		game_state.n_players = n_players;
	}

	_EXIT:

	clear_message(TEXT_ERROR);

	return ret_val;
}

// Get number of cards to pick
// Returns CONTINUE, DO_RECYCLE (only if RECYCLE_ON) or LS_ESC
return_code_t get_n_pick_or_deal(game_rules_t game_rules, uint16_t *p_n_cards)
{
	extern button encoder_btn;
	extern button escape_btn;
	return_code_t ret_val = CONTINUE;
	uint16_t n_cards = *p_n_cards;
	int16_t increment;
	uint16_t number_width = 25;
	bool change = false;
	bool plural = (n_cards > 1);
	const uint16_t soft_card_limit = N_SLOTS / 2; // max n_hole_cards in game_rules
	char label_1[15];
	char *label_2 = "card";
	extern uint8_t n_cards_in;

// Set up max cards that can be picked/dealt
	uint16_t max_n_cards = (
			game_state.current_stage == PICK ?
					min(game_rules.max_pick, n_cards_in) : soft_card_limit);

// Set up text according to game stage being played and calculate num_x_pos
	snprintf(label_1, 15, game_state.current_stage == PICK ? "Pick" : "Deal");

	uint16_t label_1_width = calculate_width(label_1, LCD_REGULAR_FONT);
	uint16_t label_2_width = calculate_width(label_2, LCD_REGULAR_FONT);

// Calculate centering margin
	uint16_t x_pos = (BSP_LCD_GetXSize()
			- (label_1_width + PRE_NUMBER_SPACE + number_width
					+ POST_NUMBER_SPACE + label_2_width)) / 2;
// Derive x_pos
	x_pos += label_1_width + PRE_NUMBER_SPACE;

// Prompt labels
	clear_message(TEXT_ERROR);
	prompt_labels(label_1, label_2, n_cards, number_width);

// Display icons
	display_encoder_icon(ICON_CARD);
	display_escape_icon(ICON_RED_CROSS);
// Show number
	show_num(n_cards, LCD_COLOR_RED_SHFLR, x_pos, number_width);

	reset_btns();
	while (!escape_btn.interrupt_press && !encoder_btn.interrupt_press)
	{
		// Trigger recycling if necessary
		extern uint8_t n_cards_in;
		if (game_rules.recycle_mode == RECYCLE_ON
				&& (n_cards_in == 0 || cards_in_tray()))
		{
			ret_val = DO_RECYCLE;
			break;
		}
		if ((increment = read_encoder(CLK_WISE)))
		{
			// Detect CW turn of encoder
			if (increment > 0 && n_cards < min(max_n_cards, n_cards_in))
			{
				n_cards = min(min(max_n_cards, n_cards + (uint16_t ) increment),
						n_cards_in);
				change = true;
			}
			// Detect CCW turn of encoder
			else if (increment < 0 && n_cards > 1)
			{
				increment = max(increment, 1 - (int16_t ) n_cards);
				n_cards -= (uint16_t) (-increment);
				change = true;
			}
		}
		if (change)
		{
			// Update labels if necessary
			if (plural != (n_cards > 1))
			{
				prompt_labels(label_1, label_2, n_cards, number_width);
				plural = (n_cards > 1);
			}
			// Show number
			show_num(n_cards, LCD_COLOR_RED_SHFLR, x_pos, number_width);

			change = false;
		}
		update_btns();
	}

	// If recycling was not triggered
	if (ret_val != DO_RECYCLE)
	{
		// If escape button press, return ESC without saving
		if (escape_btn.interrupt_press)
			ret_val = LS_ESC;
		// Else save and return OK
		else
		{
			*p_n_cards = (uint8_t) n_cards;
			// Show number in white
			show_num(n_cards, LCD_COLOR_TEXT, x_pos, number_width);
			ret_val = CONTINUE;
		}
	}

	clear_message(TEXT_ERROR);

	return ret_val;
}

return_code_t adjust_deal_gap(void)
{
	return_code_t ret_val = LS_OK;
	extern gen_prefs_t gen_prefs;
	const char *label = "1/10s between hole card";
	uint16_t dd;
	const uint16_t min_dd = 0;
	const uint16_t max_dd = 15;

	// Start with current deal gap index (16 bit for get_n_x)
	dd = (uint16_t) gen_prefs.deal_gap_index;

	clear_message(TEXT_ERROR);
	// Get updated deal gap index, jump if ESCAPE
	if ((ret_val = get_n_x(&dd, min_dd, max_dd, "", label)) != LS_OK)
		goto _EXIT;

	// Save dd as new default
	ret_val = write_deal_gap_index((uint8_t) dd);

	_EXIT:

	clear_message(TEXT_ERROR);

	return ret_val;
}

return_code_t set_cut_card_flag(void)
{
	return_code_t user_input;
	extern icon_set_t icon_set_check;

	user_input = prompt_interface(MESSAGE, CUSTOM_MESSAGE,
			"Use cut card to shuffle?", icon_set_check, ICON_CROSS,
			BUTTON_PRESS);

	return write_cut_card_flag(user_input == LS_OK);
}

// Get rules
return_code_t get_rules(game_rules_t *p_game_rules, item_code_t game_code)
{
	return_code_t ret_val = INVALID_CHOICE;
//	NOT RESTRICTED TO PROPER GAMES, JUST TO GAME_LIST
//	if (is_proper_game(game_code))
//	{
// If preset read from memory (no preset after CUSTOM_1 - 1)
	if (game_code < CUSTOM_1)
	{
		bool found = false;
		for (uint16_t i = 0; i < n_presets; i++)
			if (game_code == rules_list[i].game_code)
			{
				found = true;
				for (uint16_t j = 0; j < E_GAME_RULES_SIZE; j++)
					p_game_rules->bytes[j] = rules_list[i].bytes[j];
				break;
			}

		// If found in preset list, OK
		if (found)
			ret_val = LS_OK;
	}
// Else read from EERAM
	else
		ret_val = read_custom_game_rule(p_game_rules, game_code);

// Set game code
	p_game_rules->game_code = game_code;
//	}
//
//// If not proper game return INVALID_CHOICE

	return ret_val;
}

// run_game gets parameters from current game_state
return_code_t run_game(void)
{

	extern context_t context;
	return_code_t ret_val;
	return_code_t local_ret_val; //  not to shadow by main ret_val
	game_rules_t game_rules;
	user_prefs_t user_prefs;

// Get machine state
	if ((ret_val = read_machine_state()) != LS_OK)
		goto _EXIT;

// Get rules
	if ((ret_val = get_rules(&game_rules, game_state.game_code)) != LS_OK)
		goto _EXIT;

// Get user prefs
	if ((ret_val = read_user_prefs(&user_prefs, game_state.game_code)) != LS_OK)
		goto _EXIT;

// Loop through stages starting with the current one until END_GAME
// game_state not saved this time as composite, will be saved after each stage thereafter
// Introducing run_stage ret_val not to be shadowed
	while (game_state.current_stage != END_GAME)
	{
		if ((ret_val = run_stage(game_rules, &user_prefs)) == LS_OK)
		{
			// If initialising, save n players as defaults
			if (game_state.current_stage == LOADING)
			{
				user_prefs.default_n_players = game_state.n_players;
				if ((local_ret_val = write_user_prefs(&user_prefs,
						user_prefs.game_code)) != LS_OK)
				{
					ret_val = local_ret_val;
					goto _EXIT;
				}
			}
		}
		else
			break;
		// Save game_state
		if ((local_ret_val = write_game_state()) != LS_OK)
		{
			ret_val = local_ret_val;
			goto _EXIT;
		}
	}

// If end reached, reset
	if (game_state.current_stage == END_GAME || ret_val == LS_ESC)
		game_state.current_stage = NO_GAME_RUNNING;

	_EXIT:

	if ((local_ret_val = write_game_state()) != LS_OK)
		ret_val = local_ret_val;

	if (context == CONTEXT_DEALERS_CHOICE)
		ret_val = LS_ESC;

// Make sure latch is closed
	set_latch(LATCH_CLOSED);

	return ret_val;
}

static return_code_t process_cc_after(game_rules_t game_rules,
		user_prefs_t *p_user_prefs)
{
	return_code_t ret_val;
	return_code_t user_input;
	uint32_t burn_delay = 700;
	icon_set_t encoder_context;
	extern icon_set_t icon_set_A;
	extern icon_set_t icon_set_B;
	extern icon_set_t icon_set_C;
	extern icon_set_t icon_set_D;
	extern icon_set_t icon_set_E;
	extern icon_set_t icon_set_card;

// Get encoder context
// (offer burn cards only if CC are dealt with open flap)
	if (p_user_prefs->community_cards_dest == DEST_TABLE)
	{
		if (game_rules.allows_burn_cards && game_rules.allows_xtra_cards)
		{
			if (p_user_prefs->burn_cards)
				encoder_context = icon_set_B;
			else
				encoder_context = icon_set_A;
		}
		else if (game_rules.allows_burn_cards && !game_rules.allows_xtra_cards)
		{
			if (p_user_prefs->burn_cards)
				encoder_context = icon_set_D;
			else
				encoder_context = icon_set_C;
		}
		else if (!game_rules.allows_burn_cards && game_rules.allows_xtra_cards)
			encoder_context = icon_set_E;
		else
			encoder_context = icon_set_card;
	}
	else
	{
		if (game_rules.allows_xtra_cards)
			encoder_context = icon_set_E;
		else
			encoder_context = icon_set_card;
		flap_close();
	}

// Prompt interface
	snprintf(display_buf, N_DISP_MAX, "\n%s?",
			game_rules.cc_stages[game_state.current_stage - CC_1].name);
	user_input = prompt_interface(MESSAGE, CUSTOM_MESSAGE, display_buf,
			encoder_context, ICON_RED_CROSS, BUTTON_PRESS);

// Process user input
// Manage game stage
	if (user_input == DRAW_CARD || user_input == BURN_CARD)
	{
		// Deal a burn card at random if requested
		if (user_input == BURN_CARD)
		{
			// Prompt
			clear_message(TEXT_ERROR);
			prompt_message("\nBurn card");
			// Because selected at random, need to close latch
			set_latch(LATCH_CLOSED);
			// Manage flap
			if (!cards_in_tray())
				flap_open();
			// Eject a card at random
			int8_t targets[N_SLOTS];
			if ((ret_val = read_random_slots_w_offset(targets, FULL_SLOT))
					!= LS_OK || (ret_val = go_to_position(targets[0])) != LS_OK
					|| (ret_val = eject_one_card(NON_SAFE_MODE, RAND_MODE,
					NO_DEAL_GAP)) != LS_OK)
				goto _EXIT;
			// Update user prefs
			p_user_prefs->burn_cards = true;
			// Game has been cut via the burn card
			if ((ret_val = write_flag(CUT_FLAG, CUT_STATE)) != LS_OK)
				goto _EXIT;
			// Little delay before dealing CCs
			HAL_Delay(burn_delay);
			// If cc in internal tray, close flap
			if (p_user_prefs->community_cards_dest == DEST_INTERNAL_TRAY)
				flap_close();
			// Little delay before dealing CCs
			HAL_Delay(burn_delay);
		}
		else
			// Update user prefs
			p_user_prefs->burn_cards = false;
		// Save user prefs
		if ((ret_val = write_user_prefs(p_user_prefs, p_user_prefs->game_code))
				!= LS_OK)
			goto _EXIT;
		// Display CC name without "?"
		snprintf(display_buf, N_DISP_MAX, "\n%s",
				game_rules.cc_stages[game_state.current_stage - CC_1].name);
		clear_message(TEXT_ERROR);
		prompt_message(display_buf);
		// Deal CCs
		if ((ret_val = discharge_cards(game_rules)) != LS_OK)
			return ret_val;

		// If cc to internal tray, wait for pick up
		if (p_user_prefs->community_cards_dest == DEST_INTERNAL_TRAY)
		{
			HAL_Delay(100); // Time to see the card
			wait_pickup_shoe(game_state.n_cards_dealt);
		}

		// Select next stage (we are after hole cards)
		if (game_state.current_stage + 1 - CC_1 < game_rules.n_cc_stages)
			game_state.current_stage++;
		else if (game_rules.max_pick != 0)
			game_state.current_stage = PICK;
		else if (p_user_prefs->empty_machine_after)
			game_state.current_stage = EMPTYING;
		else
			game_state.current_stage = END_GAME;
	}
// If just an extra card, deal card and no update to game state or preferences
	else if (user_input == EXTRA_CARD)
	{
		// Deal an extra card at random if requested
		clear_message(TEXT_ERROR);
		prompt_message("\nExtra card");
		int8_t targets[N_SLOTS];
		// Manage flap
		if (!cards_in_tray())
			flap_open();
		if ((ret_val = read_random_slots_w_offset(targets, FULL_SLOT)) != LS_OK
				|| (ret_val = go_to_position(targets[0])) != LS_OK || (ret_val =
						eject_one_card(NON_SAFE_MODE, RAND_MODE,
						NO_DEAL_GAP)) != LS_OK)
			return ret_val;

		// If cc in internal tray, close flap
		if (p_user_prefs->community_cards_dest == DEST_INTERNAL_TRAY)
			flap_close();

	}
	else if (user_input == LS_ESC)
	{
		// If escape community cards, either pick, empty or end game (no recycle if empty_after)
		if (game_rules.max_pick > 0)
			game_state.current_stage = PICK;
		else if (p_user_prefs->empty_machine_after)
			game_state.current_stage = EMPTYING;
		else
			game_state.current_stage = END_GAME;
		// Normalise ESC
		ret_val = LS_OK;
	}
// Error catch
	else
		ret_val = INVALID_CHOICE;

// Reset n cards and n players dealt
	game_state.n_cards_dealt = 0;
	game_state.n_players_dealt = 0;

	_EXIT:

	return ret_val;
}

/* return values
 * all EERAM errors
 * LS_ESC (init_game_state)
 *
 */
// Game state must be updated (or relevant) before each run_stage
return_code_t run_stage(game_rules_t game_rules, user_prefs_t *p_user_prefs)
{
	return_code_t ret_val = LS_OK;
	extern icon_set_t icon_set_check;
	extern icon_set_t icon_set_void;

// Manage CC
	if (game_state.current_stage >= CC_1 && game_state.current_stage <= CC_5)
	{
		// Discharge cards depending on timing
		if (p_user_prefs->community_timing == COMMUNITY_TIMING_AFTER_HOLES)
		{
			if ((ret_val = process_cc_after(game_rules, p_user_prefs)) != LS_OK)
				goto _EXIT;
		}
		else
		{
			// If all community cards before, generic name unless there is only one stage
			clear_message(TEXT_ERROR);
			extern char display_buf[];
			snprintf (display_buf, N_DISP_MAX, "\n%s", game_rules.n_cc_stages == 1 ?
					game_rules.cc_stages[0].name : "Community cards.");
			prompt_message(display_buf);
			HAL_Delay(M_WAIT_DELAY);
			if ((ret_val = discharge_cards(game_rules)) != LS_OK)
				goto _EXIT;
			// Manage pick up if needed
			if (p_user_prefs->community_cards_dest == DEST_INTERNAL_TRAY)
			{
				//If only one CC, extra time for the card to land
				uint16_t n_cards = 0;
				for (uint16_t i = 0; i < game_rules.n_cc_stages; i++)
					n_cards += game_rules.cc_stages[i].n_cards;
				if (n_cards == 1)
					HAL_Delay(100);
				wait_pickup_shoe(game_state.n_cards_dealt);
			}
			// Else pause before dealing hole cards if they are also on table
			else if (p_user_prefs->hole_cards_dest == DEST_TABLE)
				HAL_Delay(L_WAIT_DELAY);
			// Reset n cards dealt
			game_state.n_cards_dealt = 0;
			// Update game stage
			if (game_rules.random_player)
				game_state.current_stage = RANDOM_PLAYER;
			else
				game_state.current_stage = HOLE_CARDS;
		}
	}
	else
		switch (game_state.current_stage)
		{
			case NO_GAME_RUNNING:
				if ((ret_val = init_game_state(game_rules, *p_user_prefs))
						!= LS_OK)
					goto _EXIT;
				break;

			case LOADING:
				if ((ret_val = game_load(game_rules, *p_user_prefs)) != LS_OK)
					goto _EXIT;
				if (game_rules.n_cc_stages != 0
						&& p_user_prefs->community_timing
								== COMMUNITY_TIMING_BEFORE_HOLES)
					game_state.current_stage = CC_1;
				else if (game_rules.random_player)
					game_state.current_stage = RANDOM_PLAYER;
				else
					game_state.current_stage = HOLE_CARDS;
				break;

			case RANDOM_PLAYER:
				if ((ret_val = random_player(game_state.n_players)) != LS_OK)
					goto _EXIT;
				game_state.current_stage = HOLE_CARDS;
				break;

			case HOLE_CARDS:
				if ((ret_val = deal_hole_cards(game_rules, *p_user_prefs))
						!= LS_OK)
					goto _EXIT;
				if (game_rules.n_cc_stages != 0
						&& p_user_prefs->community_timing
								== COMMUNITY_TIMING_AFTER_HOLES)
					game_state.current_stage = CC_1;
				else if (game_rules.max_pick != 0)
					game_state.current_stage = PICK;
				else if (p_user_prefs->empty_machine_after)
					game_state.current_stage = EMPTYING;
				else
					game_state.current_stage = END_GAME;
				// Reset n cards and n players dealt
				game_state.n_cards_dealt = 0;
				game_state.n_players_dealt = 0;
				break;

			case EMPTYING:
				clear_message(TEXT_ERROR);
				flap_close();
				if ((ret_val = discharge_cards(game_rules)) != LS_OK)
					goto _EXIT;
				game_state.current_stage = END_GAME;
				wait_pickup_shoe_or_ESC();
				break;

			case PICK:
				extern uint8_t n_cards_in;
				return_code_t user_input;
				uint16_t n_pick = min(min(game_rules.max_pick, n_cards_in),
						max(game_state.n_pick, 1));
				while (n_cards_in)
				{
					// Reset n cards dealt
					game_state.n_cards_dealt = 0;

					// Get number of cards to pick and stay in PICK stage if check
					n_pick = min(n_pick, n_cards_in);
					if ((user_input = get_n_pick_or_deal(game_rules, &n_pick))
							== CONTINUE)
					{
						// Keep memory of last pick
						game_state.n_pick = n_pick;
						if ((ret_val = write_game_state()) != LS_OK)
							goto _EXIT;
						// Discharge cards accordingly
						discharge_cards(game_rules);
					}
					else
						break; // while
				}
				if (n_cards_in == 0)
				{
					if (game_rules.recycle_mode == RECYCLE_ON)
						game_state.current_stage = RECYCLE;
					else
						game_state.current_stage = END_GAME;
				}
				else if (user_input == LS_ESC)
				{

					if (p_user_prefs->empty_machine_after == true)
						game_state.current_stage = EMPTYING;
					else
						game_state.current_stage = END_GAME;
				}
				else if (user_input == DO_RECYCLE)
					game_state.current_stage = RECYCLE;

				if ((ret_val = write_game_state()) != LS_OK)
					goto _EXIT;

				break;

			case RECYCLE:
				user_input = LS_OK;
				return_code_t local_ret_val;
				if (!cards_in_tray())
					user_input = prompt_interface(MESSAGE, CUSTOM_MESSAGE,
							"\nRecycle cards?", icon_set_void, ICON_RED_CROSS,
							TRAY_LOAD);
				if (user_input == LS_OK)
				{
					uint16_t n_cards_loaded;

					// Reset content if carousel empty
					extern uint8_t n_cards_in;
					if (n_cards_in == 0)
					{
						machine_state.random_in = RANDOM_STATE;
						machine_state.seq_dschg = RANDOM_STATE;
						if ((local_ret_val = write_machine_state()) != LS_OK)
						{
							ret_val = local_ret_val;
							goto _EXIT;
						}

					}

					while ((ret_val = load_carousel(
							machine_state.random_in == RANDOM_STATE ?
									RAND_MODE : SEQ_MODE, &n_cards_loaded))
							!= LS_OK && ret_val != LS_ESC)
						;
					if (ret_val == LS_ESC)
						game_state.current_stage = END_GAME;
					else
						game_state.current_stage = PICK;
				}
				else
				{
					game_state.current_stage = END_GAME;
					ret_val = LS_ESC;
				}
				clear_message(TEXT_ERROR);
				break;

			default:
				ret_val = INVALID_CHOICE;
		}

	_EXIT:

	return ret_val;

}

return_code_t init_game_state(game_rules_t game_rules, user_prefs_t user_prefs)
{
	return_code_t ret_val = LS_OK;
	game_state.n_cards_dealt = 0;
	game_state.n_players_dealt = 0;

// Set number of players, ESC aborts
	if ((ret_val = get_n_players(game_rules, user_prefs)) != LS_OK)
		goto _EXIT;

// Next stage is LOADING
	game_state.current_stage = LOADING;

	_EXIT:

	return ret_val;

}

return_code_t game_load(game_rules_t game_rules, user_prefs_t user_prefs)
{
	return_code_t ret_val;
	uint16_t n_cards = 0;
	rand_mode_t rand_mode;

	extern uint8_t n_cards_in;
	extern icon_set_t icon_set_check;

// Get machine state
	if ((ret_val = read_machine_state()) != LS_OK)
		goto _EXIT;

	/*
	 * Load sequentially if:
	 *  • carousel sequential
	 *  Load randomly if:
	 *  • carousel is going to be empty
	 */

// Determine how many cards to be dealt (hole + cc)
	uint16_t n_cards_to_be_dealt = game_state.n_players
			* game_rules.n_hole_cards;
	for (uint16_t i = 0; i < game_rules.n_cc_stages; i++)
		n_cards_to_be_dealt += game_rules.cc_stages[i].n_cards;

// Select sequential by default
	rand_mode = SEQ_MODE;
	if (machine_state.random_in == RANDOM_STATE)
		if (user_prefs.empty_machine_after == true
				|| n_cards_to_be_dealt == game_rules.deck_size)
			rand_mode = RAND_MODE;

// Load up to deck size (or to carousel if deal_n_cards)
	do
	{
		if (game_rules.deck_size == 0)
			ret_val = load_carousel(rand_mode, &n_cards);
		else
			ret_val = load_to_n_cards(game_rules.deck_size, rand_mode,
					&n_cards);
		if (ret_val == CARD_STUCK_IN_TRAY)
		{
			prompt_interface(MESSAGE, ret_val, "\nRearrange tray",
					icon_set_check, ICON_VOID, BUTTON_PRESS);
		}
	}
	while (ret_val == CARD_STUCK_IN_TRAY);

	_EXIT:

	return ret_val;
}

return_code_t random_player(uint16_t n_players)
{
	float interval = 100;
	const float max_interval = 300;
	const float factor = 1.1;
	const uint16_t x = 330;
	const uint16_t y = LCD_TOP_ROW + LCD_ROW_HEIGHT * MSG_ROW + V_ADJUST;
	const uint16_t w = 40;
	uint8_t rdm = (uint8_t) n_players;
	uint8_t previous_rdm;

	if (n_players > 1)
	{

		clear_message(TEXT_ERROR);
		do
		{
			// Get different players all the time
			do
			{
				previous_rdm = rdm;
				if (bounded_random(&rdm, (uint8_t) n_players) != HAL_OK)
					return TRNG_ERROR;
			}
			while (rdm == previous_rdm);

			prompt_message("\nRandom player:");
			BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
//			BSP_LCD_SetTextColor(LCD_COLOR_BLUE); // SCREEN DEBUGGING
			BSP_LCD_FillRect(x, y, w, LCD_ROW_HEIGHT);
			BSP_LCD_SetTextColor(LCD_COLOR_TEXT);
			snprintf(display_buf, N_DISP_MAX, "%u ", rdm + 1);

			uint16_t color = LCD_COLOR_RED_SHFLR;
			uint32_t beep_time = 1;
			if (interval * factor >= max_interval)
			{
				color = LCD_COLOR_TEXT;
				beep_time = MEDIUM_BEEP;
			}
			display_text(x, y, display_buf, LCD_BOLD_FONT, color);

			beep(beep_time);
			HAL_Delay((uint32_t) interval);
			interval *= factor;

		}
		while (interval < max_interval);

		// Additional delay
		HAL_Delay(L_WAIT_DELAY);

		clear_message(TEXT_ERROR);
	}

	return LS_OK;

}

return_code_t set_user_prefs(item_code_t game_code)
{
	return_code_t ret_val = LS_OK;
	int16_t start_row;
	const uint16_t n_prefs_categories = 5;
	uint16_t n_prefs_active = 0;
	typedef enum
	{
		DIST_MODE, HC_DEST, CC_DEST, CC_TIMING, EMPTY_AFTER
	} prefs_category_t;

	uint16_t forced_prefs[] =
	{ 0, DEST_TABLE, 0, 0, 0 };

	char *prefs_category_labels[] =
	{ "Hands distribution:", "Deal Hole Cards:", "Deal Comm. Cards:",
			"Comm. Cards:", "Empty at End:" };

	char *dist_mode_labels[] =
	{ "By Player", "Round Robin" };

	char *deal_destination_labels[] =
	{ "Internal Tray", "On Table" };

	char *community_timing_labels[] =
	{ "After Hole Cards", "Before Hole Cards" };

	char *no_yes_labels[] =
	{ "No", "Yes" };

	char **user_selection_labels[] =
	{ dist_mode_labels, deal_destination_labels, deal_destination_labels,
			community_timing_labels, no_yes_labels };

	uint16_t n_options[] =
	{ 2, 2, 2, 2, 2 };

	prefs_category_t idx_to_cat[n_prefs_categories];
	bool is_blocked[n_prefs_categories];
	int16_t cat_to_idx[n_prefs_categories];
	memset(is_blocked, 0, n_prefs_categories);
	memset(cat_to_idx, -1, n_prefs_categories);
	game_rules_t game_rules;
	user_prefs_t user_prefs;
	menu_t temp_menu;
	set_menu(&temp_menu, game_code);

	// set headline composes the menu
	void set_head_line(int16_t *p_idx, prefs_category_t prefs_category)
	{
		idx_to_cat[*p_idx] = prefs_category;
		cat_to_idx[prefs_category] = *p_idx;
		(*p_idx)++;
	}

	uint16_t current_pref_fm_idx(int16_t idx)
	{
		uint16_t X;
		switch (idx_to_cat[idx])
		{
			case DIST_MODE:
				X = user_prefs.dist_mode;
				break;

			case HC_DEST:
				X = user_prefs.hole_cards_dest;
				break;

			case CC_DEST:
				X = user_prefs.community_cards_dest;
				break;

			case CC_TIMING:
				X = user_prefs.community_timing;
				break;

			case EMPTY_AFTER:
				X = user_prefs.empty_machine_after;
				break;

			default:
				X = 0;
		}
		return X;
	}

// Get rules
	if ((ret_val = get_rules(&game_rules, game_code)) != LS_OK)
		goto _EXIT;

// Get preferences
	if ((ret_val = read_user_prefs(&user_prefs, game_code)) != LS_OK)
		goto _EXIT;

// Show title
	snprintf(display_buf, N_DISP_MAX, "* %s", temp_menu.label);
	prompt_title(display_buf);

// Set up screen structure based on game rules
	int16_t idx = 0;
	set_head_line(&idx, DIST_MODE);

	set_head_line(&idx, HC_DEST);
	if (game_rules.n_cc_stages > 0)
	{
		set_head_line(&idx, CC_DEST);
		set_head_line(&idx, CC_TIMING);
	}
	if (game_rules.min_players * game_rules.n_hole_cards < game_rules.deck_size)
		set_head_line(&idx, EMPTY_AFTER);

// Take note of n_prefs_active
	n_prefs_active = idx;

// Determine start row for a balanced screen
	start_row = 4 - n_prefs_active / 2;

// Apply consistency rules
	if (game_rules.stud_structure)
		is_blocked[cat_to_idx[DIST_MODE]] = true;
	else
		is_blocked[cat_to_idx[DIST_MODE]] = false;

//	if (user_prefs.dist_mode == DIST_MODE_ROUND_ROBIN)
//		is_blocked[cat_to_idx[HC_DEST]] = true;
//	else
//		is_blocked[cat_to_idx[HC_DEST]] = false;

	const uint16_t x_cat_pos = LCD_X_TEXT;
	const uint16_t x_pref_pos = 245;
	const uint16_t pref_box_w = BUTTON_ICON_X - x_pref_pos;
	uint16_t y_text_pos;
	uint16_t y_box;

	void refresh_screen(void)
	{
		for (idx = 0; idx < n_prefs_active; idx++)
		{
			// Set y_text_pos
			y_text_pos = LCD_TOP_ROW + LCD_ROW_HEIGHT * (idx + start_row);
			uint16_t y_box = y_text_pos + LCD_ROW_ADJUSTMENT / 2;

			// Determine what preference to show
			uint16_t sel_pref =
					is_blocked[idx] ?
							forced_prefs[idx_to_cat[idx]] :
							current_pref_fm_idx(idx);

			// Display category
			BSP_LCD_SetTextColor(LCD_COLOR_TEXT);
			display_text(x_cat_pos, y_text_pos,
					prefs_category_labels[idx_to_cat[idx]], LCD_BOLD_FONT,
					LCD_COLOR_TEXT);

			// Display selection
			BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
			BSP_LCD_FillRect(x_pref_pos, y_box, pref_box_w, LCD_ROW_HEIGHT);
			BSP_LCD_SetTextColor(LCD_COLOR_TEXT);
			display_text(x_pref_pos, y_text_pos,
					user_selection_labels[idx_to_cat[idx]][sel_pref],
					is_blocked[idx] ? LCD_REGULAR_FONT : LCD_BOLD_FONT,
					is_blocked[idx] ? LCD_COLOR_GREY : LCD_COLOR_TEXT);

		}
	}

// Display preferences based on the above
	refresh_screen();

// Display icons
	display_encoder_icon(ICON_CHECK);
	display_escape_icon(ICON_SAVE);

// Manage cursor
	extern uint8_t LCD_row;
	LCD_row = start_row;
	int16_t increment = 0;
	prompt_dot(LCD_row);
	reset_btns();

// Main loop until function exit
	extern button encoder_btn;
	extern button escape_btn;
	while (!escape_btn.interrupt_press)
	{
		while (!encoder_btn.interrupt_press && !escape_btn.interrupt_press)
		{
			if ((increment = read_encoder(CLK_WISE)))
			{
				// Detect CW turn of encoder
				if (increment > 0)
				{
					clear_dot(LCD_row);
					LCD_row = (uint16_t) min(
							(int16_t ) LCD_row + (int16_t ) increment,
							(int16_t ) (n_prefs_active - 1 + start_row));
					prompt_dot(LCD_row);
				}
				// Detect CCW turn of encoder
				else if (increment < 0)
				{
					clear_dot(LCD_row);
					LCD_row = (uint16_t) max((int16_t ) LCD_row + increment,
							start_row);
					prompt_dot(LCD_row);
				}
			}
		}

		// If encoder pressed and non-blocked row
		if (encoder_btn.interrupt_press)
		{
			// Reset encoder
			reset_btn(&encoder_btn);

			// Take note of index and corresponding selected category
			idx = LCD_row - start_row;
			prefs_category_t sel_cat = idx_to_cat[idx];

			// Determine y positions
			y_text_pos = LCD_TOP_ROW + LCD_ROW_HEIGHT * LCD_row;
			y_box = y_text_pos + LCD_ROW_ADJUSTMENT / 2;

			// Action only if not blocked
			if (!is_blocked[idx])
			{
				uint16_t sel_pref = current_pref_fm_idx(idx);
				uint16_t initial_pref = sel_pref;

				// Initial display of selected preferences in red
				BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
				BSP_LCD_FillRect(x_pref_pos, y_box, pref_box_w,
				LCD_ROW_HEIGHT);
				BSP_LCD_SetTextColor(LCD_COLOR_TEXT);
				display_text(x_pref_pos, y_text_pos,
						user_selection_labels[sel_cat][sel_pref],
						LCD_BOLD_FONT, LCD_COLOR_RED_SHFLR);

				do // while no button press
				{
					// Update display when encoder turn
					if ((increment = read_encoder(CLK_WISE)))
					{
						// Detect CW turn of encoder
						if (increment > 0)
							sel_pref = (sel_pref + 1) % n_options[sel_cat];
						// Detect CCW turn of encoder
						else if (increment < 0)
							sel_pref = (sel_pref + n_options[sel_cat] - 1)
									% n_options[sel_cat];

						// Update selected preference in red
						BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
						BSP_LCD_FillRect(x_pref_pos, y_box, pref_box_w,
						LCD_ROW_HEIGHT);
						BSP_LCD_SetTextColor(LCD_COLOR_TEXT);
						display_text(x_pref_pos, y_text_pos,
								user_selection_labels[sel_cat][sel_pref],
								LCD_BOLD_FONT, LCD_COLOR_RED_SHFLR);
					}
				}
				while (!encoder_btn.interrupt_press
						&& !escape_btn.interrupt_press);

				if (escape_btn.interrupt_press)
					sel_pref = initial_pref;

				reset_btns();

				// Show final choice in white
				BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
				BSP_LCD_FillRect(x_pref_pos, y_box, pref_box_w,
				LCD_ROW_HEIGHT);
				BSP_LCD_SetTextColor(LCD_COLOR_TEXT);
				display_text(x_pref_pos, y_text_pos,
						user_selection_labels[sel_cat][sel_pref],
						LCD_BOLD_FONT, LCD_COLOR_TEXT);

				// Save preference
				if (sel_pref != initial_pref)
				{
					switch (sel_cat)
					{
						case DIST_MODE:
							user_prefs.dist_mode = sel_pref;
							/*
							 * Apply game consistency rules
							 * (must be implemented in parallel in run_stage)
							 */
//							if (user_prefs.dist_mode == DIST_MODE_ROUND_ROBIN)
//								is_blocked[cat_to_idx[HC_DEST]] = true;
//							else
//								is_blocked[cat_to_idx[HC_DEST]] = false;
							break;

						case HC_DEST:
							user_prefs.hole_cards_dest = sel_pref;
							break;

						case CC_DEST:
							user_prefs.community_cards_dest = sel_pref;
							break;

						case CC_TIMING:
							user_prefs.community_timing = sel_pref;
							break;

						case EMPTY_AFTER:
							user_prefs.empty_machine_after = sel_pref;
							break;

						default:
							ret_val = INVALID_CHOICE;
							goto _EXIT;
					}

					// Refresh screen to take changes into account
					refresh_screen();

					// Save user prefs
					if ((ret_val = write_user_prefs(&user_prefs, game_code))
							!= LS_OK)
						goto _EXIT;
				}
			}
		}

	}

	_EXIT:

	return ret_val;
}

return_code_t set_custom_game_rules(item_code_t custom_game_code)
{
	return_code_t ret_val = LS_OK;
	return_code_t user_input;
	char game_name[GAME_NAME_MAX_CHAR + 1];
	game_rules_t game_rules;
	int16_t m_row = 1;
	extern char custom_game_names[N_CUSTOM_GAMES][GAME_NAME_MAX_CHAR + 1];
	extern icon_set_t icon_set_save;
	extern icon_set_t icon_set_check;

// Get rules
	if ((ret_val = get_rules(&game_rules, custom_game_code)) != LS_OK)
		goto _EXIT;

	// Set game name
	strncpy(game_name, custom_game_names[custom_game_code - CUSTOM_1],
	GAME_NAME_MAX_CHAR);
	prompt_text("Edit game name:", m_row, LCD_BOLD_FONT);
	name_input(game_name, GAME_NAME_MAX_CHAR);

	// Get deck size
	clear_text();
	prompt_text("Enter deck size:", m_row, LCD_BOLD_FONT);
	uint16_t number_input = game_rules.deck_size;
	if (get_n_x(&number_input, 2, N_SLOTS, "", "card") == LS_OK)
		game_rules.deck_size = number_input;

	// Get cards per player
	clear_text();
	prompt_text("Enter # of cards per player:", m_row, LCD_BOLD_FONT);
	number_input = min(game_rules.n_hole_cards, game_rules.deck_size / 2);
	if (get_n_x(&number_input, 1, game_rules.deck_size / 2, "", "card")
			== LS_OK)
		game_rules.n_hole_cards = number_input;

	// Get max players
	clear_text();
	prompt_text("Enter max # of players:", m_row, LCD_BOLD_FONT);
	number_input = min(game_rules.max_players,
			game_rules.deck_size / game_rules.n_hole_cards);
	if (get_n_x(&number_input, 2,
			min(15, game_rules.deck_size / game_rules.n_hole_cards), "",
			"player") == LS_OK)
		game_rules.max_players = number_input;

	// Get min players
	clear_text();
	prompt_text("Enter min # of players:", m_row, LCD_BOLD_FONT);
	number_input = min(game_rules.min_players, game_rules.max_players);
	if (get_n_x(&number_input, 2, game_rules.max_players, "", "player")
			== LS_OK)
		game_rules.min_players = number_input;

	// Set community stages
	clear_text();
	prompt_text("Enter # of community card stages:", m_row, LCD_BOLD_FONT);
	number_input = min(game_rules.n_cc_stages, MAX_CC_STAGES);
	if (get_n_x(&number_input, 0, MAX_CC_STAGES, "", "stage") == LS_OK)
		game_rules.n_cc_stages = number_input;

	// Set community stages names
	for (uint16_t i = 0; i < game_rules.n_cc_stages; i++)
	{
		if (game_rules.cc_stages[i].name[0] == 0)
			switch (i)
			{
				case 0:
					strncpy(game_rules.cc_stages[i].name, "Flop",
					STAGE_NAME_MAX_CHAR);
					break;
				case 1:
					strncpy(game_rules.cc_stages[i].name, "Turn",
					STAGE_NAME_MAX_CHAR);
					break;
				case 2:
					strncpy(game_rules.cc_stages[i].name, "River",
					STAGE_NAME_MAX_CHAR);
					break;
				default:
					snprintf(game_rules.cc_stages[i].name,
					STAGE_NAME_MAX_CHAR, "Stage %u", i + 1);
			}

		clear_text();
		prompt_text("Edit stage name:", m_row, LCD_BOLD_FONT);
		name_input(game_rules.cc_stages[i].name, STAGE_NAME_MAX_CHAR);

		// Get number of cards in stage (max defined here)
		clear_text();
		snprintf(display_buf, N_DISP_MAX, "# of cards in %s",
				game_rules.cc_stages[i].name);
		prompt_text(display_buf, m_row, LCD_BOLD_FONT);
		number_input = max(1, min(game_rules.cc_stages[i].n_cards, 6));
		if (get_n_x(&number_input, 1, 6, "", "card") == LS_OK)
			game_rules.cc_stages[i].n_cards = number_input;
	}

	// Get cards to pick
	clear_text();
	prompt_text("Enter max # of cards", m_row, LCD_BOLD_FONT);
	prompt_text("picked at one time:", m_row + 1, LCD_BOLD_FONT);
	number_input = min(game_rules.max_pick, 7);
	if (get_n_x(&number_input, 0, 7, "", "card") == LS_OK)
		game_rules.max_pick = number_input;

	// Allow recycling cards
	user_input = prompt_interface(MESSAGE, CUSTOM_MESSAGE,
			"Option to recycle cards?\n(Reload during games)", icon_set_check,
			ICON_CROSS, BUTTON_PRESS);
	game_rules.recycle_mode = (user_input == LS_OK);

	// Allow burn cards
	user_input = prompt_interface(MESSAGE, CUSTOM_MESSAGE,
			"Option to burn cards?", icon_set_check, ICON_CROSS, BUTTON_PRESS);
	game_rules.allows_burn_cards = (user_input == LS_OK);

	// Allow extra cards
	user_input = prompt_interface(MESSAGE, CUSTOM_MESSAGE,
			"Option for extra cards?", icon_set_check, ICON_CROSS,
			BUTTON_PRESS);
	game_rules.allows_xtra_cards = (user_input == LS_OK);

	// Prompt random player
	user_input = prompt_interface(MESSAGE, CUSTOM_MESSAGE,
			"Prompt random player?", icon_set_check, ICON_CROSS, BUTTON_PRESS);
	game_rules.random_player = (user_input == LS_OK);

	// Save or not
	clear_text();
	user_input = prompt_interface(MESSAGE, CUSTOM_MESSAGE,
			"\nSave custom game?", icon_set_save, ICON_RED_CROSS, BUTTON_PRESS);

	if (user_input == LS_OK)
	{
		// Save game name in live memory (variable)
		strncpy(custom_game_names[custom_game_code - CUSTOM_1], game_name,
		GAME_NAME_MAX_CHAR);
		// Save game name in EERAM
		if ((ret_val = write_custom_game_name(game_name, custom_game_code))
				!= LS_OK)
			goto _EXIT;

		// Save game rules (includes cc_stages)
		if ((ret_val = write_custom_game_rule(&game_rules, custom_game_code))
				!= LS_OK)
			goto _EXIT;;

		// Ensure consistency of user prefs (default number of players)
		user_prefs_t user_prefs;
		if ((ret_val = read_user_prefs(&user_prefs, custom_game_code)) != LS_OK)
			goto _EXIT;
		user_prefs.default_n_players = (game_rules.min_players
				+ game_rules.max_players) / 2;
		if ((ret_val = write_user_prefs(&user_prefs, custom_game_code))
				!= LS_OK)
			goto _EXIT;

	}
	_EXIT:

	return ret_val;
}

return_code_t deal_n_cards(void)
{
	return_code_t ret_val;
	return_code_t user_input;
	extern uint8_t n_cards_in;
	static uint16_t n_cards = 1;

	// Game rules and game states used to parameter discharge_cards
	// Game state written in memory = only n_cards remains as static
	game_rules_t game_rules;
	if ((ret_val = get_rules(&game_rules, DEAL_N_CARDS)) != LS_OK)
		goto _EXIT;
	user_prefs_t user_prefs;
	if ((ret_val = read_user_prefs(&user_prefs, DEAL_N_CARDS)) != LS_OK)
		goto _EXIT;

	_LOADING_LOOP: game_state.current_stage = DEALING_N_CARDS;

	// Load cards if shuffler empty or if cards in tray and not full
	if (n_cards_in == 0 || (n_cards_in != N_SLOTS && cards_in_tray()))
		if ((ret_val = game_load(game_rules, user_prefs)) != LS_OK)
			goto _EXIT;

	do
	{
		if (n_cards_in == 0)
			goto _LOADING_LOOP;

		// Normalise n_cards
		n_cards = min(n_cards, n_cards_in);
		user_input = get_n_pick_or_deal(game_rules, &n_cards);

		// If recycling triggered, exit and load
		if (user_input == DO_RECYCLE)
			goto _LOADING_LOOP;

		if (user_input == CONTINUE)
		{
			game_rules.n_hole_cards = n_cards;
			game_state.n_cards_dealt = 0;
			if ((ret_val = write_game_state()) != LS_OK)
				goto _EXIT;
			ret_val = discharge_cards(game_rules);
		}
	}
	while (user_input == CONTINUE && ret_val == LS_OK);

	// De-mask user input if all OK
	if (ret_val == LS_OK)
		ret_val = user_input;

	_EXIT:

	return ret_val;
}

return_code_t shufflers_choice(void)
{
	return_code_t ret_val;
	return_code_t user_input;
	uint8_t rdm;
	uint8_t prev_rdm;
	extern menu_t dealers_choice_menu;
	extern menu_t shufflers_choice_menu;
	extern button encoder_btn;
	extern button escape_btn;
	extern icon_set_t icon_set_check;

	if (dealers_choice_menu.n_items < 2)
	{
		ret_val = NO_DEALERS_CHOICE_SELECTED;
		goto _EXIT;
	}

	prompt_message("\nWelcome to Shuffler's Choice!");
	HAL_Delay(L_WAIT_DELAY);
	clear_message(TEXT_ERROR);

	prev_rdm = dealers_choice_menu.n_items;
	reset_btns();
	while (1)
	{
		// Set game state to random game from dealer's choice
		if ((ret_val = reset_game_state()) != LS_OK)
			goto _EXIT;
		do
		{
			bounded_random(&rdm, dealers_choice_menu.n_items);
		}
		while (rdm == prev_rdm);
		// Avoid to have twice the same
		prev_rdm = rdm;

		game_state.game_code = dealers_choice_menu.items[rdm];
		if ((ret_val = write_game_state()) != LS_OK)
			goto _EXIT;

		// Get game label
		menu_t current_dealer_game_menu;
		if ((ret_val = set_menu(&current_dealer_game_menu, game_state.game_code))
				!= LS_OK)
			goto _EXIT;
		// Prompt this as title
		prompt_title(current_dealer_game_menu.label);

		// Run game
		if ((ret_val = run_game()) != LS_OK)
		{
			if (ret_val != LS_ESC)
				break;
			else
			{
				prompt_title(shufflers_choice_menu.label);
				user_input = prompt_interface(MESSAGE, CUSTOM_MESSAGE,
						"\nContinue playing?", icon_set_check, ICON_RED_CROSS,
						BUTTON_PRESS);

				if (user_input == LS_ESC)
					break;
			}
		}
	}

	_EXIT:

	return ret_val;
}

