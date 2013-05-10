/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "g_local.h"

/*
 * @brief Inspect all damage received this frame and play a pain sound if appropriate.
 */
static void G_ClientDamage(g_edict_t *ent) {
	g_client_t *client;

	client = ent->client;

	if (client->locals.damage_health || client->locals.damage_armor) {
		// play an appropriate pain sound
		if (g_level.time > client->locals.pain_time) {
			vec3_t org;
			int32_t l;

			client->locals.pain_time = g_level.time + 700;

			if (ent->locals.health < 25)
				l = 25;
			else if (ent->locals.health < 50)
				l = 50;
			else if (ent->locals.health < 75)
				l = 75;
			else
				l = 100;

			VectorAdd(client->ps.pm_state.origin, client->ps.pm_state.view_offset, org);
			VectorScale(org, 0.125, org);

			gi.PositionedSound(org, ent, gi.SoundIndex(va("*pain%i_1", l)), ATTN_NORM);
		}
	}

	// clear totals
	client->locals.damage_health = 0;
	client->locals.damage_armor = 0;
	client->locals.damage_inflicted = 0;
}

/*
 * @brief Handles water entry and exit
 */
static void G_ClientWaterInteraction(g_edict_t *ent) {
	g_client_t *client = ent->client;

	if (ent->locals.move_type == MOVE_TYPE_NO_CLIP) {
		client->locals.drown_time = g_level.time + 12000; // don't need air
		return;
	}

	const int32_t water_level = ent->locals.water_level;
	const int32_t old_water_level = ent->locals.old_water_level;

	ent->locals.old_water_level = water_level;

	// if just entered a water volume, play a sound
	if (!old_water_level && water_level)
		gi.Sound(ent, gi.SoundIndex("world/water_in"), ATTN_NORM);

	// completely exited the water
	if (old_water_level && !water_level)
		gi.Sound(ent, gi.SoundIndex("world/water_out"), ATTN_NORM);

	// head just coming out of water, play a gasp if we were down for a while
	if (old_water_level == 3 && water_level != 3 && (client->locals.drown_time - g_level.time)
			< 8000) {
		vec3_t org;

		VectorAdd(client->ps.pm_state.origin, client->ps.pm_state.view_offset, org);
		VectorScale(org, 0.125, org);

		gi.PositionedSound(org, ent, gi.SoundIndex("*gasp_1"), ATTN_NORM);
	}

	// check for drowning
	if (water_level != 3) { // take some air, push out drown time
		client->locals.drown_time = g_level.time + 12000;
		ent->locals.dmg = 0;
	} else { // we're under water
		if (client->locals.drown_time < g_level.time && ent->locals.health > 0) {
			client->locals.drown_time = g_level.time + 1000;

			// take more damage the longer under water
			ent->locals.dmg += 2;

			if (ent->locals.dmg > 15)
				ent->locals.dmg = 15;

			// play a gurp sound instead of a normal pain sound
			if (ent->locals.health <= ent->locals.dmg)
				ent->s.event = EV_CLIENT_DROWN;
			else
				ent->s.event = EV_CLIENT_GURP;

			// suppress normal pain sound
			client->locals.pain_time = g_level.time;

			// and apply the damage
			G_Damage(ent, NULL, NULL, vec3_origin, ent->s.origin, vec3_origin, ent->locals.dmg, 0,
					DAMAGE_NO_ARMOR, MOD_WATER);
		}
	}

	// check for sizzle damage
	if (water_level && (ent->locals.water_type & (CONTENTS_LAVA | CONTENTS_SLIME))) {
		if (client->locals.sizzle_time <= g_level.time && ent->locals.health > 0) {

			client->locals.sizzle_time = g_level.time + 200;

			if (ent->locals.water_type & CONTENTS_LAVA) {
				G_Damage(ent, NULL, NULL, vec3_origin, ent->s.origin, vec3_origin, 2 * water_level,
						0, DAMAGE_NO_ARMOR, MOD_LAVA);
			}

			if (ent->locals.water_type & CONTENTS_SLIME) {
				G_Damage(ent, NULL, NULL, vec3_origin, ent->s.origin, vec3_origin, 1 * water_level,
						0, DAMAGE_NO_ARMOR, MOD_SLIME);
			}
		}
	}
}

