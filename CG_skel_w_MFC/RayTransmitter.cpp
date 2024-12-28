#include "RayTransmitter.h"
#include "MeshModel.h"

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
		float y = std::sin(theta) * std::sin(phi);
		float z = std::cos(theta);

		// Add only the top hemisphere points (z >= 0)
		if (z >= 0.0f) {
			rays.push_back(vec3(x, y, z));
		}
	}

	return rays;
}

void RayTransmitter::StartSimulation()
{
	// Get first time N directions relative to (0,0,0)
	if (rays.size() == 0) {
		getNrays(scene->num_of_rays);
	}

	// At this points we got N rays we want to shoot starting from our position.

	// for CPU version:
	for (auto& r : rays) {
		//if r collided with any face (triangle)
			// Add the HIT (hit point in world space and distance (hit_point - pos_w)
			// continue to next ray
		// else
			// misses++;

		//3. draw each Model
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
		if (possible_hit.distance == 0) // no hit found at all
			misses++;
	}

	//for GPU version:
		// load the model data as a triangle list
		// load the rays (directions)
		// load the uniform position of the Transmitter
		// Launch the Compute shader for the collision detection
		// Read the output of Compute Shader from shared memory

}
