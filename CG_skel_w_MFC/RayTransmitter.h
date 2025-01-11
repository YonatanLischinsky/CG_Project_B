#pragma once
#include <ctime>
#include <chrono>
#include "Scene.h"

typedef enum _VBOs_ {
	GPU_TRIANGLE_DATA = 0,
	GPU_RAY_DIR,
	GPU_ROUTE_CHECK_POINTS,
	GPU_FEEDBACK_BUFFER,
};

class RayTransmitter
{
private:
	Scene* scene;
	Renderer* renderer;
	vec3 pos_w;
	std::vector<std::vector<HIT>> hits;
	std::vector<std::vector<HIT>> misses;
	std::vector<vec3> rays;
	std::vector<vec3> rays_data_gpu;
	std::vector<vec3> scene_triangles_wrld_pos;
	void getNrays(uint n);
	GLuint hitTexture;
	GLuint vbo;
	GLuint triangleTexture;
	chrono::high_resolution_clock::time_point start_time;
	chrono::high_resolution_clock::time_point end_time;
	void UpdateRaysVisualizationInGPU();
	void GenerateAllGPU_Stuff();
	void LoadSceneTriangles();
	void LoadRayDirections();
	void LoadRoutePoints();
	void UpdateSimulationResults(uint method);
public:
	GLuint VAO[2] = { 0 };
	GLuint VBO[4] = { 0 };
	SIM_RESULT sim_result;
	std::vector<vec3> route;

	RayTransmitter(Scene* s, Renderer* r);
	void UpdateModelViewInGPU(mat4& Tc, mat4& Tc_for_normals);
	void N_Rays_Updated() { rays.clear(); }
	void SetPosition_w(vec3 p) { pos_w = p; }
	int GetHitBufferLen() {
		uint c = 0;
		for (auto& i : hits) c += i.size();
		return c;
	}
	int GetMissBufferLen() {
		uint c = 0;
		for (auto& i : misses) c += i.size();
		return c;
	}
	void StartSimulation(uint method);

};

