/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "g_local.h"
#include "g_savetab.h"

field_t fields[] = {
	{"classname", FOFS(classname), F_LSTRING},
	{"model", FOFS(model), F_LSTRING},
	{"spawnflags", FOFS(spawnflags), F_INT},
	{"speed", FOFS(speed), F_FLOAT},
	{"accel", FOFS(accel), F_FLOAT},
	{"decel", FOFS(decel), F_FLOAT},
	{"target", FOFS(target), F_LSTRING},
	{"targetname", FOFS(targetname), F_LSTRING},
	{"pathtarget", FOFS(pathtarget), F_LSTRING},
	{"deathtarget", FOFS(deathtarget), F_LSTRING},
	{"killtarget", FOFS(killtarget), F_LSTRING},
	{"combattarget", FOFS(combattarget), F_LSTRING},
	{"message", FOFS(message), F_LSTRING},
	{"team", FOFS(team), F_LSTRING},
	{"wait", FOFS(wait), F_FLOAT},
	{"delay", FOFS(delay), F_FLOAT},
	{"random", FOFS(random), F_FLOAT},
	{"move_origin", FOFS(move_origin), F_VECTOR},
	{"move_angles", FOFS(move_angles), F_VECTOR},
	{"style", FOFS(style), F_INT},
	{"count", FOFS(count), F_INT},
	{"health", FOFS(health), F_INT},
	{"sounds", FOFS(sounds), F_INT},
	{"light", 0, F_IGNORE},
	{"dmg", FOFS(dmg), F_INT},
	{"mass", FOFS(mass), F_INT},
	{"volume", FOFS(volume), F_FLOAT},
	{"attenuation", FOFS(attenuation), F_FLOAT},
	{"map", FOFS(map), F_LSTRING},
	{"origin", FOFS(s) + (int)offsetof(entity_state_t, origin), F_VECTOR},
	{"angles", FOFS(s) + (int)offsetof(entity_state_t, angles), F_VECTOR},
	{"angle", FOFS(s) + (int)offsetof(entity_state_t, angles), F_ANGLEHACK},

	{"goalentity", FOFS(goalentity), F_EDICT, FFL_NOSPAWN},
	{"movetarget", FOFS(movetarget), F_EDICT, FFL_NOSPAWN},
	{"enemy", FOFS(enemy), F_EDICT, FFL_NOSPAWN},
	{"oldenemy", FOFS(oldenemy), F_EDICT, FFL_NOSPAWN},
	{"activator", FOFS(activator), F_EDICT, FFL_NOSPAWN},
	{"groundentity", FOFS(groundentity), F_EDICT, FFL_NOSPAWN},
	{"teamchain", FOFS(teamchain), F_EDICT, FFL_NOSPAWN},
	{"teammaster", FOFS(teammaster), F_EDICT, FFL_NOSPAWN},
	{"owner", FOFS(owner), F_EDICT, FFL_NOSPAWN},
	{"mynoise", FOFS(mynoise), F_EDICT, FFL_NOSPAWN},
	{"mynoise2", FOFS(mynoise2), F_EDICT, FFL_NOSPAWN},
	{"target_ent", FOFS(target_ent), F_EDICT, FFL_NOSPAWN},
	{"chain", FOFS(chain), F_EDICT, FFL_NOSPAWN},

	{"prethink", FOFS(prethink), F_FUNCTION, FFL_NOSPAWN},
	{"think", FOFS(think), F_FUNCTION, FFL_NOSPAWN},
	{"blocked", FOFS(blocked), F_FUNCTION, FFL_NOSPAWN},
	{"touch", FOFS(touch), F_FUNCTION, FFL_NOSPAWN},
	{"use", FOFS(use), F_FUNCTION, FFL_NOSPAWN},
	{"pain", FOFS(pain), F_FUNCTION, FFL_NOSPAWN},
	{"die", FOFS(die), F_FUNCTION, FFL_NOSPAWN},

	{"stand", FOFS(monsterinfo) + (int)offsetof(monsterinfo_t, stand), F_FUNCTION, FFL_NOSPAWN},
	{"idle", FOFS(monsterinfo) + (int)offsetof(monsterinfo_t, idle), F_FUNCTION, FFL_NOSPAWN},
	{"search", FOFS(monsterinfo) + (int)offsetof(monsterinfo_t, search), F_FUNCTION, FFL_NOSPAWN},
	{"walk", FOFS(monsterinfo) + (int)offsetof(monsterinfo_t, walk), F_FUNCTION, FFL_NOSPAWN},
	{"run", FOFS(monsterinfo) + (int)offsetof(monsterinfo_t, run), F_FUNCTION, FFL_NOSPAWN},
	{"dodge", FOFS(monsterinfo) + (int)offsetof(monsterinfo_t, dodge), F_FUNCTION, FFL_NOSPAWN},
	{"attack", FOFS(monsterinfo) + (int)offsetof(monsterinfo_t, attack), F_FUNCTION, FFL_NOSPAWN},
	{"melee", FOFS(monsterinfo) + (int)offsetof(monsterinfo_t, melee), F_FUNCTION, FFL_NOSPAWN},
	{"sight", FOFS(monsterinfo) + (int)offsetof(monsterinfo_t, sight), F_FUNCTION, FFL_NOSPAWN},
	{"checkattack", FOFS(monsterinfo) + (int)offsetof(monsterinfo_t, checkattack), F_FUNCTION, FFL_NOSPAWN},
	{"currentmove", FOFS(monsterinfo) + (int)offsetof(monsterinfo_t, currentmove), F_MMOVE, FFL_NOSPAWN},

	{"endfunc", FOFS(moveinfo) + (int)offsetof(moveinfo_t, endfunc), F_FUNCTION, FFL_NOSPAWN},

	// temp spawn vars -- only valid when the spawn function is called
	{"lip", STOFS(lip), F_INT, FFL_SPAWNTEMP},
	{"distance", STOFS(distance), F_INT, FFL_SPAWNTEMP},
	{"height", STOFS(height), F_INT, FFL_SPAWNTEMP},
	{"noise", STOFS(noise), F_LSTRING, FFL_SPAWNTEMP},
	{"pausetime", STOFS(pausetime), F_FLOAT, FFL_SPAWNTEMP},
	{"item", STOFS(item), F_LSTRING, FFL_SPAWNTEMP},

//need for item field in edict struct, FFL_SPAWNTEMP item will be skipped on saves
	{"item", FOFS(item), F_ITEM},

	{"gravity", STOFS(gravity), F_LSTRING, FFL_SPAWNTEMP},
	{"sky", STOFS(sky), F_LSTRING, FFL_SPAWNTEMP},
	{"skyrotate", STOFS(skyrotate), F_FLOAT, FFL_SPAWNTEMP},
	{"skyaxis", STOFS(skyaxis), F_VECTOR, FFL_SPAWNTEMP},
	{"minyaw", STOFS(minyaw), F_FLOAT, FFL_SPAWNTEMP},
	{"maxyaw", STOFS(maxyaw), F_FLOAT, FFL_SPAWNTEMP},
	{"minpitch", STOFS(minpitch), F_FLOAT, FFL_SPAWNTEMP},
	{"maxpitch", STOFS(maxpitch), F_FLOAT, FFL_SPAWNTEMP},
	{"nextmap", STOFS(nextmap), F_LSTRING, FFL_SPAWNTEMP},

	{0, 0, 0, 0}

};

