#pragma once
#include "Scene.h"

class RayTransmitter
{
private:
	Scene* scene;
	Renderer* renderer;
	vec3 pos_w;
	uint misses;
	std::vector<HIT> hits;
	std::vector<vec3> rays;
	std::vector<vec3> getNrays(uint n);

public:
	RayTransmitter(Scene* s, Renderer* r) : scene(s), renderer(r) {
		pos_w = vec3(0);
		misses = 0;
		rays = std::vector<vec3>(0);
		hits = std::vector<HIT>(0);
	}

	void N_Rays_Updated() { rays.clear(); }
	void SetPosition_w(vec3 p) { pos_w = p; }
	std::vector<HIT>& GetHits() { return hits; }
	uint GetMisses() { return misses; }

	void StartSimulation();

};

