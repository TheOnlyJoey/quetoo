/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
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

#include "cg_local.h"

static cg_particle_t *cg_free_particles; // list of free particles
static cg_particles_t *cg_active_particles; // list of active particles, by image

static cg_particle_t cg_particles[MAX_PARTICLES];

static r_atlas_t *cg_particle_atlas;

/**
 * @brief Pushes the particle onto the head of specified list.
 */
static void Cg_PushParticle(cg_particle_t *p, cg_particle_t **list) {

	p->prev = NULL;

	if (*list) {
		(*list)->prev = p;
	}

	p->next = *list;
	*list = p;
}

/**
 * @brief Pops the particle from the specified list, repairing the list if it
 * becomes broken.
 */
static void Cg_PopParticle(cg_particle_t *p, cg_particle_t **list) {

	if (p->prev) {
		p->prev->next = p->next;
	}

	if (p->next) {
		p->next->prev = p->prev;
	}

	if (*list == p) {
		*list = p->next;
	}

	p->prev = p->next = NULL;
}

/**
 * @brief Allocates a free particle with the specified type and image.
 */
cg_particle_t *Cg_AllocParticle(const r_particle_type_t type, cg_particles_t *particles) {

	if (!cg_add_particles->integer) {
		return NULL;
	}

	if (!cg_free_particles) {
		cgi.Debug("No free particles\n");
		return NULL;
	}

	cg_particle_t *p = cg_free_particles;

	Cg_PopParticle(p, &cg_free_particles);

	particles = particles ? particles : cg_particles_normal;

	p->part.type = type;
	p->part.image = particles->image;

	p->color_end[3] = p->color_start[3] = p->part.color[3] = 1.0;

	Cg_PushParticle(p, &particles->particles);

	p->start = cgi.client->unclamped_time;
	p->lifetime = PARTICLE_INFINITE;

	return p;
}

/**
 * @brief Frees the specified particle, returning the particle it was pointing
 * to as a convenience for continued iteration.
 */
static cg_particle_t *Cg_FreeParticle(cg_particle_t *p, cg_particle_t **list) {
	cg_particle_t *next = p->next;

	if (list) {
		Cg_PopParticle(p, list);
	}

	memset(p, 0, sizeof(cg_particle_t));

	p->part.blend = GL_ONE;
	Vector4Set(p->part.color, 1.0, 1.0, 1.0, 1.0);
	p->part.scale = 1.0;

	Cg_PushParticle(p, &cg_free_particles);

	return next;
}

/**
 * @brief Allocates a particles chain for the specified image.
 */
cg_particles_t *Cg_AllocParticles(const r_image_t *image, const _Bool use_atlas) {
	cg_particles_t *particles;

	particles = cgi.Malloc(sizeof(*particles), MEM_TAG_CGAME);

	if (use_atlas) {
		particles->original_image = image;
		cgi.AddImageToAtlas(cg_particle_atlas, image);
	} else {
		particles->image = image;
	}

	particles->next = cg_active_particles;
	cg_active_particles = particles;

	return particles;
}

/**
 * @brief Initializes particle subsystem
 */
void Cg_InitParticles(void) {

	cg_particle_atlas = cgi.CreateAtlas("cg_particle_atlas");
}

/**
 * @brief Called when all particle images are done loading.
 */
void Cg_SetupParticleAtlas(void) {

	cgi.CompileAtlas(cg_particle_atlas);

	cg_particles_t *ps = cg_active_particles;
	while (ps) {

		if (ps->original_image) {
			ps->image = (const r_image_t *) cgi.GetAtlasImageFromAtlas(cg_particle_atlas, ps->original_image);
		}

		ps = ps->next;
	}
}

/**
 * @brief Frees all particles, returning them to the eligible list.
 */
void Cg_FreeParticles(void) {
	uint32_t i;

	cg_free_particles = NULL;
	cg_active_particles = NULL;

	memset(cg_particles, 0, sizeof(cg_particles));

	for (i = 0; i < lengthof(cg_particles); i++) {
		Cg_FreeParticle(&cg_particles[i], NULL);
	}

	cg_particle_atlas = NULL;
}

/**
 * @brief
 */
static _Bool Cg_UpdateParticle_Weather(cg_particle_t *p, const vec_t delta, const vec_t delta_squared) {

	// free up weather particles that have hit the ground
	if (p->part.org[2] <= p->weather.end_z) {
		return true;
	}

	return false;
}

/**
 * @brief
 */
