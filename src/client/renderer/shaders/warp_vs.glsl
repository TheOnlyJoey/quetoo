/**
 * @brief Warp vertex shader.
 */

#version 330

#define VERTEX_SHADER

#include "fog_inc.glsl"
#include "matrix_inc.glsl"

out vec2 texcoord;

in vec3 POSITION;
in vec2 TEXCOORD;

/**
 * @brief Program entry point.
 */
void main(void) {

	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * MODELVIEW_MAT * vec4(POSITION, 1.0);

	// pass texcoords through
	texcoord = TEXCOORD;

	FogVertex();
}
