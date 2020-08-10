#pragma once

#ifndef GlobalDefines_h__
#define GlobalDefines_h__

#define DO_FOR_EACH_ENUM_GLOBAL_DEFINE(DO) \
	DO(int, DEFAULT_SPECIALIST)

#define DO_FOR_EACH_EXPOSED_INT_GLOBAL_DEFINE(DO) \
	DO(int, MOVE_DENOMINATOR) \
	DO(int, NUM_UNIT_PREREQ_OR_BONUSES) \
	DO(int, MAX_PLOT_LIST_ROWS) \
	DO(int, EVENT_MESSAGE_TIME) \
	DO(int, ROUTE_FEATURE_GROWTH_MODIFIER) \
	DO(int, FEATURE_GROWTH_MODIFIER) \
	DO(int, NUM_AND_TECH_PREREQS) \
	DO(int, NUM_OR_TECH_PREREQS) \
	DO(int, MIN_WATER_SIZE_FOR_OCEAN) \
	DO(int, MAX_CITY_DEFENSE_DAMAGE) \
	DO(int, NUM_CORPORATION_PREREQ_BONUSES) \
	DO(int, BATTLE_EFFECT_LESS_FOOD) \
	DO(int, BATTLE_EFFECT_LESS_PRODUCTION) \
	DO(int, BATTLE_EFFECT_LESS_COMMERCE) \
	DO(int, BATTLE_EFFECTS_MINIMUM_TURN_INCREMENTS) \
	DO(int, MAX_BATTLE_TURNS) \
	DO(int, DCM_RB_CITY_INACCURACY) \
	DO(int, DCM_RB_CITYBOMBARD_CHANCE) \
	DO(int, IDW_INFLUENCE_RADIUS) \
	DO(int, IDW_EMERGENCY_DRAFT_MIN_POPULATION)

#define DO_FOR_EACH_EXPOSED_BOOL_GLOBAL_DEFINE(DO) \
	DO(bool, DCM_BATTLE_EFFECTS) \
	DO(bool, DCM_AIR_BOMBING) \
	DO(bool, DCM_RANGE_BOMBARD) \
	DO(bool, DCM_ATTACK_SUPPORT) \
	DO(bool, DCM_OPP_FIRE) \
	DO(bool, DCM_ACTIVE_DEFENSE) \
	DO(bool, DCM_FIGHTER_ENGAGE) \
	DO(bool, DYNAMIC_CIV_NAMES) \
	DO(bool, LIMITED_RELIGIONS_EXCEPTIONS) \
	DO(bool, OC_RESPAWN_HOLY_CITIES) \
	DO(bool, IDW_ENABLED) \
	DO(bool, IDW_EMERGENCY_DRAFT_ENABLED) \
	DO(bool, IDW_NO_BARBARIAN_INFLUENCE) \
	DO(bool, IDW_NO_NAVAL_INFLUENCE) \
	DO(bool, IDW_PILLAGE_INFLUENCE_ENABLED) \
	DO(bool, SS_ENABLED) \
	DO(bool, SS_BRIBE) \
	DO(bool, SS_ASSASSINATE)

#define DO_FOR_EACH_EXPOSED_GLOBAL_DEFINE(DO) \
	DO_FOR_EACH_EXPOSED_INT_GLOBAL_DEFINE(DO) \
	DO_FOR_EACH_EXPOSED_BOOL_GLOBAL_DEFINE(DO)