static _Bool Cg_UpdateParticle_Spark(cg_particle_t *p, const vec_t delta, const vec_t delta_squared) {

	VectorMA(p->part.org, p->spark.length, p->vel, p->part.end);

	return false;
}

/**
 * @brief
 */
static void Cg_UpdateParticleSpecial_Blood(cg_particle_t *p) {

	if (p->blood.time < cgi.client->unclamped_time) {

		cgi.AddStain(&(const r_stain_t) {
			.origin = {
				p->part.org[0] + Randomc() * 8.0,
				p->part.org[1] + Randomc() * 8.0,
				p->part.org[2] + Randomc() * 8.0
			},
			.color = {
				0.6 + Randomc() * 0.1,
				0.0,
				0.0,
				0.25 + Randomf() * 0.125
			},
			.radius = p->part.scale * 0.125
		});

		p->blood.time = cgi.client->unclamped_time + 96 + (Randomf() * 96);
	}
}

/**
 * @brief Adds all particles that are active for this frame to the view.
 */
void Cg_AddParticles(void) {
	static uint32_t ticks;

	if (!cg_add_particles->value) {
		return;
	}

	if (ticks > cgi.client->unclamped_time) {
		ticks = 0;
	}

	const vec_t delta = (cgi.client->unclamped_time - ticks) * 0.001;
	const vec_t delta_squared = delta * delta;
	_Bool cull;

	ticks = cgi.client->unclamped_time;

	cg_particles_t *ps = cg_active_particles;
	while (ps) {

		cg_particle_t *p = ps->particles;
		while (p) {
			// update any particles allocated in previous frames
			if (p->start != cgi.client->unclamped_time) {

				// calculate where we are in time
				if (p->lifetime) {

					// get rid of expired particles
					if (cgi.client->unclamped_time >= p->start + (p->lifetime - 1)) {
						p = Cg_FreeParticle(p, &ps->particles);
						continue;
					}

					const vec_t frac = (cgi.client->unclamped_time - p->start) / (vec_t) (p->lifetime - 1);

					if (p->effects & PARTICLE_EFFECT_COLOR) {
						Vector4Lerp(p->color_start, p->color_end, frac, p->part.color);

						if (p->part.color[3] > 1.0) {
							p->part.color[3] = 1.0;
						}
					}

					if (p->effects & PARTICLE_EFFECT_SCALE) {
						p->part.scale = Lerp(p->scale_start, p->scale_end, frac);
					}
				}

				// free up particles that have disappeared
				if (p->part.color[3] <= 0.0 || p->part.scale <= 0.0) {
					p = Cg_FreeParticle(p, &ps->particles);
					continue;
				}

				for (int32_t i = 0; i < 3; i++) { // update origin and acceleration
					p->part.org[i] += p->vel[i] * delta + p->accel[i] * delta_squared;
					p->vel[i] += p->accel[i] * delta;
				}

				_Bool free = false;

				switch (p->part.type) {
					case PARTICLE_WEATHER:
						free = Cg_UpdateParticle_Weather(p, delta, delta_squared);
						break;
					case PARTICLE_SPARK:
						free = Cg_UpdateParticle_Spark(p, delta, delta_squared);
						break;
					default:
						break;
				}

				switch (p->special) {
					case PARTICLE_SPECIAL_BLOOD:
						Cg_UpdateParticleSpecial_Blood(p);
						break;
					default:
						break;
				}

				if (free) {
					p = Cg_FreeParticle(p, &ps->particles);
					continue;
				}
			} else {

				if (p->effects & PARTICLE_EFFECT_COLOR) {
					Vector4Copy(p->color_start, p->part.color);

					if (p->part.color[3] > 1.0) {
						p->part.color[3] = 1.0;
					}
				}

				if (p->effects & PARTICLE_EFFECT_SCALE) {
					p->part.scale = p->scale_start;
				}
			}

			// add the particle if it's visible on our screen
			if (p->part.type == PARTICLE_BEAM || p->part.type == PARTICLE_SPARK) {
				vec3_t distance, center;

				VectorSubtract(p->part.end, p->part.org, distance);
				VectorMA(p->part.org, 0.5, distance, center);
				const vec_t radius = VectorLength(distance);
				cull = cgi.CullSphere(center, radius);
			} else {
				const vec_t radius = p->part.scale * 0.5;

				cull = cgi.CullSphere(p->part.org, radius);
			}

			if (!cull) {
				cgi.AddParticle(&p->part);
			}

			p = p->next;
		}

		ps = ps->next;
	}
}
