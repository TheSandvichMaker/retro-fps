#pragma once

global thread_local struct gamestate_t *g_game;

fn_local void equip_gamestate(struct gamestate_t *game) 
{ 
	g_game = game; 
}

fn_local void unequip_gamestate(void) 
{ 
	equip_gamestate(NULL); 
}