/*
============
InitGame

This will be called when the dll is first loaded, which
only happens when a new game is started or a save game
is loaded.
============
*/
void InitGame (void)
{
	gi.dprintf ("==== InitGame ====\n");

	gun_x = gi.cvar ("gun_x", "0", 0);
	gun_y = gi.cvar ("gun_y", "0", 0);
	gun_z = gi.cvar ("gun_z", "0", 0);

	//FIXME: sv_ prefix is wrong for these
	sv_rollspeed = gi.cvar ("sv_rollspeed", "200", 0);
	sv_rollangle = gi.cvar ("sv_rollangle", "2", 0);
	sv_maxvelocity = gi.cvar ("sv_maxvelocity", "2000", 0);
	sv_gravity = gi.cvar ("sv_gravity", "800", 0);

	// noset vars
	dedicated = gi.cvar ("dedicated", "0", CVAR_NOSET);

	// latched vars
	sv_cheats = gi.cvar ("cheats", "0", CVAR_SERVERINFO|CVAR_LATCH);
	gi.cvar ("gamename", GAMEVERSION , CVAR_SERVERINFO | CVAR_LATCH);
	gi.cvar ("gamedate", __DATE__ , CVAR_SERVERINFO | CVAR_LATCH);

	maxclients = gi.cvar ("maxclients", "4", CVAR_SERVERINFO | CVAR_LATCH);
	maxspectators = gi.cvar ("maxspectators", "4", CVAR_SERVERINFO);
	deathmatch = gi.cvar ("deathmatch", "0", CVAR_LATCH);
	coop = gi.cvar ("coop", "0", CVAR_LATCH);
	skill = gi.cvar ("skill", "1", CVAR_LATCH);
	maxentities = gi.cvar ("maxentities", "1024", CVAR_LATCH);

	// change anytime vars
	dmflags = gi.cvar ("dmflags", "0", CVAR_SERVERINFO);
	fraglimit = gi.cvar ("fraglimit", "0", CVAR_SERVERINFO);
	timelimit = gi.cvar ("timelimit", "0", CVAR_SERVERINFO);
	password = gi.cvar ("password", "", CVAR_USERINFO);
	spectator_password = gi.cvar ("spectator_password", "", CVAR_USERINFO);
	filterban = gi.cvar ("filterban", "1", 0);

	g_select_empty = gi.cvar ("g_select_empty", "0", CVAR_ARCHIVE);

	run_pitch = gi.cvar ("run_pitch", "0.002", 0);
	run_roll = gi.cvar ("run_roll", "0.005", 0);
	bob_up  = gi.cvar ("bob_up", "0.005", 0);
	bob_pitch = gi.cvar ("bob_pitch", "0.002", 0);
	bob_roll = gi.cvar ("bob_roll", "0.002", 0);

	// flood control
	flood_msgs = gi.cvar ("flood_msgs", "4", 0);
	flood_persecond = gi.cvar ("flood_persecond", "4", 0);
	flood_waitdelay = gi.cvar ("flood_waitdelay", "10", 0);

	// dm map list
	sv_maplist = gi.cvar ("sv_maplist", "", 0);

	// items
	InitItems ();

	Com_sprintf (game.helpmessage1, sizeof(game.helpmessage1), "");

	Com_sprintf (game.helpmessage2, sizeof(game.helpmessage2), "");

	// initialize all entities for this game
	game.maxentities = maxentities->value;
	g_edicts =  gi.TagMalloc (game.maxentities * sizeof(g_edicts[0]), TAG_GAME);
	globals.edicts = g_edicts;
	globals.max_edicts = game.maxentities;

	// initialize all clients for this game
	game.maxclients = maxclients->value;
	game.clients = gi.TagMalloc (game.maxclients * sizeof(game.clients[0]), TAG_GAME);
	globals.num_edicts = game.maxclients+1;
}

//=========================================================

/*
WarpQuake2: the savegames are written field by field, big-endian, with the
function pointers and monster moves by name (g_savetab.c) and every other
pointer as an index or the string itself.  id's wrote each struct as the
compiler laid it out and a function as its offset from InitGame, which no
other CPU or build could read: a save from an older build (same __DATE__)
jumped into the wrong code.  So a save now loads on any CPU and in any build
that has the same fields (SAVE_MAGIC and the layout's hash say so).
*/

#define SAVE_MAGIC	"WQ2SAVE1"

