#pragma once

#include "gl/glew.h"
#include <vector>
#include <string>
#include "Renderer.h"
#include "Grid.h"
#include "Light.h"
#include "Utils.h"

using namespace std;
void colorPicker(ImVec4* color, std::string button_label, std::string id);

class Model
{
protected:
	virtual ~Model() {}
	string name = MODEL_DEFAULT_NAME;
	bool userInitFinished = false;



public:
	void virtual draw(mat4& cTransform, mat4& projection, bool allowClipping, mat4& cameraRot) = 0;
	void setName(std::string newName) { name = newName; }
	void SetUserInitFinished() { userInitFinished = true; }
	bool GetUserInitFinished() { return userInitFinished; }
	std::string& getName() { return name; }
	
	bool selected = false;
	GLuint VAOs[VAO_COUNT] = { 0 };
	GLuint VBOs[VAO_COUNT][VBO_COUNT] = { 0 };

};

class Camera
{
private:

	string name = "";
	vec4 target;

	mat4 transform_mid_worldspace, transform_mid_viewspace, rotation_mat;
	

	unsigned int num_icon_vertices;

	bool lockFov_GUI = false;

	friend class Scene;	// To acces transformations;

public:
	float c_left, c_right, c_top, c_bottom, c_fovy , c_aspect , c_zNear, c_zFar;
	vec4 c_trnsl, c_rot, c_trnsl_viewspace, c_rot_viewspace;

	mat4 cTransform;
	mat4 view_matrix;	// cTransform inversed
	mat4 projection;
	mat4 rotationMat_normals;

	// Icon stuff
	vec3* icon;
	vec2* iconBuffer;
	GLuint VAO = 0;
	GLuint VBO = 0;
	Camera::Camera();
	
	mat4 LookAt(const vec4& eye, const vec4& at, const vec4& up);
	void Camera::LookAt(const Model* target = nullptr);
	mat4 GetPerspectiveoMatrix();

	void setOrtho();
	void setPerspective();
	void setPerspectiveByFov();
	void setPerspectiveByParams();
	void UpdateProjectionMatInGPU();
	void resetProjection();
	void zoom(double s_offset, double update_rate = 0.02);
	
	void setName(std::string newName) { name = newName; }
	std::string& getName() { return name; }
	vec4 getTranslation() { return vec4(c_trnsl); }
	vec3 getPosition() { return vec3(c_trnsl.x, c_trnsl.y, c_trnsl.z); }
	void setStartPosition(vec4& pos) { c_trnsl = pos; }
	
	void updateTransform();
	void ResetTranslation() { c_trnsl = vec4(0,1.5f,3.0f,1); }
	void ResetRotation() { c_rot = vec4(0,0,0,1); }
	
	void iconInit();
	bool iconDraw(mat4& active_cTransform, mat4& active_projection);
	vec2* getIconBuffer() { return iconBuffer; }
	unsigned int getIconBufferSize() { return num_icon_vertices; }


	void ResetTranslation_viewspace() { c_trnsl_viewspace = vec4(0, 0, 0, 1); }
	void ResetRotation_viewspace() { c_rot_viewspace = vec4(0,0,0,1); }
	
	void unLockFovy() { lockFov_GUI = false; }
	bool* getLockFovyPTR() { return &lockFov_GUI; }
	bool selected = false;
	bool isOrtho = true;
	bool renderCamera = false;
	bool allowClipping = false;
};

class Scene {

private:
	

	void AddCamera();
	void AddLight();
	void UpdateModelSelection();
	void UpdateGeneralUniformInGPU();
	void ResetPopUpFlags();
	void drawCameraTab();
	void drawModelTab();
	void drawLightTab();
	void drawSimResultsTab();
	void drawSimRouteTab();
	void drawSimSettingsTab();

	bool GUI_popup_pressedOK = false, GUI_popup_pressedCANCEL = false;
	bool showGrid = false;


	char* drawAlgoToString(DrawAlgo x)
	{
		switch (x)
		{
		case WIRE_FRAME:
			return "WIRE FRAME";
		case FLAT:
			return "FLAT";
		case GOURAUD:
			return "GOURAUD";
		case PHONG:
			return "PHONG";
		default:
			return ("error");
		}
	}
	vector<STB_Image> skyBoxImages = vector<STB_Image>(6);
	const float skyboxVertices[108] = {
		// positions          
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
	};
	GLuint skyboxVAO = 0;
	GLuint skyboxVBO = 0;
public:
	vector<Model*> models;
	vector<Camera*> cameras;
	Renderer* m_renderer;
	Scene(Renderer* renderer) : m_renderer(renderer)
	{
		num_of_rays = 1000;
		AddCamera();							 //Add the first default camera
		AddLight ();							 //Add the first default ambient light
		activeCamera = 0;						 //index = 0 because it is the first
		activeLight  = 0;						 //index = 0 because it is the first
		cameras[activeCamera]->selected = true;  //Select it because it is the default
		lights[activeLight]->selected   = true;  //Select it because it is the default
	};
	~Scene()
	{
		if (cubeMapId > 0)
		{
			glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapId);
			glDeleteTextures(1, &cubeMapId);
		}
	}
	void loadOBJModel(string fileName);
	void draw();
	void drawGUI();
	void resize_callback_handle(int width, int height);
	void setViewPort(vec4& vp);
	void zoom(double s_offset) { cameras[activeCamera]->zoom(s_offset); }
	
	Camera* GetActiveCamera();
	Model* GetActiveModel();
	Light* GetActiveLight();
	vector<Light*> lights;

	int activeModel  = NOT_SELECTED;
	int activeLight  = NOT_SELECTED;
	int activeCamera = 0;	// Always at least one camera
	DrawAlgo draw_algo = WIRE_FRAME;
	bool applyBloom = false;
	bool applyFullScreenBlur = false;
	bool applyEnviornmentShading = false;
	GLuint cubeMapId = 0;
	int viewportX;
	int viewportY;
	int viewportWidth;
	int viewportHeight;

	int num_of_rays;
	int cpu_mode = 0;
	bool display_rays_hits = true;
	bool display_rays_misses = false;
	bool display_hit_points = false;
	float pointSize = 10.0f;

	vec3 hitColor = vec3(0, 1, 0);	// green
	vec3 misColor = vec3(1, 0, 0);	// red
	vec3 hitPColor = vec3(1, 0, 1); // purple

};
