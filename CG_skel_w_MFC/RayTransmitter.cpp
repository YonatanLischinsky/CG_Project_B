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
	hits = std::vector<std::vector<HIT>>(0);
	misses = std::vector<std::vector<HIT>>(0);
	sim_result = { 0 };
	route = { vec3(0,0,0) };

	GenerateAllGPU_Stuff();
}

void RayTransmitter::StartSimulation(uint method)
{
	// Get first time N directions relative to (0,0,0)
	if (rays.size() == 0) {
		getNrays(scene->num_of_rays);
	}

	// At this points we got N rays we want to shoot starting from our position.
	hits.clear();
	misses.clear();


	// CPU version:
	if (method == 0) 
	{	
		this->start_time = chrono::high_resolution_clock::now();
		sim_result.hits = 0;
		for (uint i = 0; i < route.size(); i++) {
			this->pos_w = route[i];
			hits.push_back(vector<HIT>(0));
			misses.push_back(vector<HIT>(0));
			for (auto& r : rays) {
				HIT possible_hit = { 0 };
				bool intersected_already = false;
				for (auto model : scene->models)
				{
					MeshModel* p = (MeshModel*)model;
					p->updateTransform();
					p->updateTransformWorld();
					if (p->CollisionCheck(this->pos_w, r, &possible_hit))
					{
						if (intersected_already) {
							if (hits[i][hits[i].size() - 1].distance > possible_hit.distance) {
								hits[i][hits[i].size() - 1] = possible_hit;
							}
						}
						else {
							hits[i].push_back(possible_hit);
							intersected_already = true;
						}
					}
				}
				if (!intersected_already) {
					HIT miss = { 0 };
					miss.origin_w = this->pos_w;
					miss.hit_point_w = miss.origin_w + 10000000 * r;
					misses[i].push_back(miss);
				}
			}
			sim_result.hits += hits[i].size();
		}
		this->end_time = chrono::high_resolution_clock::now();

		UpdateRaysDataInGPU(); /* visualization */
	}
	//GPU version:
	else
	{
		// 1. load the model data as a triangle list
		// 2. load the rays (directions)
		// 3. load the uniform position of the Transmitter
		// Launch the Compute shader for the collision detection
		// Read the output of Compute Shader from shared memory
	}

	/* Update Simulation results object */
	sim_result.route_pts = route.size();
	sim_result.rays = rays.size() * route.size();
	sim_result.polygons = 0;
	for (auto& model : scene->models) sim_result.polygons += ((MeshModel*)model)->GetNumberOfPolygons();
	auto sim_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
	sim_result.time_seconds = std::chrono::duration<float>(sim_time).count();
	sim_result.time_milli	= std::chrono::duration<float>(sim_time).count() * 1000.0f;
	sim_result.time_micro	= std::chrono::duration<float>(sim_time).count() * 1000000.0f;
	sim_result.method		= method;
}

void RayTransmitter::UpdateRaysDataInGPU()
{
	if (VAO == 0 | VBO == 0)
		return;

	rays_data_gpu.clear();
	for (auto& hits_vec : hits) {
		for (auto& h : hits_vec) {
			rays_data_gpu.push_back(h.origin_w);
			rays_data_gpu.push_back(h.hit_point_w);
		}
	}

	for (auto& miss_vec : misses) {
		for (auto& h : miss_vec) {
			rays_data_gpu.push_back(h.origin_w);
			rays_data_gpu.push_back(h.hit_point_w);
		}
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