typedef enum
{
	SF_INT,			// any integer, char to int (an enum too), as 32 bits
	SF_FLOAT,
	SF_BYTES,		// char arrays
	SF_LSTRING,		// char *, TAG_LEVEL when read
	SF_EDICT,
	SF_CLIENT,
	SF_ITEM,
	SF_FUNC,
	SF_MMOVE,
	SF_IGNORE,		// rebuilt after loading (links, game.clients)
	SF_STRUCT
} savetype_t;

typedef struct savefield_s
{
	char		*name;
	int			ofs;
	int			size;		// the whole field
	int			elem;		// one element of it
	savetype_t	type;
	struct savefield_s	*sub;	// SF_STRUCT
} savefield_t;

#define SF(st, m, t)		{#m, (int)offsetof(st, m), sizeof(((st *)0)->m), sizeof(((st *)0)->m), t, NULL}
#define SFA(st, m, t)		{#m, (int)offsetof(st, m), sizeof(((st *)0)->m), sizeof(((st *)0)->m[0]), t, NULL}
#define SFS(st, m, sub)		{#m, (int)offsetof(st, m), sizeof(((st *)0)->m), sizeof(((st *)0)->m), SF_STRUCT, sub}
#define SF_END				{NULL, 0, 0, 0, SF_IGNORE, NULL}

static savefield_t pmovefields[] =
{
	SF(pmove_state_t, pm_type, SF_INT),
	SFA(pmove_state_t, origin, SF_INT),
	SFA(pmove_state_t, velocity, SF_INT),
	SF(pmove_state_t, pm_flags, SF_INT),
	SF(pmove_state_t, pm_time, SF_INT),
	SF(pmove_state_t, gravity, SF_INT),
	SFA(pmove_state_t, delta_angles, SF_INT),
	SF_END
};

static savefield_t entstatefields[] =
{
	SF(entity_state_t, number, SF_INT),
	SFA(entity_state_t, origin, SF_FLOAT),
	SFA(entity_state_t, angles, SF_FLOAT),
	SFA(entity_state_t, old_origin, SF_FLOAT),
	SF(entity_state_t, modelindex, SF_INT),
	SF(entity_state_t, modelindex2, SF_INT),
	SF(entity_state_t, modelindex3, SF_INT),
	SF(entity_state_t, modelindex4, SF_INT),
	SF(entity_state_t, frame, SF_INT),
	SF(entity_state_t, skinnum, SF_INT),
	SF(entity_state_t, effects, SF_INT),
	SF(entity_state_t, renderfx, SF_INT),
	SF(entity_state_t, solid, SF_INT),
	SF(entity_state_t, sound, SF_INT),
	SF(entity_state_t, event, SF_INT),
	SF_END
};

static savefield_t playerstatefields[] =
{
	SFS(player_state_t, pmove, pmovefields),
	SFA(player_state_t, viewangles, SF_FLOAT),
	SFA(player_state_t, viewoffset, SF_FLOAT),
	SFA(player_state_t, kick_angles, SF_FLOAT),
	SFA(player_state_t, gunangles, SF_FLOAT),
	SFA(player_state_t, gunoffset, SF_FLOAT),
	SF(player_state_t, gunindex, SF_INT),
	SF(player_state_t, gunframe, SF_INT),
	SFA(player_state_t, blend, SF_FLOAT),
	SF(player_state_t, fov, SF_FLOAT),
	SF(player_state_t, rdflags, SF_INT),
	SFA(player_state_t, stats, SF_INT),
	SF_END
};

static savefield_t persfields[] =
{
	SF(client_persistant_t, userinfo, SF_BYTES),
	SF(client_persistant_t, netname, SF_BYTES),
	SF(client_persistant_t, hand, SF_INT),
	SF(client_persistant_t, connected, SF_INT),
	SF(client_persistant_t, health, SF_INT),
	SF(client_persistant_t, max_health, SF_INT),
	SF(client_persistant_t, savedFlags, SF_INT),
	SF(client_persistant_t, selected_item, SF_INT),
	SFA(client_persistant_t, inventory, SF_INT),
	SF(client_persistant_t, max_bullets, SF_INT),
	SF(client_persistant_t, max_shells, SF_INT),
	SF(client_persistant_t, max_rockets, SF_INT),
	SF(client_persistant_t, max_grenades, SF_INT),
	SF(client_persistant_t, max_cells, SF_INT),
	SF(client_persistant_t, max_slugs, SF_INT),
	SF(client_persistant_t, weapon, SF_ITEM),
	SF(client_persistant_t, lastweapon, SF_ITEM),
	SF(client_persistant_t, power_cubes, SF_INT),
	SF(client_persistant_t, score, SF_INT),
	SF(client_persistant_t, game_helpchanged, SF_INT),
	SF(client_persistant_t, helpchanged, SF_INT),
	SF(client_persistant_t, spectator, SF_INT),
	SF_END
};

static savefield_t respfields[] =
{
	SFS(client_respawn_t, coop_respawn, persfields),
	SF(client_respawn_t, enterframe, SF_INT),
	SF(client_respawn_t, score, SF_INT),
	SFA(client_respawn_t, cmd_angles, SF_FLOAT),
	SF(client_respawn_t, spectator, SF_INT),
	SF_END
};

