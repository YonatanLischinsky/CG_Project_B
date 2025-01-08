#pragma once
#include <ctime>
#include <chrono>
#include "Scene.h"


class RayTransmitter
{
private:
	Scene* scene;
	Renderer* renderer;
	vec3 pos_w;
	std::vector<HIT> hits;
	std::vector<vec3> rays;
	std::vector<vec3> rays_data_gpu;
	std::vector<vec3> getNrays(uint n);
	chrono::high_resolution_clock::time_point start_time;
	chrono::high_resolution_clock::time_point end_time;
	void UpdateRaysDataInGPU();
	void GenerateAllGPU_Stuff();
public:
	GLuint VAO = 0;
	GLuint VBO = 0;
	SIM_RESULT sim_result;

	RayTransmitter(Scene* s, Renderer* r);
	void UpdateModelViewInGPU(mat4& Tc, mat4& Tc_for_normals);
	void N_Rays_Updated() { rays.clear(); }
	void SetPosition_w(vec3 p) { pos_w = p; }
	int GetBufferLen() { return rays_data_gpu.size(); }
	void StartSimulation(uint method);

};