/*
 * @brief Set the angles of the client's world model, after clamping them to sane
 * values.
 */
static void G_ClientWorldAngles(g_edict_t *ent) {

	ent->s.angles[PITCH] = ent->client->locals.angles[PITCH] / 3.0;
	ent->s.angles[YAW] = ent->client->locals.angles[YAW];

	// set roll based on lateral velocity and ground entity
	const vec_t dot = DotProduct(ent->locals.velocity, ent->client->locals.right);

	ent->s.angles[ROLL] = ent->locals.ground_entity ? dot * 0.025 : dot * 0.005;

	// check for footsteps
	if (ent->locals.ground_entity && ent->locals.move_type == MOVE_TYPE_WALK && !ent->s.event) {

		if (ent->client->locals.speed > 250.0 && ent->client->locals.footstep_time < g_level.time) {
			ent->client->locals.footstep_time = g_level.time + 275;
			ent->s.event = EV_CLIENT_FOOTSTEP;
		}
	}
}

#define KICK_SCALE 15.0

/*
 * @brief Adds view kick in the specified direction to the specified client.
 */
void G_ClientDamageKick(g_edict_t *ent, const vec3_t dir, const vec_t kick) {
	vec3_t old_kick_angles, kick_angles;

	UnpackAngles(ent->client->ps.pm_state.kick_angles, old_kick_angles);

	VectorClear(kick_angles);
	kick_angles[PITCH] = DotProduct(dir, ent->client->locals.forward) * kick * KICK_SCALE;
	kick_angles[ROLL] = DotProduct(dir, ent->client->locals.right) * kick * KICK_SCALE;

	//gi.Print("kicked %s from %s at %1.2f\n", vtos(kick_angles), vtos(dir), kick);
	VectorAdd(old_kick_angles, kick_angles, kick_angles);
	PackAngles(kick_angles, ent->client->ps.pm_state.kick_angles);
}

/*
 * @brief A convenience function for adding view kick from firing weapons.
 */
void G_ClientWeaponKick(g_edict_t *ent, const vec_t kick) {
	vec3_t dir;

	VectorScale(ent->client->locals.forward, -1.0, dir);

	G_ClientDamageKick(ent, dir, kick);
}

/*
 * @brief Sets the kick value based on recent events such as falling. Firing of
 * weapons may also set the kick value, and we factor that in here as well.
 */
static void G_ClientKickAngles(g_edict_t *ent) {
	int16_t *kick_angles = ent->client->ps.pm_state.kick_angles;

	vec3_t kick;
	UnpackAngles(kick_angles, kick);

	// add in any event-based feedback

	switch (ent->s.event) {
		case EV_CLIENT_LAND:
			kick[PITCH] += 2.5;
			break;
		case EV_CLIENT_JUMP:
			kick[PITCH] -= 1.5;
			break;
		case EV_CLIENT_FALL:
			kick[PITCH] += 5.0;
			break;
		case EV_CLIENT_FALL_FAR:
			kick[PITCH] += 10.0;
			break;
		default:
			break;
	}

	// and any velocity-based feedback

	vec_t forward = DotProduct(ent->locals.velocity, ent->client->locals.forward);
	kick[PITCH] += forward / 500.0;

	vec_t right = DotProduct(ent->locals.velocity, ent->client->locals.right);
	kick[ROLL] += right / 500.0;

	// now interpolate the kick angles towards neutral over time

	vec_t delta = VectorLength(kick);

	if (!delta) // no kick, we're done
		return;

	// we recover from kick at a rate based to the kick itself

	delta = 0.5 + delta * delta * gi.frame_seconds;

	int32_t i;
	for (i = 0; i < 3; i++) {

		// clear angles smaller than our delta to avoid oscillations
		if (fabs(kick[i]) <= delta) {
			kick[i] = 0.0;
		} else if (kick[i] > 0.0) {
			kick[i] -= delta;
		} else {
			kick[i] += delta;
		}

	}

	PackAngles(kick, kick_angles);
}

/*
 * @brief Sets the animation sequences for the specified entity. This is called
 * towards the end of each frame, after our ground entity and water level have
 * been resolved.
 */