static savefield_t clientfields[] =
{
	SFS(gclient_t, ps, playerstatefields),
	SF(gclient_t, ping, SF_INT),
	SFS(gclient_t, pers, persfields),
	SFS(gclient_t, resp, respfields),
	SFS(gclient_t, old_pmove, pmovefields),
	SF(gclient_t, showscores, SF_INT),
	SF(gclient_t, showinventory, SF_INT),
	SF(gclient_t, showhelp, SF_INT),
	SF(gclient_t, showhelpicon, SF_INT),
	SF(gclient_t, ammo_index, SF_INT),
	SF(gclient_t, buttons, SF_INT),
	SF(gclient_t, oldbuttons, SF_INT),
	SF(gclient_t, latched_buttons, SF_INT),
	SF(gclient_t, weapon_thunk, SF_INT),
	SF(gclient_t, newweapon, SF_ITEM),
	SF(gclient_t, damage_armor, SF_INT),
	SF(gclient_t, damage_parmor, SF_INT),
	SF(gclient_t, damage_blood, SF_INT),
	SF(gclient_t, damage_knockback, SF_INT),
	SFA(gclient_t, damage_from, SF_FLOAT),
	SF(gclient_t, killer_yaw, SF_FLOAT),
	SF(gclient_t, weaponstate, SF_INT),
	SFA(gclient_t, kick_angles, SF_FLOAT),
	SFA(gclient_t, kick_origin, SF_FLOAT),
	SF(gclient_t, v_dmg_roll, SF_FLOAT),
	SF(gclient_t, v_dmg_pitch, SF_FLOAT),
	SF(gclient_t, v_dmg_time, SF_FLOAT),
	SF(gclient_t, fall_time, SF_FLOAT),
	SF(gclient_t, fall_value, SF_FLOAT),
	SF(gclient_t, damage_alpha, SF_FLOAT),
	SF(gclient_t, bonus_alpha, SF_FLOAT),
	SFA(gclient_t, damage_blend, SF_FLOAT),
	SFA(gclient_t, v_angle, SF_FLOAT),
	SF(gclient_t, bobtime, SF_FLOAT),
	SFA(gclient_t, oldviewangles, SF_FLOAT),
	SFA(gclient_t, oldvelocity, SF_FLOAT),
	SF(gclient_t, next_drown_time, SF_FLOAT),
	SF(gclient_t, old_waterlevel, SF_INT),
	SF(gclient_t, breather_sound, SF_INT),
	SF(gclient_t, machinegun_shots, SF_INT),
	SF(gclient_t, anim_end, SF_INT),
	SF(gclient_t, anim_priority, SF_INT),
	SF(gclient_t, anim_duck, SF_INT),
	SF(gclient_t, anim_run, SF_INT),
	SF(gclient_t, quad_framenum, SF_FLOAT),
	SF(gclient_t, invincible_framenum, SF_FLOAT),
	SF(gclient_t, breather_framenum, SF_FLOAT),
	SF(gclient_t, enviro_framenum, SF_FLOAT),
	SF(gclient_t, grenade_blew_up, SF_INT),
	SF(gclient_t, grenade_time, SF_FLOAT),
	SF(gclient_t, silencer_shots, SF_INT),
	SF(gclient_t, weapon_sound, SF_INT),
	SF(gclient_t, pickup_msg_time, SF_FLOAT),
	SF(gclient_t, flood_locktill, SF_FLOAT),
	SFA(gclient_t, flood_when, SF_FLOAT),
	SF(gclient_t, flood_whenhead, SF_INT),
	SF(gclient_t, respawn_time, SF_FLOAT),
	SF(gclient_t, chase_target, SF_EDICT),
	SF(gclient_t, update_chase, SF_INT),
	SF_END
};

static savefield_t moveinfofields[] =
{
	SFA(moveinfo_t, start_origin, SF_FLOAT),
	SFA(moveinfo_t, start_angles, SF_FLOAT),
	SFA(moveinfo_t, end_origin, SF_FLOAT),
	SFA(moveinfo_t, end_angles, SF_FLOAT),
	SF(moveinfo_t, sound_start, SF_INT),
	SF(moveinfo_t, sound_middle, SF_INT),
	SF(moveinfo_t, sound_end, SF_INT),
	SF(moveinfo_t, accel, SF_FLOAT),
	SF(moveinfo_t, speed, SF_FLOAT),
	SF(moveinfo_t, decel, SF_FLOAT),
	SF(moveinfo_t, distance, SF_FLOAT),
	SF(moveinfo_t, wait, SF_FLOAT),
	SF(moveinfo_t, state, SF_INT),
	SFA(moveinfo_t, dir, SF_FLOAT),
	SF(moveinfo_t, current_speed, SF_FLOAT),
	SF(moveinfo_t, move_speed, SF_FLOAT),
	SF(moveinfo_t, next_speed, SF_FLOAT),
	SF(moveinfo_t, remaining_distance, SF_FLOAT),
	SF(moveinfo_t, decel_distance, SF_FLOAT),
	SF(moveinfo_t, endfunc, SF_FUNC),
	SF_END
};

static savefield_t monsterinfofields[] =
{
	SF(monsterinfo_t, currentmove, SF_MMOVE),
	SF(monsterinfo_t, aiflags, SF_INT),
	SF(monsterinfo_t, nextframe, SF_INT),
	SF(monsterinfo_t, scale, SF_FLOAT),
	SF(monsterinfo_t, stand, SF_FUNC),
	SF(monsterinfo_t, idle, SF_FUNC),
	SF(monsterinfo_t, search, SF_FUNC),
	SF(monsterinfo_t, walk, SF_FUNC),
	SF(monsterinfo_t, run, SF_FUNC),
	SF(monsterinfo_t, dodge, SF_FUNC),
	SF(monsterinfo_t, attack, SF_FUNC),
	SF(monsterinfo_t, melee, SF_FUNC),
	SF(monsterinfo_t, sight, SF_FUNC),
	SF(monsterinfo_t, checkattack, SF_FUNC),
	SF(monsterinfo_t, pausetime, SF_FLOAT),
	SF(monsterinfo_t, attack_finished, SF_FLOAT),
	SFA(monsterinfo_t, saved_goal, SF_FLOAT),
	SF(monsterinfo_t, search_time, SF_FLOAT),
	SF(monsterinfo_t, trail_time, SF_FLOAT),
	SFA(monsterinfo_t, last_sighting, SF_FLOAT),
	SF(monsterinfo_t, attack_state, SF_INT),
	SF(monsterinfo_t, lefty, SF_INT),
	SF(monsterinfo_t, idle_time, SF_FLOAT),
	SF(monsterinfo_t, linkcount, SF_INT),
	SF(monsterinfo_t, power_armor_type, SF_INT),
	SF(monsterinfo_t, power_armor_power, SF_INT),
	SF_END
};

