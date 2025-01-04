#include "RayTransmitter.h"
#include "MeshModel.h"
#include "Scene.h"
#include "Renderer.h"

std::vector<vec3> RayTransmitter::getNrays(uint n)
{
	const float goldenRatio = (1.0f + std::sqrt(5.0f)) / 2.0f;
	const float angleIncrement = 2.0f * M_PI / goldenRatio;
	uint total_points = 2 * n;
	for (uint i = 0; i < total_points; ++i) {
		// Calculate the vertical angle using the Fibonacci spiral
		float t = static_cast<float>(i) / static_cast<float>(total_points); // normalized [0, 1)
		float theta = std::acos(1.0f - 2.0f * t); // Elevation angle

		// Calculate the azimuth angle
		float phi = angleIncrement * static_cast<float>(i);

		// Convert spherical coordinates to Cartesian coordinates
		float x = std::sin(theta) * std::cos(phi);
		float z = std::sin(theta) * std::sin(phi);
		float y = std::cos(theta);

		// Add only the top hemisphere points (y >= 0)
		if (y >= 0) {
			rays.push_back(vec3(x, y, z));
		}
	}

	return rays;
}

RayTransmitter::RayTransmitter(Scene* s, Renderer* r) {
	this->scene = s;
	this->renderer = r;

	pos_w = vec3(0);
	rays = std::vector<vec3>(0);
	hits = std::vector<HIT>(0);

	GenerateAllGPU_Stuff();
}

void RayTransmitter::StartSimulation()
{
	// Get first time N directions relative to (0,0,0)
	if (rays.size() == 0) {
		getNrays(scene->num_of_rays);
	}

	// At this points we got N rays we want to shoot starting from our position.

	// for CPU version:
	hits.clear();
	for (auto& r : rays) {
		//if r collided with any face (triangle)
			// Add the HIT (hit point in world space and distance (hit_point - pos_w)
			// continue to next ray
		// else
			// misses++;

		HIT possible_hit = { 0 };
		for (auto model : scene->models)
		{
			MeshModel* p = (MeshModel*)model;
			p->updateTransform();
			p->updateTransformWorld();
			if (p->CollisionCheck(this->pos_w, r, &possible_hit))
			{
				this->hits.push_back(possible_hit);
				break;
			}
		}
	}
	UpdateRaysDataInGPU();



	//for GPU version:
		// load the model data as a triangle list
		// load the rays (directions)
		// load the uniform position of the Transmitter
		// Launch the Compute shader for the collision detection
		// Read the output of Compute Shader from shared memory

}

void RayTransmitter::UpdateRaysDataInGPU()
{
	if (VAO == 0 | VBO == 0)
		return;

	rays_data_gpu.clear();
	for (auto& h : hits) {
		rays_data_gpu.push_back(h.origin_w);
		rays_data_gpu.push_back(h.hit_point_w);
	}


	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	int lenInBytes = rays_data_gpu.size() * 3 * sizeof(float);
	glBufferData(GL_ARRAY_BUFFER, lenInBytes, rays_data_gpu.data(), GL_STATIC_DRAW);

	glBindVertexArray(0);
}

void RayTransmitter::UpdateModelViewInGPU(mat4& Tc, mat4& Tc_for_normals)
{
	// Calculate model view matrix:
	mat4 model = mat4(1) * mat4(1);
	mat4 view = Tc;

	// Calculate model view matrix:
	mat4 model_normals = mat4(1) * mat4(1);
	mat4 view_normals = Tc_for_normals;

	/* Bind the model matrix*/
	glUniformMatrix4fv(glGetUniformLocation(renderer->program, "model"), 1, GL_TRUE, &(model[0][0]));

	/* Bind the view matrix*/
	glUniformMatrix4fv(glGetUniformLocation(renderer->program, "view"), 1, GL_TRUE, &(view[0][0]));

	/* Bind the model_normals matrix*/
	glUniformMatrix4fv(glGetUniformLocation(renderer->program, "model_normals"), 1, GL_TRUE, &(model_normals[0][0]));

	/* Bind the view_normals matrix*/
	glUniformMatrix4fv(glGetUniformLocation(renderer->program, "view_normals"), 1, GL_TRUE, &(view_normals[0][0]));
}

void RayTransmitter::GenerateAllGPU_Stuff()
{
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	//Genereate VAO and VBOs in GPU:
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	GLint raysPos = glGetAttribLocation(renderer->program, "raysPos");
	glVertexAttribPointer(raysPos, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(raysPos);

	glBindVertexArray(0);
}