static void G_ClientAnimation(g_edict_t *ent) {

	if (ent->sv_flags & SVF_NO_CLIENT)
		return;

	// no-clippers do not animate

	if (ent->locals.move_type == MOVE_TYPE_NO_CLIP) {
		G_SetAnimation(ent, ANIM_TORSO_STAND1, false);
		G_SetAnimation(ent, ANIM_LEGS_JUMP1, false);
		return;
	}

	// check for falling

	g_client_locals_t *cl = &ent->client->locals;
	if (!ent->locals.ground_entity) { // not on the ground

		if (g_level.time - cl->jump_time > 400) {
			if (ent->locals.water_level == 3 && cl->speed > 10.0) { // swimming
				G_SetAnimation(ent, ANIM_LEGS_SWIM, false);
				return;
			}
			if (ent->client->ps.pm_state.pm_flags & PMF_DUCKED) { // ducking
				G_SetAnimation(ent, ANIM_LEGS_IDLECR, false);
				return;
			}
		}

		_Bool jumping = G_IsAnimation(ent, ANIM_LEGS_JUMP1);
		jumping |= G_IsAnimation(ent, ANIM_LEGS_JUMP2);

		if (!jumping)
			G_SetAnimation(ent, ANIM_LEGS_JUMP1, false);

		return;
	}

	// duck, walk or run after landing

	if (g_level.time - 400 > cl->land_time && g_level.time - 50 > cl->ground_time) {

		if (ent->client->ps.pm_state.pm_flags & PMF_DUCKED) { // ducked
			if (cl->speed < 1.0)
				G_SetAnimation(ent, ANIM_LEGS_IDLECR, false);
			else
				G_SetAnimation(ent, ANIM_LEGS_WALKCR, false);

			return;
		}

		if (cl->speed < 1.0 && !cl->cmd.forward && !cl->cmd.right && !cl->cmd.up) {
			G_SetAnimation(ent, ANIM_LEGS_IDLE, false);
			return;
		}

		vec3_t angles, forward;

		VectorSet(angles, 0.0, ent->s.angles[YAW], 0.0);
		AngleVectors(angles, forward, NULL, NULL);

		if (DotProduct(ent->locals.velocity, forward) < -0.1)
			G_SetAnimation(ent, ANIM_LEGS_BACK, false);
		else if (cl->speed < 200.0)
			G_SetAnimation(ent, ANIM_LEGS_WALK, false);
		else
			G_SetAnimation(ent, ANIM_LEGS_RUN, false);

		return;
	}
}

/*
 * @brief Called for each client at the end of the server frame.
 */
void G_ClientEndFrame(g_edict_t *ent) {

	g_client_t *client = ent->client;

	// If the origin or velocity have changed since G_ClientThink(),
	// update the pmove values. This will happen when the client
	// is pushed by a bsp model or kicked by an explosion.
	//
	// If it wasn't updated here, the view position would lag a frame
	// behind the body position when pushed -- "sinking into plats"
	PackPosition(ent->s.origin, client->ps.pm_state.origin);
	PackPosition(ent->locals.velocity, client->ps.pm_state.velocity);

	// If in intermission, just set stats and scores and return
	if (g_level.intermission_time) {
		G_ClientStats(ent);
		G_ClientScores(ent);
		return;
	}

	// set the stats for this client
	if (ent->client->locals.persistent.spectator)
		G_ClientSpectatorStats(ent);
	else
		G_ClientStats(ent);

	// check for water entry / exit, burn from lava, slime, etc
	G_ClientWaterInteraction(ent);

	// apply all the damage taken this frame
	G_ClientDamage(ent);

	// set the kick angle for the view
	G_ClientKickAngles(ent);

	// and the angles on the world model
	G_ClientWorldAngles(ent);

	// update the player's animations
	G_ClientAnimation(ent);

	// if the scoreboard is up, update it
	if (ent->client->locals.show_scores)
		G_ClientScores(ent);
}

/*
 * @brief
 */
void G_EndClientFrames(void) {
	int32_t i;
	g_edict_t *ent;

	// finalize the player_state_t for this frame
	for (i = 0; i < sv_max_clients->integer; i++) {

		ent = g_game.edicts + 1 + i;

		if (!ent->in_use || !ent->client)
			continue;

		G_ClientEndFrame(ent);
	}
}