static savefield_t edictfields[] =
{
	SFS(edict_t, s, entstatefields),
	SF(edict_t, client, SF_CLIENT),
	SF(edict_t, inuse, SF_INT),
	SF(edict_t, linkcount, SF_INT),
	SF(edict_t, area, SF_IGNORE),
	SF(edict_t, num_clusters, SF_INT),
	SFA(edict_t, clusternums, SF_INT),
	SF(edict_t, headnode, SF_INT),
	SF(edict_t, areanum, SF_INT),
	SF(edict_t, areanum2, SF_INT),
	SF(edict_t, svflags, SF_INT),
	SFA(edict_t, mins, SF_FLOAT),
	SFA(edict_t, maxs, SF_FLOAT),
	SFA(edict_t, absmin, SF_FLOAT),
	SFA(edict_t, absmax, SF_FLOAT),
	SFA(edict_t, size, SF_FLOAT),
	SF(edict_t, solid, SF_INT),
	SF(edict_t, clipmask, SF_INT),
	SF(edict_t, owner, SF_EDICT),
	SF(edict_t, movetype, SF_INT),
	SF(edict_t, flags, SF_INT),
	SF(edict_t, model, SF_LSTRING),
	SF(edict_t, freetime, SF_FLOAT),
	SF(edict_t, message, SF_LSTRING),
	SF(edict_t, classname, SF_LSTRING),
	SF(edict_t, spawnflags, SF_INT),
	SF(edict_t, timestamp, SF_FLOAT),
	SF(edict_t, angle, SF_FLOAT),
	SF(edict_t, target, SF_LSTRING),
	SF(edict_t, targetname, SF_LSTRING),
	SF(edict_t, killtarget, SF_LSTRING),
	SF(edict_t, team, SF_LSTRING),
	SF(edict_t, pathtarget, SF_LSTRING),
	SF(edict_t, deathtarget, SF_LSTRING),
	SF(edict_t, combattarget, SF_LSTRING),
	SF(edict_t, target_ent, SF_EDICT),
	SF(edict_t, speed, SF_FLOAT),
	SF(edict_t, accel, SF_FLOAT),
	SF(edict_t, decel, SF_FLOAT),
	SFA(edict_t, movedir, SF_FLOAT),
	SFA(edict_t, pos1, SF_FLOAT),
	SFA(edict_t, pos2, SF_FLOAT),
	SFA(edict_t, velocity, SF_FLOAT),
	SFA(edict_t, avelocity, SF_FLOAT),
	SF(edict_t, mass, SF_INT),
	SF(edict_t, air_finished, SF_FLOAT),
	SF(edict_t, gravity, SF_FLOAT),
	SF(edict_t, goalentity, SF_EDICT),
	SF(edict_t, movetarget, SF_EDICT),
	SF(edict_t, yaw_speed, SF_FLOAT),
	SF(edict_t, ideal_yaw, SF_FLOAT),
	SF(edict_t, nextthink, SF_FLOAT),
	SF(edict_t, prethink, SF_FUNC),
	SF(edict_t, think, SF_FUNC),
	SF(edict_t, blocked, SF_FUNC),
	SF(edict_t, touch, SF_FUNC),
	SF(edict_t, use, SF_FUNC),
	SF(edict_t, pain, SF_FUNC),
	SF(edict_t, die, SF_FUNC),
	SF(edict_t, touch_debounce_time, SF_FLOAT),
	SF(edict_t, pain_debounce_time, SF_FLOAT),
	SF(edict_t, damage_debounce_time, SF_FLOAT),
	SF(edict_t, fly_sound_debounce_time, SF_FLOAT),
	SF(edict_t, last_move_time, SF_FLOAT),
	SF(edict_t, health, SF_INT),
	SF(edict_t, max_health, SF_INT),
	SF(edict_t, gib_health, SF_INT),
	SF(edict_t, deadflag, SF_INT),
	SF(edict_t, show_hostile, SF_INT),
	SF(edict_t, powerarmor_time, SF_FLOAT),
	SF(edict_t, map, SF_LSTRING),
	SF(edict_t, viewheight, SF_INT),
	SF(edict_t, takedamage, SF_INT),
	SF(edict_t, dmg, SF_INT),
	SF(edict_t, radius_dmg, SF_INT),
	SF(edict_t, dmg_radius, SF_FLOAT),
	SF(edict_t, sounds, SF_INT),
	SF(edict_t, count, SF_INT),
	SF(edict_t, chain, SF_EDICT),
	SF(edict_t, enemy, SF_EDICT),
	SF(edict_t, oldenemy, SF_EDICT),
	SF(edict_t, activator, SF_EDICT),
	SF(edict_t, groundentity, SF_EDICT),
	SF(edict_t, groundentity_linkcount, SF_INT),
	SF(edict_t, teamchain, SF_EDICT),
	SF(edict_t, teammaster, SF_EDICT),
	SF(edict_t, mynoise, SF_EDICT),
	SF(edict_t, mynoise2, SF_EDICT),
	SF(edict_t, noise_index, SF_INT),
	SF(edict_t, noise_index2, SF_INT),
	SF(edict_t, volume, SF_FLOAT),
	SF(edict_t, attenuation, SF_FLOAT),
	SF(edict_t, wait, SF_FLOAT),
	SF(edict_t, delay, SF_FLOAT),
	SF(edict_t, random, SF_FLOAT),
	SF(edict_t, teleport_time, SF_FLOAT),
	SF(edict_t, watertype, SF_INT),
	SF(edict_t, waterlevel, SF_INT),
	SFA(edict_t, move_origin, SF_FLOAT),
	SFA(edict_t, move_angles, SF_FLOAT),
	SF(edict_t, light_level, SF_INT),
	SF(edict_t, style, SF_INT),
	SF(edict_t, item, SF_ITEM),
	SFS(edict_t, moveinfo, moveinfofields),
	SFS(edict_t, monsterinfo, monsterinfofields),
	SF_END
};

