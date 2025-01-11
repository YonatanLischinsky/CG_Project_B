#include "RayTransmitter.h"
#include "MeshModel.h"
#include "Scene.h"
#include "Renderer.h"

void RayTransmitter::getNrays(uint n)
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
}

RayTransmitter::RayTransmitter(Scene* s, Renderer* r) {
	this->scene = s;
	this->renderer = r;

	pos_w = vec3(0);
	rays = std::vector<vec3>(0);
	hits = std::vector<std::vector<HIT>>(0);
	misses = std::vector<std::vector<HIT>>(0);
	scene_triangles_wrld_pos = std::vector<vec3>(0);
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
					miss.hit_point_w = miss.origin_w + 1e20 * r;
					misses[i].push_back(miss);
				}
			}
			sim_result.hits += hits[i].size();
		}
		this->end_time = chrono::high_resolution_clock::now();
	}
	//GPU version:
	else
	{
		glBindBuffer(GL_ARRAY_BUFFER, VBO[GPU_FEEDBACK_BUFFER]);
		glBufferData(GL_ARRAY_BUFFER, route.size() * scene->num_of_rays * sizeof(HIT), nullptr, GL_DYNAMIC_READ);

		// 1. Load triangles, rays, and route points
		glBindVertexArray(VAO[1]);
		LoadSceneTriangles();		// Triangles to the GPU
		LoadRayDirections();		// Rays to the GPU
		LoadRoutePoints();			// Route points to the GPU

		// 2. Set up simulation
		glUniform1i(glGetUniformLocation(renderer->program, "simulation"), 1);

		// Bind the feedback buffer
		glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, VBO[GPU_FEEDBACK_BUFFER]);

		// 3. Run the simulation
		this->start_time = chrono::high_resolution_clock::now();
		glBeginTransformFeedback(GL_POINTS);
		glDrawArrays(GL_POINTS, 0, scene->num_of_rays * route.size());
		glEndTransformFeedback();

		// 4. Disable simulation mode
		glUniform1i(glGetUniformLocation(renderer->program, "simulation"), 0);

		// 5. Wait for GPU to finish
		glFinish(); // Ensure all GPU tasks are complete

		this->end_time = chrono::high_resolution_clock::now();

		// 6. Read back results
		std::vector<HIT> hitResults(scene->num_of_rays * route.size());
		glBindBuffer(GL_ARRAY_BUFFER, VBO[GPU_FEEDBACK_BUFFER]);
		glGetBufferSubData(GL_ARRAY_BUFFER, 0, route.size() * scene->num_of_rays * sizeof(HIT), hitResults.data());

		hits.push_back(vector<HIT>(0));
		misses.push_back(vector<HIT>(0));

		// Process the results
		for (const auto& result : hitResults) {
			if (result.distance < 0) {
				misses[0].push_back(result);
			}
			else {
				hits[0].push_back(result);
			}
		}

		sim_result.hits = hits[0].size();



		glBindVertexArray(0);

	}

	
	/* Update simulation result */
	UpdateSimulationResults(method);

	/* visualization the rays */
	UpdateRaysVisualizationInGPU();
}

void RayTransmitter::UpdateSimulationResults(uint method) {
	/* Update Simulation results object */
	sim_result.route_pts = route.size();
	sim_result.rays = rays.size() * route.size();
	sim_result.polygons = 0;
	for (auto& model : scene->models) sim_result.polygons += ((MeshModel*)model)->GetNumberOfPolygons();
	auto sim_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
	sim_result.time_seconds = std::chrono::duration<float>(sim_time).count();
	sim_result.time_milli = std::chrono::duration<float>(sim_time).count() * 1000.0f;
	sim_result.time_micro = std::chrono::duration<float>(sim_time).count() * 1000000.0f;
	sim_result.method = method;
}

void RayTransmitter::LoadSceneTriangles() {
	scene_triangles_wrld_pos.clear();

	for (auto& model : scene->models)
	{
		MeshModel* p = (MeshModel*)model;
		std::vector<vec3> triangles = p->GetTrianglesWorldPos();
		for (auto& t : triangles)
			scene_triangles_wrld_pos.push_back(t);
	}

	//Populate the GPU memory
	glBindBuffer(GL_TEXTURE_BUFFER, VBO[GPU_TRIANGLE_DATA]);
	int lenInBytes = scene_triangles_wrld_pos.size() * sizeof(scene_triangles_wrld_pos[0]);
	glBufferData(GL_TEXTURE_BUFFER, lenInBytes, scene_triangles_wrld_pos.data(), GL_STATIC_DRAW);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_BUFFER, triangleTexture);
	glUniform1i(glGetUniformLocation(renderer->program, "triangleBuffer"), 0); // Bind triangle buffer
	glUniform1i(glGetUniformLocation(renderer->program, "numTriangles"), scene_triangles_wrld_pos.size() / 3);
}


void RayTransmitter::LoadRoutePoints() {
	//Populate the GPU memory
	glBindBuffer(GL_TEXTURE_BUFFER, VBO[GPU_ROUTE_CHECK_POINTS]);
	int lenInBytes = route.size() * sizeof(route[0]);
	glBufferData(GL_TEXTURE_BUFFER, lenInBytes, route.data(), GL_STATIC_DRAW);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_BUFFER, routeTexture);
	glUniform1i(glGetUniformLocation(renderer->program, "routeBuffer"), 1);
	glUniform1i(glGetUniformLocation(renderer->program, "numRoutePoints"), route.size());
}

void RayTransmitter::LoadRayDirections() {
	//Populate the GPU memory
	glBindBuffer(GL_TEXTURE_BUFFER, VBO[GPU_RAY_DIR]);
	int lenInBytes = rays.size() * sizeof(rays[0]);
	glBufferData(GL_TEXTURE_BUFFER, lenInBytes, rays.data(), GL_STATIC_DRAW);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_BUFFER, raysTexture);
	glUniform1i(glGetUniformLocation(renderer->program, "rayDirections"), 2);
	glUniform1i(glGetUniformLocation(renderer->program, "numRays"), rays.size());
}

void RayTransmitter::UpdateRaysVisualizationInGPU()
{
	if (VAO[0] == 0)
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


	glBindVertexArray(VAO[0]);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

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
	glGenVertexArrays(2, VAO);
	
	/* Visualizations */
	glBindVertexArray(VAO[0]);

	glGenBuffers(1, &vbo);
	
	//Bind rayPos for the CPU visualization:
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	GLint raysPos = glGetAttribLocation(renderer->program, "raysPos");
	glVertexAttribPointer(raysPos, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(raysPos);


	/* GPU Simulation */
	glBindVertexArray(VAO[1]);
	glGenBuffers(4, VBO);

	/* Generate Textures */
	glBindBuffer(GL_TEXTURE_BUFFER, VBO[GPU_TRIANGLE_DATA]);
	glGenTextures(1, &triangleTexture);
	glBindTexture(GL_TEXTURE_BUFFER, triangleTexture);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, VBO[GPU_TRIANGLE_DATA]);

	glBindBuffer(GL_TEXTURE_BUFFER, VBO[GPU_ROUTE_CHECK_POINTS]);
	glGenTextures(1, &routeTexture);
	glBindTexture(GL_TEXTURE_BUFFER, routeTexture);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, VBO[GPU_ROUTE_CHECK_POINTS]);

	glBindBuffer(GL_TEXTURE_BUFFER, VBO[GPU_RAY_DIR]);
	glGenTextures(1, &raysTexture);
	glBindTexture(GL_TEXTURE_BUFFER, raysTexture);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, VBO[GPU_RAY_DIR]);
	

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_BUFFER, 0);
}