#define DO_FOR_EACH_INT_GLOBAL_DEFINE(DO) \
	DO(int, ADVANCED_START_SIGHT_RANGE) \
	DO(int, AI_SHOULDNT_MANAGE_PLOT_ASSIGNMENT) \
	DO(int, AT_WAR_CULTURE_ANGER_MODIFIER) \
	DO(int, BASE_OCCUPATION_TURNS) \
	DO(int, BUILDING_PRODUCTION_DECAY_PERCENT) \
	DO(int, BUILDING_PRODUCTION_DECAY_TIME) \
	DO(int, CAPITAL_TRADE_MODIFIER) \
	DO(int, CIRCUMNAVIGATE_FREE_MOVES) \
	DO(int, CITY_AIR_UNIT_CAPACITY) \
	DO(int, CITY_DEFENSE_DAMAGE_HEAL_RATE) \
	DO(int, CITY_FREE_CULTURE_GROWTH_FACTOR) \
	DO(int, CITY_THIRD_RING_EXTRA_GROWTH_THRESHOLD_PERCENT) \
	DO(int, COLONY_NUM_FREE_DEFENDERS) \
	DO(int, COMMERCE_ATTACKS_FADE_RATE) \
	DO(int, CONSCRIPT_ANGER_DIVISOR) \
	DO(int, CONSCRIPT_MIN_CITY_POPULATION) \
	DO(int, CONSCRIPT_MIN_CULTURE_PERCENT) \
	DO(int, CONSCRIPT_POP_ANGER) \
	DO(int, CONSCRIPT_POPULATION_PER_COST) \
	DO(int, CORPORATION_FOREIGN_SPREAD_COST_PERCENT) \
	DO(int, CORPORATION_RESOURCE_BASE_INFLUENCE) \
	DO(int, CORPORATION_SPREAD_DISTANCE_DIVISOR) \
	DO(int, CORPORATION_SPREAD_RAND) \
	DO(int, CULTURE_PERCENT_ANGER) \
	DO(int, DEFY_RESOLUTION_ANGER_DIVISOR) \
	DO(int, DEFY_RESOLUTION_POP_ANGER) \
	DO(int, DIRTY_POWER_HEALTH_CHANGE) \
	DO(int, DISABLE_RAND_SEED_ON_LOAD) \
	DO(int, EVENT_MESSAGE_TIME_LONG) \
	DO(int, FOREIGN_TRADE_FULL_CREDIT_PEACE_TURNS) \
	DO(int, FOREIGN_TRADE_MODIFIER) \
	DO(int, FREE_CITY_ADJACENT_CULTURE) \
	DO(int, FREE_CITY_CULTURE) \
	DO(int, FREE_TRADE_CORPORATION_SPREAD_MOD) \
	DO(int, FRESH_WATER_HEALTH_CHANGE) \
	DO(int, GOLDEN_AGE_GREAT_PEOPLE_MODIFIER) \
	DO(int, GOLDEN_AGE_LENGTH) \
	DO(int, HOLY_CITY_INFLUENCE) \
	DO(int, HURRY_ANGER_DIVISOR) \
	DO(int, HURRY_POP_ANGER) \
	DO(int, IGNORE_PLOT_GROUP_FOR_TRADE_ROUTES) \
	DO(int, BASE_FREE_UNITS_UPKEEP_MILITARY) \
	DO(int, BASE_FREE_UNITS_UPKEEP_CIVILIAN) \
	DO(int, INITIAL_CITY_POPULATION) \
	DO(int, BASE_FREE_UNITS_UPKEEP_MILITARY_PER_100_POP) \
	DO(int, BASE_FREE_UNITS_UPKEEP_CIVILIAN_PER_100_POP) \
	DO(int, INITIAL_NON_STATE_RELIGION_HAPPINESS) \
	DO(int, INITIAL_STATE_RELIGION_HAPPINESS) \
	DO(int, INITIAL_TRADE_ROUTES) \
	DO(int, LANDMARK_ANGER_DIVISOR) \
	DO(int, MAX_DISTANCE_CITY_MAINTENANCE) \
	DO(int, MAX_TRADE_ROUTES) \
	DO(int, MAXED_BUILDING_GOLD_PERCENT) \
	DO(int, MAXED_PROJECT_GOLD_PERCENT) \
	DO(int, MAXED_UNIT_GOLD_PERCENT) \
	DO(int, MIN_BARBARIAN_CITY_STARTING_DISTANCE) \
	DO(int, MIN_BARBARIAN_STARTING_DISTANCE) \
	DO(int, MIN_POP_CONSIDER_INQUISITORS) \
	DO(int, MIN_TIMER_UNIT_DOUBLE_MOVES) \
	DO(int, NEW_HURRY_MODIFIER) \
	DO(int, NO_MILITARY_PERCENT_ANGER) \
	DO(int, OCCUPATION_TURNS_POPULATION_PERCENT) \
	DO(int, OUR_POPULATION_TRADE_MODIFIER) \
	DO(int, OUR_POPULATION_TRADE_MODIFIER_OFFSET) \
	DO(int, OVERSEAS_TRADE_MODIFIER) \
	DO(int, PLOT_VISIBILITY_RANGE) \
	DO(int, POWER_HEALTH_CHANGE) \
	DO(int, RELIGION_PERCENT_ANGER) \
	DO(int, RELIGION_SPREAD_DISTANCE_DIVISOR) \
	DO(int, RELIGION_SPREAD_RAND) \
	DO(int, REVOLT_DEFENSE_STATE_RELIGION_MODIFIER) \
	DO(int, REVOLT_OFFENSE_STATE_RELIGION_MODIFIER) \
	DO(int, REVOLT_TEST_PROB) \
	DO(int, REVOLT_TOTAL_CULTURE_MODIFIER) \
	DO(int, SCORE_FREE_PERCENT) \
	DO(int, SCORE_LAND_FACTOR) \
	DO(int, SCORE_POPULATION_FACTOR) \
	DO(int, SCORE_TECH_FACTOR) \
	DO(int, SCORE_WONDER_FACTOR) \
	DO(int, SHOW_BUILDINGS_LEVEL) \
	DO(int, START_YEAR) \
	DO(int, STARTING_DISTANCE_PERCENT) \
	DO(int, TEAM_VOTE_MIN_CANDIDATES) \
	DO(int, TEMP_HAPPY) \
	DO(int, THEIR_POPULATION_TRADE_PERCENT) \
	DO(int, TRADE_PROFIT_PERCENT) \
	DO(int, TREAT_NEGATIVE_GOLD_AS_MAINTENANCE) \
	DO(int, UNIT_PRODUCTION_DECAY_PERCENT) \
	DO(int, UNIT_PRODUCTION_DECAY_TIME) \
	DO(int, WE_LOVE_THE_KING_POPULATION_MIN_POPULATION) \
	DO(int, WE_LOVE_THE_KING_RAND) \
	DO(int, VASSAL_HAPPINESS) \
	DO(int, WASTAGE_START_CONSUMPTION_PERCENT) \
	DO(int, USE_CANNOT_CREATE_PROJECT_CALLBACK) \
	DO(int, USE_CAN_DO_MELTDOWN_CALLBACK) \
	DO(int, USE_CANNOT_MAINTAIN_PROCESS_CALLBACK) \
	DO(int, USE_CAN_DO_GROWTH_CALLBACK) \
	DO(int, USE_CAN_DO_CULTURE_CALLBACK) \
	DO(int, USE_CAN_DO_PRODUCTION_CALLBACK) \
	DO(int, USE_CAN_DO_RELIGION_CALLBACK) \
	DO(int, USE_CAN_DO_GREATPEOPLE_CALLBACK) \
	DO(int, USE_CAN_RAZE_CITY_CALLBACK) \
	DO(int, USE_CAN_DO_GOLD_CALLBACK) \
	DO(int, USE_CAN_DO_RESEARCH_CALLBACK) \
	DO(int, USE_UPGRADE_UNIT_PRICE_CALLBACK) \
	DO(int, USE_AI_UPDATE_UNIT_CALLBACK) \
	DO(int, USE_AI_CHOOSE_PRODUCTION_CALLBACK) \
	DO(int, USE_AI_DO_DIPLO_CALLBACK) \
	DO(int, USE_AI_BESTTECH_CALLBACK) \
	DO(int, USE_CAN_DO_COMBAT_CALLBACK) \
	DO(int, USE_AI_CAN_DO_WARPLANS_CALLBACK) \
	DO(int, LAND_UNITS_CAN_ATTACK_WATER_CITIES) \
	DO(int, BASE_UNIT_UPGRADE_COST) \
	DO(int, UPGRADE_ROUND_LIMIT) \
	DO(int, CITY_BARBARIAN_DEFENSE_MODIFIER) \
	DO(int, UNIT_VISIBILITY_RANGE) \
	DO(int, MAX_UNIT_VISIBILITY_RANGE) \
	DO(int, GREATER_COMMERCE_SWITCH_POINT) \
	DO(int, WORKER_TRADE_VALUE_PERCENT_ADJUSTMENT) \
	DO(int, TRADE_MISSION_END_TOTAL_PERCENT_ADJUSTMENT) \
	DO(int, INFILTRATE_MISSION_END_TOTAL_PERCENT_ADJUSTMENT) \
	DO(int, ESPIONAGE_MISSION_COST_END_TOTAL_PERCENT_ADJUSTMENT) \
	DO(int, WATER_POTENTIAL_CITY_WORK_FOR_AREA) \
	DO(int, SAD_MAX_MODIFIER) \
	DO(int, UPSCALED_RESEARCH_COST_MODIFIER) \
	DO(int, ENABLE_DYNAMIC_UNIT_ENTITIES) \
	DO(int, PEAK_EXTRA_DEFENSE) \
	DO(int, PEAK_EXTRA_MOVEMENT) \
	DO(int, NUM_BUILDING_PREREQ_OR_BONUSES) \
	DO(int, FOOD_CONSUMPTION_PER_POPULATION) \
	DO(int, MAX_HIT_POINTS) \
	DO(int, PATH_DAMAGE_WEIGHT) \
	DO(int, HILLS_EXTRA_DEFENSE) \
	DO(int, RIVER_ATTACK_MODIFIER) \
	DO(int, AMPHIB_ATTACK_MODIFIER) \
	DO(int, HILLS_EXTRA_MOVEMENT) \
	DO(int, RIVER_EXTRA_MOVEMENT) \
	DO(int, UNIT_MULTISELECT_MAX) \
	DO(int, PERCENT_ANGER_DIVISOR) \
	DO(int, MIN_CITY_RANGE) \
	DO(int, CITY_MAX_NUM_BUILDINGS) \
	DO(int, NUM_UNIT_AND_TECH_PREREQS) \
	DO(int, LAKE_MAX_AREA_SIZE) \
	DO(int, NUM_ROUTE_PREREQ_OR_BONUSES) \
	DO(int, NUM_BUILDING_AND_TECH_PREREQS) \
	DO(int, FORTIFY_MODIFIER_PER_TURN) \
	DO(int, ESTABLISH_MODIFIER_PER_TURN) \
	DO(int, ESCAPE_MODIFIER_PER_TURN) \
	DO(int, PEAK_SEE_THROUGH_CHANGE) \
	DO(int, HILLS_SEE_THROUGH_CHANGE) \
	DO(int, SEAWATER_SEE_FROM_CHANGE) \
	DO(int, PEAK_SEE_FROM_CHANGE) \
	DO(int, HILLS_SEE_FROM_CHANGE) \
	DO(int, USE_SPIES_NO_ENTER_BORDERS) \
	DO(int, USE_CANNOT_FOUND_CITY_CALLBACK) \
	DO(int, USE_CAN_FOUND_CITIES_ON_WATER_CALLBACK) \
	DO(int, USE_IS_PLAYER_RESEARCH_CALLBACK) \
	DO(int, USE_CAN_RESEARCH_CALLBACK) \
	DO(int, USE_CANNOT_DO_CIVIC_CALLBACK) \
	DO(int, USE_CAN_DO_CIVIC_CALLBACK) \
	DO(int, USE_CANNOT_CONSTRUCT_CALLBACK) \
	DO(int, USE_CAN_CONSTRUCT_CALLBACK) \
	DO(int, USE_CAN_DECLARE_WAR_CALLBACK) \
	DO(int, USE_CANNOT_RESEARCH_CALLBACK) \
	DO(int, USE_GET_UNIT_COST_MOD_CALLBACK) \
	DO(int, USE_GET_BUILDING_COST_MOD_CALLBACK) \
	DO(int, USE_GET_CITY_FOUND_VALUE_CALLBACK) \
	DO(int, USE_CANNOT_HANDLE_ACTION_CALLBACK) \
	DO(int, USE_CAN_TRAIN_CALLBACK) \
	DO(int, USE_CANNOT_TRAIN_CALLBACK) \
	DO(int, USE_CAN_BUILD_CALLBACK) \
	DO(int, USE_UNIT_CANNOT_MOVE_INTO_CALLBACK) \
	DO(int, USE_USE_CANNOT_SPREAD_RELIGION_CALLBACK) \
	DO(int, USE_FINISH_TEXT_CALLBACK) \
	DO(int, USE_ON_UNIT_SET_XY_CALLBACK) \
	DO(int, USE_ON_UNIT_SELECTED_CALLBACK) \
	DO(int, USE_ON_UPDATE_CALLBACK) \
	DO(int, USE_ON_UNIT_CREATED_CALLBACK) \
	DO(int, USE_ON_UNIT_LOST_CALLBACK) \
	DO(int, USE_CAN_DO_PLOT_CULTURE_CALLBACK) \
	DO(int, GAMEFONT_TGA_RELIGIONS) \
	DO(int, GAMEFONT_TGA_CORPORATIONS) \
	DO(int, BBAI_DEFENSIVE_PACT_BEHAVIOR) \
	DO(int, WAR_SUCCESS_CITY_CAPTURING) \
	DO(int, BBAI_ATTACK_CITY_STACK_RATIO) \
	DO(int, BBAI_SKIP_BOMBARD_BEST_ATTACK_ODDS) \
	DO(int, BBAI_SKIP_BOMBARD_BASE_STACK_RATIO) \
	DO(int, BBAI_SKIP_BOMBARD_MIN_STACK_RATIO) \
	DO(int, TECH_DIFFUSION_KNOWN_TEAM_MODIFIER) \
	DO(int, TECH_DIFFUSION_WELFARE_THRESHOLD) \
	DO(int, TECH_DIFFUSION_WELFARE_MODIFIER) \
	DO(int, TECH_COST_MODIFIER) \
	DO(int, UNIT_PRODUCTION_PERCENT_SM) \
	DO(int, UNIT_PRODUCTION_PERCENT) \
	DO(int, BUILDING_PRODUCTION_PERCENT) \
	DO(int, COMBAT_DIE_SIDES) \
	DO(int, COMBAT_DAMAGE) \
	DO(int, PLAYER_ALWAYS_RAZES_CITIES) \
	DO(int, MAX_PLOT_LIST_SIZE) \
	DO(int, UNIT_GOLD_DISBAND_DIVISOR) \
	DO(int, SIZE_MATTERS_MOST_MULTIPLIER) \
	DO(int, IDW_BASE_COMBAT_INFLUENCE) \
	DO(int, IDW_WARLORD_MULTIPLIER) \
	DO(int, IDW_CITY_TILE_MULTIPLIER) \
	DO(int, IDW_WINNER_PLOT_MULTIPLIER) \
	DO(int, IDW_LOSER_PLOT_MULTIPLIER) \
	DO(int, IDW_NO_CITY_DEFENDER_MULTIPLIER) \
	DO(int, IDW_FORT_CAPTURE_MULTIPLIER) \
	DO(int, IDW_EMERGENCY_DRAFT_STRENGTH) \
	DO(int, IDW_EMERGENCY_DRAFT_ANGER_MULTIPLIER) \
	DO(int, IDW_BASE_PILLAGE_INFLUENCE) \
	DO_FOR_EACH_EXPOSED_INT_GLOBAL_DEFINE(DO)