static savefield_t gamefields[] =
{
	SF(game_locals_t, helpmessage1, SF_BYTES),
	SF(game_locals_t, helpmessage2, SF_BYTES),
	SF(game_locals_t, helpchanged, SF_INT),
	SF(game_locals_t, clients, SF_IGNORE),
	SF(game_locals_t, spawnpoint, SF_BYTES),
	SF(game_locals_t, maxclients, SF_INT),
	SF(game_locals_t, maxentities, SF_INT),
	SF(game_locals_t, serverflags, SF_INT),
	SF(game_locals_t, num_items, SF_INT),
	SF(game_locals_t, autosaved, SF_INT),
	SF_END
};

static savefield_t levelfields[] =
{
	SF(level_locals_t, framenum, SF_INT),
	SF(level_locals_t, time, SF_FLOAT),
	SF(level_locals_t, level_name, SF_BYTES),
	SF(level_locals_t, mapname, SF_BYTES),
	SF(level_locals_t, nextmap, SF_BYTES),
	SF(level_locals_t, intermissiontime, SF_FLOAT),
	SF(level_locals_t, changemap, SF_LSTRING),
	SF(level_locals_t, exitintermission, SF_INT),
	SFA(level_locals_t, intermission_origin, SF_FLOAT),
	SFA(level_locals_t, intermission_angle, SF_FLOAT),
	SF(level_locals_t, sight_client, SF_EDICT),
	SF(level_locals_t, sight_entity, SF_EDICT),
	SF(level_locals_t, sight_entity_framenum, SF_INT),
	SF(level_locals_t, sound_entity, SF_EDICT),
	SF(level_locals_t, sound_entity_framenum, SF_INT),
	SF(level_locals_t, sound2_entity, SF_EDICT),
	SF(level_locals_t, sound2_entity_framenum, SF_INT),
	SF(level_locals_t, pic_health, SF_INT),
	SF(level_locals_t, total_secrets, SF_INT),
	SF(level_locals_t, found_secrets, SF_INT),
	SF(level_locals_t, total_goals, SF_INT),
	SF(level_locals_t, found_goals, SF_INT),
	SF(level_locals_t, total_monsters, SF_INT),
	SF(level_locals_t, killed_monsters, SF_INT),
	SF(level_locals_t, current_entity, SF_EDICT),
	SF(level_locals_t, body_que, SF_INT),
	SF(level_locals_t, power_cubes, SF_INT),
	SF_END
};

/*
==============
Save_CheckTable

That a table names every byte of its struct but the padding: a field added
to a struct and not to its table would be lost from the saves without a word
==============
*/
static qboolean Save_CheckTable (char *what, savefield_t *t, int size)
{
	static byte	covered[4096];
	qboolean	ok = true;
	int			i, run;

	if (size > sizeof(covered))
	{
		gi.dprintf ("savegame: %s is %d bytes, more than the check takes\n", what, size);
		return false;
	}
	memset (covered, 0, size);
	for ( ; t->name ; t++)
	{
		if (t->ofs + t->size > size || t->size % t->elem)
		{
			gi.dprintf ("savegame: %s.%s does not fit\n", what, t->name);
			ok = false;
			continue;
		}
		for (i=t->ofs ; i<t->ofs+t->size ; i++)
		{
			if (covered[i])
			{
				gi.dprintf ("savegame: %s.%s overlaps another field\n", what, t->name);
				ok = false;
				break;
			}
			covered[i] = 1;
		}
		if (t->type == SF_STRUCT)
			ok &= Save_CheckTable (t->name, t->sub, t->elem);
	}
	// padding is at most 3 bytes, before an int or at the end
	for (i=0, run=0 ; i<=size ; i++)
	{
		if (i < size && !covered[i])
			run++;
		else
		{
			if (run > 3)
			{
				gi.dprintf ("savegame: %s: %d bytes at %d are in no field\n", what, run, i - run);
				ok = false;
			}
			run = 0;
		}
	}
	return ok;
}

static unsigned	save_hash;

static void Save_HashTable (savefield_t *t)
{
	char	*c;

	for ( ; t->name ; t++)
	{
		for (c=t->name ; *c ; c++)
			save_hash = (save_hash ^ (byte)*c) * 16777619u;
		save_hash = (save_hash ^ (unsigned)(t->type * 4096 + t->size / t->elem)) * 16777619u;
		if (t->type == SF_STRUCT)
			Save_HashTable (t->sub);
	}
}

/*
==============
Save_Init

The tables checked, the layout's hash, the names sorted for the lookups
==============
*/
static savesym_t	*funcs_byaddr, *moves_byaddr;
static int			num_funcs, num_moves;

static int Save_CmpName (const void *a, const void *b)
{
	return strcmp (((savesym_t *)a)->name, ((savesym_t *)b)->name);
}

static int Save_CmpAddr (const void *a, const void *b)
{
	byte	*pa = ((savesym_t *)a)->addr, *pb = ((savesym_t *)b)->addr;

	return pa < pb ? -1 : pa > pb;
}

static savesym_t *Save_SortedCopy (savesym_t *t, int *count)
{
	savesym_t	*copy;
	int			n;

	for (n=0 ; t[n].name ; n++)
		;
	qsort (t, n, sizeof(t[0]), Save_CmpName);
	copy = malloc (n * sizeof(t[0]));
	memcpy (copy, t, n * sizeof(t[0]));
	qsort (copy, n, sizeof(t[0]), Save_CmpAddr);
	*count = n;
	return copy;
}

static void Save_Init (void)
{
	static qboolean	done;

	if (done)
		return;
	done = true;

	Save_CheckTable ("game_locals_t", gamefields, sizeof(game_locals_t));
	Save_CheckTable ("level_locals_t", levelfields, sizeof(level_locals_t));
	Save_CheckTable ("gclient_t", clientfields, sizeof(gclient_t));
	Save_CheckTable ("edict_t", edictfields, sizeof(edict_t));

	save_hash = 2166136261u;
	Save_HashTable (gamefields);
	Save_HashTable (levelfields);
	Save_HashTable (clientfields);
	Save_HashTable (edictfields);

	funcs_byaddr = Save_SortedCopy (savefuncs, &num_funcs);
	moves_byaddr = Save_SortedCopy (savemoves, &num_moves);
}

static savesym_t *Save_FindAddr (savesym_t *t, int n, void *addr)
{
	savesym_t	key;

	key.addr = addr;
	return bsearch (&key, t, n, sizeof(t[0]), Save_CmpAddr);
}

static savesym_t *Save_FindName (savesym_t *t, int n, char *name)
{
	savesym_t	key;

	key.name = name;
	return bsearch (&key, t, n, sizeof(t[0]), Save_CmpName);
}

//=========================================================

static void Save_Long (FILE *f, unsigned v)
{
	byte	b[4];

	b[0] = v >> 24;
	b[1] = v >> 16;
	b[2] = v >> 8;
	b[3] = v;
	fwrite (b, 4, 1, f);
}

static unsigned Load_Long (FILE *f)
{
	byte	b[4];

	if (fread (b, 4, 1, f) != 1)
		gi.error ("Savegame is cut short");
	return ((unsigned)b[0] << 24) | ((unsigned)b[1] << 16) | ((unsigned)b[2] << 8) | b[3];
}

static void Save_String (FILE *f, const char *s)
{
	int		len;

	len = s ? strlen(s) + 1 : 0;
	Save_Long (f, len);
	if (len)
		fwrite (s, len, 1, f);
}

// NULL for none; buf of MAX_QPATH, or a TAG_LEVEL copy when buf is NULL
static char *Load_String (FILE *f, char *buf)
{
	int		len;
	char	*s;

	len = Load_Long (f);
	if (!len)
		return NULL;
	if (len < 0 || len > 65536 || (buf && len > MAX_QPATH))
		gi.error ("Savegame is damaged (a string of %d bytes)", len);
	s = buf ? buf : gi.TagMalloc (len, TAG_LEVEL);
	if (fread (s, len, 1, f) != 1)
		gi.error ("Savegame is cut short");
	s[len-1] = 0;
	return s;
}

static int Save_Index (void *p, void *base, int elemsize)
{
	if (!p)
		return -1;
	return ((byte *)p - (byte *)base) / elemsize;
}

static void Save_Fields (FILE *f, savefield_t *t, byte *base)
{
	byte		*p;
	savesym_t	*sym;
	unsigned	v;
	int			i;

	for ( ; t->name ; t++)
	{
		p = base + t->ofs;
		switch (t->type)
		{
		case SF_INT:
			for (i=0 ; i<t->size ; i+=t->elem)
			{
				if (t->elem == 1)
					v = p[i];
				else if (t->elem == 2)
					v = (unsigned)*(short *)(p + i);
				else
					v = *(unsigned *)(p + i);
				Save_Long (f, v);
			}
			break;
		case SF_FLOAT:
			for (i=0 ; i<t->size ; i+=4)
			{
				memcpy (&v, p + i, 4);
				Save_Long (f, v);
			}
			break;
		case SF_BYTES:
			fwrite (p, t->size, 1, f);
			break;
		case SF_LSTRING:
			Save_String (f, *(char **)p);
			break;
		case SF_EDICT:
			Save_Long (f, Save_Index (*(edict_t **)p, g_edicts, sizeof(edict_t)));
			break;
		case SF_CLIENT:
			Save_Long (f, Save_Index (*(gclient_t **)p, game.clients, sizeof(gclient_t)));
			break;
		case SF_ITEM:
			Save_Long (f, Save_Index (*(gitem_t **)p, itemlist, sizeof(gitem_t)));
			break;
		case SF_FUNC:
		case SF_MMOVE:
			if (!*(void **)p)
				sym = NULL;
			else
			{
				if (t->type == SF_FUNC)
					sym = Save_FindAddr (funcs_byaddr, num_funcs, *(void **)p);
				else
					sym = Save_FindAddr (moves_byaddr, num_moves, *(void **)p);
				if (!sym)
					gi.dprintf ("savegame: %s %p has no name, saved as none\n", t->name, *(void **)p);
			}
			Save_String (f, sym ? sym->name : NULL);
			break;
		case SF_IGNORE:
			break;
		case SF_STRUCT:
			Save_Fields (f, t->sub, p);
			break;
		}
	}
}

static void *Load_Pointer (FILE *f, void *base, int elemsize, int count, char *what)
{
	int		index;

	index = Load_Long (f);
	if (index == -1)
		return NULL;
	if (index < 0 || index >= count)
		gi.error ("Savegame is damaged (%s %d of %d)", what, index, count);
	return (byte *)base + index * elemsize;
}

static void Load_Fields (FILE *f, savefield_t *t, byte *base)
{
	byte		*p;
	savesym_t	*sym;
	unsigned	v;
	char		name[MAX_QPATH];
	int			i;

	for ( ; t->name ; t++)
	{
		p = base + t->ofs;
		switch (t->type)
		{
		case SF_INT:
			for (i=0 ; i<t->size ; i+=t->elem)
			{
				v = Load_Long (f);
				if (t->elem == 1)
					p[i] = v;
				else if (t->elem == 2)
					*(short *)(p + i) = v;
				else
					*(unsigned *)(p + i) = v;
			}
			break;
		case SF_FLOAT:
			for (i=0 ; i<t->size ; i+=4)
			{
				v = Load_Long (f);
				memcpy (p + i, &v, 4);
			}
			break;
		case SF_BYTES:
			if (fread (p, t->size, 1, f) != 1)
				gi.error ("Savegame is cut short");
			break;
		case SF_LSTRING:
			*(char **)p = Load_String (f, NULL);
			break;
		case SF_EDICT:
			*(edict_t **)p = Load_Pointer (f, g_edicts, sizeof(edict_t), game.maxentities, t->name);
			break;
		case SF_CLIENT:
			*(gclient_t **)p = Load_Pointer (f, game.clients, sizeof(gclient_t), game.maxclients, t->name);
			break;
		case SF_ITEM:
			*(gitem_t **)p = Load_Pointer (f, itemlist, sizeof(gitem_t), game.num_items, t->name);
			break;
		case SF_FUNC:
		case SF_MMOVE:
			if (!Load_String (f, name))
			{
				*(void **)p = NULL;
				break;
			}
			if (t->type == SF_FUNC)
				sym = Save_FindName (savefuncs, num_funcs, name);
			else
				sym = Save_FindName (savemoves, num_moves, name);
			if (!sym)
				gi.error ("Savegame needs %s, which this game does not have", name);
			*(void **)p = sym->addr;
			break;
		case SF_IGNORE:
			break;
		case SF_STRUCT:
			Load_Fields (f, t->sub, p);
			break;
		}
	}
}