#define DO_FOR_EACH_BOOL_GLOBAL_DEFINE(DO) \
	DO_FOR_EACH_EXPOSED_BOOL_GLOBAL_DEFINE(DO)

#define DO_FOR_EACH_FLOAT_GLOBAL_DEFINE(DO) \
	DO(float, CAMERA_MIN_YAW) \
	DO(float, CAMERA_MAX_YAW) \
	DO(float, CAMERA_FAR_CLIP_Z_HEIGHT) \
	DO(float, CAMERA_MAX_TRAVEL_DISTANCE) \
	DO(float, CAMERA_START_DISTANCE) \
	DO(float, AIR_BOMB_HEIGHT) \
	DO(float, CAMERA_SPECIAL_PITCH) \
	DO(float, CAMERA_MAX_TURN_OFFSET) \
	DO(float, CAMERA_MIN_DISTANCE) \
	DO(float, CAMERA_UPPER_PITCH) \
	DO(float, CAMERA_LOWER_PITCH) \
	DO(float, FIELD_OF_VIEW) \
	DO(float, SHADOW_SCALE) \
	DO(float, UNIT_MULTISELECT_DISTANCE) \
	DO(float, SAD_FACTOR_1) \
	DO(float, SAD_FACTOR_2) \
	DO(float, SAD_FACTOR_3) \
	DO(float, SAD_FACTOR_4)

#define DO_FOR_EACH_GLOBAL_DEFINE(DO) \
	DO_FOR_EACH_INT_GLOBAL_DEFINE(DO) \
	DO_FOR_EACH_ENUM_GLOBAL_DEFINE(DO) \
	DO_FOR_EACH_BOOL_GLOBAL_DEFINE(DO) \
	DO_FOR_EACH_FLOAT_GLOBAL_DEFINE(DO)

#endif