static void Save_Header (FILE *f)
{
	fwrite (SAVE_MAGIC, 8, 1, f);
	Save_Long (f, save_hash);
}

static void Load_Header (FILE *f)
{
	char	magic[8];

	if (fread (magic, 8, 1, f) != 1 || memcmp (magic, SAVE_MAGIC, 8))
	{
		fclose (f);
		gi.error ("Savegame from another version of the game");
	}
	if (Load_Long (f) != save_hash)
	{
		fclose (f);
		gi.error ("Savegame from a version with other fields");
	}
}

/*
============
WriteGame

This will be called whenever the game goes to a new level,
and when the user explicitly saves the game.

Game information include cross level data, like multi level
triggers, help computer info, and all client states.

A single player death will automatically restore from the
last save position.
============
*/
void WriteGame (char *filename, qboolean autosave)
{
	FILE	*f;
	int		i;

	Save_Init ();
	if (!autosave)
		SaveClientData ();

	f = fopen (filename, "wb");
	if (!f)
		gi.error ("Couldn't open %s", filename);

	Save_Header (f);
	game.autosaved = autosave;
	Save_Fields (f, gamefields, (byte *)&game);
	game.autosaved = false;

	for (i=0 ; i<game.maxclients ; i++)
		Save_Fields (f, clientfields, (byte *)&game.clients[i]);

	fclose (f);
}

void ReadGame (char *filename)
{
	FILE	*f;
	int		i;

	Save_Init ();
	gi.FreeTags (TAG_GAME);

	f = fopen (filename, "rb");
	if (!f)
		gi.error ("Couldn't open %s", filename);

	Load_Header (f);

	g_edicts =  gi.TagMalloc (game.maxentities * sizeof(g_edicts[0]), TAG_GAME);
	globals.edicts = g_edicts;

	Load_Fields (f, gamefields, (byte *)&game);
	game.clients = gi.TagMalloc (game.maxclients * sizeof(game.clients[0]), TAG_GAME);
	for (i=0 ; i<game.maxclients ; i++)
		Load_Fields (f, clientfields, (byte *)&game.clients[i]);

	fclose (f);
}

//==========================================================

/*
=================
WriteLevel

=================
*/
void WriteLevel (char *filename)
{
	int		i;
	edict_t	*ent;
	FILE	*f;

	Save_Init ();
	f = fopen (filename, "wb");
	if (!f)
		gi.error ("Couldn't open %s", filename);

	Save_Header (f);

	// write out level_locals_t
	Save_Fields (f, levelfields, (byte *)&level);

	// write out all the entities
	for (i=0 ; i<globals.num_edicts ; i++)
	{
		ent = &g_edicts[i];
		if (!ent->inuse)
			continue;
		Save_Long (f, i);
		Save_Fields (f, edictfields, (byte *)ent);
	}
	Save_Long (f, -1);

	fclose (f);
}


/*
=================
ReadLevel

SpawnEntities will allready have been called on the
level the same way it was when the level was saved.

That is necessary to get the baselines
set up identically.

The server will have cleared all of the world links before
calling ReadLevel.

No clients are connected yet.
=================
*/
void ReadLevel (char *filename)
{
	int		entnum;
	FILE	*f;
	int		i;
	edict_t	*ent;

	Save_Init ();
	f = fopen (filename, "rb");
	if (!f)
		gi.error ("Couldn't open %s", filename);

	// free any dynamic memory allocated by loading the level
	// base state
	gi.FreeTags (TAG_LEVEL);

	// wipe all the entities
	memset (g_edicts, 0, game.maxentities*sizeof(g_edicts[0]));
	globals.num_edicts = maxclients->value+1;

	Load_Header (f);

	// load the level locals
	Load_Fields (f, levelfields, (byte *)&level);

	// load all the entities
	while (1)
	{
		entnum = Load_Long (f);
		if (entnum == -1)
			break;
		if (entnum < 0 || entnum >= game.maxentities)
		{
			fclose (f);
			gi.error ("ReadLevel: entity %d of %d", entnum, game.maxentities);
		}
		if (entnum >= globals.num_edicts)
			globals.num_edicts = entnum+1;

		ent = &g_edicts[entnum];
		Load_Fields (f, edictfields, (byte *)ent);

		// let the server rebuild world links for this ent
		memset (&ent->area, 0, sizeof(ent->area));
		gi.linkentity (ent);
	}

	fclose (f);

	// mark all clients as unconnected
	for (i=0 ; i<maxclients->value ; i++)
	{
		ent = &g_edicts[i+1];
		ent->client = game.clients + i;
		ent->client->pers.connected = false;
	}

	// do any load time things at this point
	for (i=0 ; i<globals.num_edicts ; i++)
	{
		ent = &g_edicts[i];

		if (!ent->inuse)
			continue;

		// fire any cross-level triggers
		if (ent->classname)
			if (strcmp(ent->classname, "target_crosslevel_target") == 0)
				ent->nextthink = level.time + ent->delay;
	}
}
