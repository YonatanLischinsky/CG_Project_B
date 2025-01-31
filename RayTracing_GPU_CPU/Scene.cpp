#include "stdafx.h"
#include <string>
#include <fstream>
#include <math.h>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem> // For directory creation (C++17)
#include "Scene.h"
#include "MeshModel.h"
#include "PrimMeshModel.h"
#include "RayTracing_GPU_CPU.h"
#include "RayTransmitter.h"
#include <iostream>
#include <thread>
#include <chrono>



#define CLAMP(x, l, r) (min( max((x), (l)) , (r)))

#define MODEL_TAB_INDEX		 0
#define CAMERA_TAB_INDEX	 1
#define LIGHT_TAB_INDEX		 2
#define SIM_ROUTE_TAB_INDEX	 3
#define SIM_SETT_TAB_INDEX	 4
#define SIM_RES_TAB_INDEX	 5

using namespace std;
extern Renderer* renderer;
extern RayTransmitter* rt;

static char nameBuffer[64] = { 0 };
static float posBuffer[3] = { 0 };
static int g_ortho = 0;
static int n_rays = 1000;
static int light_type_radio_button;
static bool saved_palette_init = true;
static ImVec4 saved_palette[32] = {};
static int selectedTab = 0;

bool add_showModelDlg = false, add_showCamDlg = false, add_showLightDlg = false;
bool simulation_showNumRaysDlg = false;
bool simulation_showFileSavedDlg = false;
bool showTransWindow = false;
bool constScaleRatio = true;
bool constScaleRatio_w = true;
int transformationWindowWidth = 0;
bool closedTransfWindowFlag = false;


const float FOV_RANGE_MIN = -179, FOV_RANGE_MAX = 179;
const float ASPECT_RANGE_MIN = -10, ASPECT_RANGE_MAX = 10;
const float PROJ_RANGE_MIN = -20, PROJ_RANGE_MAX = 20;
const float ZCAM_RANGE_MIN = 0.001, ZCAM_RANGE_MAX = 100;
const float TRNSL_RANGE_MIN = -40, TRNSL_RANGE_MAX = 40;
const float ROT_RANGE_MIN = -180, ROT_RANGE_MAX = 180;
const float SCALE_RANGE_MIN = -1, SCALE_RANGE_MAX = 20;


//--------------------------------------------------
//-------------------- CAMERA ----------------------
//--------------------------------------------------

Camera::Camera()
{
	name = CAMERA_DEFAULT_NAME;
	
	iconInit();
	ResetRotation();
	ResetTranslation();
	resetProjection();
	//setOrtho();
	setPerspective();
	LookAt();
	name = CAMERA_DEFAULT_NAME;
}

void Camera::iconInit()
{
	num_icon_vertices = 6;	// Or 4 - optional
	icon = new vec3[num_icon_vertices];
	iconBuffer = new vec2[num_icon_vertices];
	icon[0] = vec3(1, 0, 0);
	icon[1] = vec3(-1, 0, 0);
	icon[2] = vec3(0, 1, 0);
	icon[3] = vec3(0, -1, 0);
	icon[4] = vec3(0, 0, 0);	// Optional
	icon[5] = vec3(0, 0, -2);	// optional
}

bool Camera::iconDraw( mat4& active_cTransform, mat4& active_projection)
{
	//for (int i = 0; i < num_icon_vertices; i++)
	//{
	//	
	//	vec4 v_i(icon[i]);

	//	//Apply transformations:
	//	v_i = active_cTransform * (transform_mid_worldspace * v_i);

	//	//Project:
	//	v_i = active_projection * v_i;
	//	
	//	v_i /= v_i.w;

	//	//Clip it:
	//	if (v_i.x < -1 || v_i.x > 1 || v_i.y < -1 || v_i.y > 1 || v_i.z < -1 || v_i.z > 1)
	//	{
	//		return false;
	//	}

	//	//Add to buffer:
	//	iconBuffer[i] = vec2(v_i.x, v_i.y);
	//}
	
	return true;
}

mat4 Camera::LookAt(const vec4& eye, const vec4& at, const vec4& up)
{
	vec4 n, u, v;
	n = normalize(eye - at);
	
	//Edge case: Camera is directly above / below the target (up is parallel to n)
	if (vec3(n.x, n.y, n.z) == vec3(up.x, up.y, up.z) || vec3(n.x, n.y, n.z) == -vec3(up.x, up.y, up.z))
	{
		/* Add little noise so it won't divide by 0 ;) */
		n.x += 0.1f;
	}
	
	u = normalize(cross(up, n));
	v = normalize(cross(n, u));
	
	vec4 t = vec4(0.0, 0.0, 0.0, 1.0);
	n.w = u.w = v.w = 0;
	mat4 c = mat4(u, v, n, t);

	mat4 result = c * Translate(-eye);

	return result;

}

//This function is called from keyboard manager (pressed F to set focus)
void Camera::LookAt(const Model* target, const vec4 targetPosition)
{
	/* Camera position */
	vec4 eye = c_trnsl;
 
	/* Target position */
	if (target)
		this->target = ((MeshModel*)target)->getCenterOffMass();
	else
		this->target = targetPosition;

	//std::cout << "look at target: " << targetPosition << "\n";

	/* Up vector */
	vec4 up = vec4(0, 1, 0, 1);

	/* Get LookAt matrix */
	mat4 lookAtMat = LookAt(eye, this->target, up);

	/* Get Euler angles by rotation matrix */
	const float m11 = lookAtMat[0][0], m12 = lookAtMat[0][1], m13 = lookAtMat[0][2];
	const float m21 = lookAtMat[1][0], m22 = lookAtMat[1][1], m23 = lookAtMat[1][2];
	const float m31 = lookAtMat[2][0], m32 = lookAtMat[2][1], m33 = lookAtMat[2][2];
	float x, y, z;
	y = asinf(CLAMP(m13, -1, 1));
	if (abs(m13) < 0.9999999)
	{
		x = atan2f(-m23, m33);
		z = atan2f(-m12, m11);
	}
	else
	{
		x = atan2f(m32, m22);
		z = 0;
	}

	/* Radians to Degrees */
	x *= 180 / M_PI;
	y *= 180 / M_PI;
	z *= 180 / M_PI;
	

	/* Update camera's vectors*/
	c_rot = vec4(-x, y, z, 1);
	c_rot_viewspace = vec4(0, 0, 0, 1);
	c_trnsl_viewspace = vec4(0, 0, 0, 1);
	updateTransform();
}

void Camera::setOrtho()
{
	GLfloat x = c_right - c_left;
	GLfloat y = c_top - c_bottom;
	GLfloat z = c_zFar - c_zNear;

	projection = mat4(  2 / x, 0    , 0      , -(c_left + c_right) / x,
							0, 2 / y, 0      , -(c_top + c_bottom) / y,
							0, 0    , -2 / z , -(c_zFar + c_zNear) / z,
							0, 0    , 0      , 1);
}

mat4 Camera::GetPerspectiveoMatrix()
{
	float c_fovy_local = 30;
	float c_zNear_local = 1;
	float c_zFar_local = 100;

	float c_top_local = c_zNear_local * tan((M_PI / 180) * c_fovy_local);
	float c_right_local = c_top_local;
	float c_bottom_local = -c_top_local;
	float c_left_local   = -c_right_local;

	GLfloat x = c_right_local - c_left_local;
	GLfloat y = c_top_local - c_bottom_local;
	GLfloat z = c_zFar_local - c_zNear_local;

	return mat4(2 * c_zNear_local / x, 0, (c_right_local + c_left_local) / x, 0,
				0, 2 * c_zNear_local / y, (c_top_local + c_bottom_local) / y, 0,
				0, 0, -(c_zFar_local + c_zNear_local) / z, -(2 * c_zNear_local * c_zFar_local) / z,
				0, 0, -1, 0);
}

void Camera::setPerspective()
{
	GLfloat x = c_right - c_left;
	GLfloat y = c_top   - c_bottom;
	GLfloat z = c_zFar  - c_zNear;

	projection = mat4(2*c_zNear / x	, 0				, (c_right+c_left)   / x , 0					 ,
					  0				, 2*c_zNear / y	, (c_top + c_bottom) / y , 0				     ,
					  0				, 0				, -(c_zFar+c_zNear)  / z , -(2*c_zNear*c_zFar)/z ,
					  0				, 0				, -1				     , 0);

}

void Camera::setPerspectiveByFov()
{
	c_top = c_zNear * tan((M_PI / 180) * c_fovy);
	c_right = c_top * c_aspect;
	
	c_bottom = -c_top;
	c_left = -c_right;

	
	setPerspective();
}

void Camera::setPerspectiveByParams()
{	
	if (!lockFov_GUI)
	{
		float width  = (c_right - c_left);
		float height = (c_top - c_bottom);
		c_aspect = c_right / c_top;
		c_fovy = (180 / M_PI) * atanf(c_top / c_zNear);
		setPerspective();
	}
	else
	{
		// Fovy is locked + user changed zNear
		// Calcualte new left right top bottom
		setPerspectiveByFov();
	}
	

}

void Camera::UpdateProjectionMatInGPU()
{
	/* Bind the projection matrix*/
	glUniformMatrix4fv(glGetUniformLocation(renderer->program, "projection"), 1, GL_TRUE, &(projection[0][0]));

	/* Bind the projection_normals matrix*/
	glUniformMatrix4fv(glGetUniformLocation(renderer->program, "projection_normals"), 1, GL_TRUE, &(rotationMat_normals[0][0]));
}

void Camera::resetProjection()
{

	c_left = c_bottom = -DEF_PARAM;
	c_right = c_top = DEF_PARAM;
	c_zNear = DEF_ZNEAR;
	c_zFar = DEF_ZFAR;

	c_fovy = DEF_FOV;
	c_aspect = DEF_ASPECT;

	if (isOrtho)
		setOrtho();
	else
		setPerspectiveByFov();

	updateTransform();

}

void Camera::updateTransform()
{
	mat4 rot_x = RotateX(c_rot.x);
	mat4 rot_y = RotateY(c_rot.y);
	mat4 rot_z = RotateZ(c_rot.z);

	rotation_mat = transpose(rot_z * (rot_y * rot_x));
	mat4 trnsl = Translate(-c_trnsl);



	//Save mid results for camera icon view
	transform_mid_worldspace = Translate(c_trnsl) * transpose(rotation_mat);
	
	// C-t  = R^T * T^-1
	cTransform = rotation_mat * trnsl; // Mid result of cTransform


	//Apply view-space transformations:
	mat4 rot_x_view = RotateX(c_rot_viewspace.x);
	mat4 rot_y_view = RotateY(c_rot_viewspace.y);
	mat4 rot_z_view = RotateZ(c_rot_viewspace.z);

	mat4 rot_view = rot_z_view * (rot_y_view * rot_x_view);
	mat4 trnsl_view = Translate(c_trnsl_viewspace);
	
	cTransform = trnsl_view * (rot_view * cTransform);

	//Save mid results for camera icon view
	transform_mid_viewspace = trnsl_view * (rot_view * transform_mid_worldspace);

	//Save the inverse rotation matrix for normals display:
	rotationMat_normals = rot_view * rotation_mat;

}

void Camera::zoom(double s_offset, double update_rate)
{
	// Change projection according to the scroll offset of the user
	double offset_tot = s_offset * update_rate;
	c_top -= offset_tot;
	c_bottom += offset_tot;
	c_right -= offset_tot;
	c_left += offset_tot;
	lockFov_GUI = false; //Unlock the params


	if (isOrtho)
		setOrtho();
	else
		setPerspectiveByParams();
}

void Camera::HandleMovement()
{
	if (move_state & KEY_W)
		MoveForward();
	if (move_state & KEY_S)
		MoveBackward();
	if (move_state & KEY_A)
		MoveLeft();
	if (move_state & KEY_D)
		MoveRight();
	if (move_state & KEY_SPACE)
		MoveUp();
	if (move_state & KEY_SHIFT)
		MoveDown();
}

void Camera::MoveForward()
{
	vec3 frwd  = normalize(calculateForwardVector());
	c_trnsl += frwd * CAM_MOVE_SPEED;
	updateTransform();
}

void Camera::MoveBackward()
{
	vec3 frwd = normalize(calculateForwardVector());
	c_trnsl -= frwd * CAM_MOVE_SPEED;
	updateTransform();
}

void Camera::MoveLeft()
{
	vec3 right = normalize(calculateRightVector());
	c_trnsl -= right * CAM_MOVE_SPEED;
	updateTransform();
}

void Camera::MoveRight()
{
	vec3 right = normalize(calculateRightVector());
	c_trnsl += right * CAM_MOVE_SPEED;
	updateTransform();
}

void Camera::MoveUp()
{
	c_trnsl += vec3(0, 1, 0) * CAM_MOVE_SPEED;
	updateTransform();
}

void Camera::MoveDown()
{
	c_trnsl -= vec3(0, 1, 0) * CAM_MOVE_SPEED;
	updateTransform();
}

vec3 Camera::calculateForwardVector()
{
	float pitchRad = toRadians(c_rot.x);
	float yawRad = toRadians(c_rot.y);

	float fx = std::sin(yawRad) * std::cos(pitchRad); // X-axis component
	float fy = std::sin(pitchRad);                   // Y-axis component
	float fz = -std::cos(yawRad) * std::cos(pitchRad); // Z-axis component

	return vec3(fx, fy, fz);
}

vec3 Camera::calculateRightVector()
{
	float yawRad = toRadians(c_rot.y);

	float rx = std::cos(yawRad); // X-axis component
	float ry = 0.0;              // Y-axis component
	float rz = std::sin(yawRad); // Z-axis component

	return vec3(rx, ry, rz);
}








//--------------------------------------------------
//-------------------- SCENE ----------------------
//--------------------------------------------------
std::string floatToStringWithPrecision(float value, uint precision = 3) {
	value = std::round(value * 1000.0f) / 1000.0f; // Round to 3 decimal places
	return std::to_string(value).substr(0, std::to_string(value).find('.') + precision+1); // Keep only 3 decimals
}

std::string GetCurrentTimeString() {
	auto time_t_now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::tm local_time;
	localtime_s(&local_time, &time_t_now);
	std::ostringstream oss;
	oss << std::put_time(&local_time, "%d-%m-%Y-%H-%M-%S");
	return oss.str();
}

void Scene::AddCamera()
{
	Camera* cam = new Camera();
	cameras.push_back(cam);

	string s = cam->getName();
	s += " " + std::to_string(cameras.size());
	cam->setName(s); //Camera 1, Camera 2, Camera 3 ...
}

void Scene::AddLight()
{
	Light* lightSource = new Light();
	lights.push_back(lightSource);
	string s = lightSource->getName();
	s += " " + std::to_string(lights.size());
	lightSource->setName(s); //Light 1, Light 2, Light 3 ...
}

void Scene::loadOBJModel(string fileName)
{
	MeshModel* model = new MeshModel(fileName, m_renderer);

	/* Get the filename as the default name */
	string extractedName = MODEL_DEFAULT_NAME;
	unsigned int pos = fileName.find_last_of('\\');
	if (pos != std::string::npos)
	{
		extractedName = fileName.substr(pos + 1);
		pos = extractedName.find_last_of('.');
		if (pos != std::string::npos)
			extractedName = extractedName.substr(0, pos); //Cut the .obj in the end of the name
	}
	model->setName(extractedName);

	/* Add model to the models array */
	models.push_back(model);
}

void Scene::ResetPopUpFlags()
{
	GUI_popup_pressedOK = false;		// Reset flag
	GUI_popup_pressedCANCEL = false;	// Reset flag
	add_showCamDlg = false;				// Reset flag
	add_showModelDlg = false;			// Reset flag
	add_showLightDlg = false;			// Reset flag
	simulation_showNumRaysDlg	= false;// Reset flag
	simulation_showFileSavedDlg = false;// Reset flag
	memset(nameBuffer, 0, IM_ARRAYSIZE(nameBuffer));
	memset(posBuffer, 0, sizeof(float) * 3);
}

void Scene::draw()
{

	//0. Move Camera
	GetActiveCamera()->HandleMovement();

	//1. Clear the pixel buffer before drawing new frame.
	m_renderer->clearBuffer();

	//2. Update general uniforms in GPU:
	UpdateGeneralUniformInGPU();
	
	//3. draw each Model
	for (auto model : models)
	{
		//Don't draw new model before user clicked 'OK'.
		if (!model->GetUserInitFinished())
			continue;

		MeshModel* p = (MeshModel*)model;
		p->updateTransform();
		p->updateTransformWorld();

		//At This Point, the model matrix, model VAO, and all other model data is updated & ready in the GPU.
		m_renderer->drawModel(draw_algo, model, GetActiveCamera()->cTransform);
	}

	//4. Draw rays
	if (rt && rt->GetHitBufferLen() != 0) {
		m_renderer->drawRays(GetActiveCamera()->cTransform);
	}

	//5. Draw Route setting
	if (add_path_mode)
	{
		m_renderer->drawPathSetting();
	}

	//6. Draw Route points
	m_renderer->drawPathPoints();
	

}

void Scene::drawGUI()
{
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowPadding = ImVec2(0, 0);
	style.ItemSpacing = ImVec2(5, 4);
	style.FramePadding = ImVec2(4, 10);
	style.SeparatorTextBorderSize = 10;
	style.SeparatorTextAlign = ImVec2(0.5, 0.5);
	style.SeparatorTextPadding = ImVec2(20, 10);

	const ImGuiViewport* viewport = ImGui::GetMainViewport();

	//UnSelect the object if it's transformation windows closed
	if (!showTransWindow && activeModel != NOT_SELECTED)
	{
		models[activeModel]->selected = false;
		activeModel = NOT_SELECTED;
	}

	if (!showTransWindow)
	{
		if (!closedTransfWindowFlag)
		{
			//Call manually the resize callback to update the scene:
			resize_callback_handle(m_renderer->GetWindowSize().x, m_renderer->GetWindowSize().y);
			closedTransfWindowFlag = true;
			transformationWindowWidth = 0;
		}
	}

	//---------------------------------------------------------
	//---------------------- Main Menu ------------------------
	//---------------------------------------------------------
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::ArrowButton("Sidebar", ImGuiDir_Right))
		{
			showTransWindow = !showTransWindow;
		}
		if (ImGui::BeginMenu("Add"))
		{
			if (ImGui::MenuItem("Model (.obj file)"))	// Loading Model
			{
				CFileDialog dlg(TRUE, _T(".obj"), NULL, NULL, _T("(*.obj)|*.obj|All Files (*.*)|*.*||"));
				if (dlg.DoModal() == IDOK)
				{
					std::string filename((LPCTSTR)dlg.GetPathName());
					loadOBJModel(filename);

					strcpy(nameBuffer, models.back()->getName().c_str());
					add_showModelDlg = true;
					showTransWindow = true;
				}
			}
			//if (ImGui::MenuItem("Camera"))
			//{
			//	AddCamera();
			//	strcpy(nameBuffer, cameras.back()->getName().c_str());
			//	vec4 c_trnsl = cameras.back()->getTranslation();
			//	posBuffer[0] = c_trnsl.x;
			//	posBuffer[1] = c_trnsl.y;
			//	posBuffer[2] = c_trnsl.z;
			//
			//	add_showCamDlg = true;
			//	showTransWindow = true;
			//}
			if (ImGui::MenuItem("Light Source"))
			{
				AddLight();
				strcpy(nameBuffer, lights.back()->getName().c_str());
				vec3* c_trnsl = lights.back()->getPositionPtr();
				posBuffer[0] = c_trnsl->x;
				posBuffer[1] = c_trnsl->y;
				posBuffer[2] = c_trnsl->z;

				add_showLightDlg = true;
				showTransWindow = true;
			}

			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Simulation"))
		{
			if (models.size() > 0 && num_of_rays > 0 && rt && rt->route.size() > 0)
			{
				if (ImGui::MenuItem("Start Simulation"))
				{
					if (rt)
						rt->StartSimulation(cpu_mode);
					showTransWindow = true;
					selectedTab = SIM_RES_TAB_INDEX;
				}
			}
			if (ImGui::MenuItem("Number of rays"))
			{
				simulation_showNumRaysDlg = true;
				n_rays = num_of_rays;
			}
			if (ImGui::MenuItem("CPU mode", NULL, cpu_mode == 1)) {
				cpu_mode = 1;
			}
			if (ImGui::MenuItem("GPU mode", NULL, cpu_mode == 0)) {
				cpu_mode = 0;
			}
			ImGui::Checkbox("Show Hits", &display_rays_hits);
			ImGui::Checkbox("Show Misses", &display_rays_misses);
			ImGui::Checkbox("Show Hit Points", &display_hit_points);
			ImGui::EndMenu();
		}
		if (models.size() > 0)
		{
			if (ImGui::BeginMenu("Models"))
			{
				int len = models.size();
				for (int c = 0; c < len; c++)
				{
					if (ImGui::MenuItem(models[c]->getName().c_str(), NULL, &models[c]->selected))
					{
						activeModel = c;
						UpdateModelSelection();

						showTransWindow = true;
					}
				}
				ImGui::EndMenu();
			}
		}
		//if (ImGui::BeginMenu("Cameras"))
		//{
		//	int len = cameras.size();
		//	for (int c = 0; c < len; c++)
		//	{
		//		if (ImGui::MenuItem(cameras[c]->getName().c_str(), NULL, &cameras[c]->selected))
		//		{
		//			/* Deselect all others */
		//			for (int t = 0; t < len; t++)
		//				cameras[t]->selected = false;
		//
		//			/* Select current camera */
		//			activeCamera = c;
		//			cameras[c]->selected = true;
		//			showTransWindow = true;
		//		}
		//	}
		//	ImGui::EndMenu();
		//}
		if (ImGui::BeginMenu("Lights"))
		{
			int len = lights.size();
			for (int c = 0; c < len; c++)
			{
				if (ImGui::MenuItem(lights[c]->getName().c_str(), NULL, &lights[c]->selected))
				{
					/* Deselect all others */
					for (int t = 0; t < len; t++)
						lights[t]->selected = false;

					/* Select current light */
					activeLight = c;
					lights[c]->selected = true;
					showTransWindow = true;
				}
			}
			ImGui::EndMenu();
		}

		// Delete Model/Camera/Lights
		if (models.size() > 0 || cameras.size() > 1 || lights.size() > 1)
		{
			if (ImGui::BeginMenu("Delete..."))
			{
				if (models.size() > 0)
				{
					if (ImGui::BeginMenu("Models"))
					{
						for (int c = 0; c < models.size(); c++)
						{
							if (ImGui::MenuItem(models[c]->getName().c_str(), NULL))
							{
								models.erase(models.begin() + c);
								if (c == activeModel)
								{
									activeModel = NOT_SELECTED;	// Selected model deleted
								}
								else if (activeModel > c)
								{
									--activeModel;	// index moved
								}
							}
						}
						ImGui::EndMenu();
					}
				}
				if (cameras.size() > 1)	// Delete only if there is more than one camera
				{
					if (ImGui::BeginMenu("Cameras"))
					{
						for (int c = 0; c < cameras.size(); c++)
						{
							if (ImGui::MenuItem(cameras[c]->getName().c_str(), NULL))
							{
								/* Delete current camera */
								cameras.erase(cameras.begin() + c);

								if (c == activeCamera)
								{
									activeCamera = max(0, activeCamera - 1);
								}
								else if (activeCamera > c)
								{
									--activeCamera;	// index changed
								}
								cameras[activeCamera]->selected = true;

							}
						}
						ImGui::EndMenu();
					}

				}
				if (lights.size() > 1)	// Delete only if there is more than one light
				{
					if (ImGui::BeginMenu("Lights"))
					{
						for (int c = 0; c < lights.size(); c++)
						{
							if (ImGui::MenuItem(lights[c]->getName().c_str(), NULL))
							{
								/* Delete current light */
								lights.erase(lights.begin() + c);

								if (c == activeLight)
								{
									activeLight = max(0, activeLight - 1);
								}
								else if (activeLight > c)
								{
									--activeLight;	// index changed
								}
								lights[activeLight]->selected = true;

								m_renderer->UpdateLightsUBO(true);

							}
						}
						ImGui::EndMenu();
					}

				}

				ImGui::EndMenu(); //End delete
			}
		}

		if (ImGui::BeginMenu("Shading Algorithm"))
		{
			for (int i = 0; i < static_cast<int>(DrawAlgo::COUNT); ++i) {
				DrawAlgo algo = static_cast<DrawAlgo>(i);
				if (ImGui::MenuItem(drawAlgoToString(algo), NULL, draw_algo == algo))
					draw_algo = algo;
			}

			ImGui::EndMenu();	// End Shading algo menu
		}
		if (ImGui::Button("Dark/Light Mode##lightmode"))
		{
			m_renderer->invertSceneColors();
		}

		ImGui::EndMainMenuBar();
	}


	//---------------------------------------------------------
	//------------ Transformations Window ---------------------
	//---------------------------------------------------------
	if (activeCamera != NOT_SELECTED && !add_showModelDlg && !add_showCamDlg && !add_showLightDlg && !simulation_showNumRaysDlg && showTransWindow)
	{
		closedTransfWindowFlag = false;
		float mainMenuBarHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.0f;
		ImGui::SetNextWindowPos(ImVec2(0, mainMenuBarHeight), ImGuiCond_Always);
		ImGui::SetNextWindowSizeConstraints(ImVec2(450, m_renderer->GetWindowSize().y - mainMenuBarHeight), \
			ImVec2(m_renderer->GetWindowSize().x / 2, m_renderer->GetWindowSize().y - mainMenuBarHeight));
		if (ImGui::Begin("Control Window", &showTransWindow, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove))
		{
			int currentWidth = (int)ImGui::GetWindowSize().x;
			if (currentWidth != transformationWindowWidth)
			{
				transformationWindowWidth = currentWidth;
				resize_callback_handle(m_renderer->GetWindowSize().x, m_renderer->GetWindowSize().y);
			}

			const char* names[6] = { "Model", "Camera", "Light", "Path", "Config","Simulation Output"};
			ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;

			if (ImGui::BeginTabBar("ControlBar", tab_bar_flags))
			{
				ImGui::PushItemWidth(80);

				for (int n = 0; n < ARRAYSIZE(names); n++) {
					if (n == MODEL_TAB_INDEX && activeModel == NOT_SELECTED)
						continue;
					
					ImGuiTabItemFlags tabFlags = (selectedTab == n) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;

					// Render the tab
					if (ImGui::BeginTabItem(names[n], nullptr, tabFlags)) 
					{
						if (ImGui::IsItemActive())
							selectedTab = n;

						// Draw the appropriate content
						if (n == MODEL_TAB_INDEX)
							drawModelTab();
						else if (n == CAMERA_TAB_INDEX)
							drawCameraTab();
						else if (n == LIGHT_TAB_INDEX)
							drawLightTab();
						else if (n == SIM_ROUTE_TAB_INDEX)
							drawSimRouteTab();
						else if (n == SIM_SETT_TAB_INDEX)
							drawSimSettingsTab();
						else if (n == SIM_RES_TAB_INDEX)
							drawSimResultsTab();

						ImGui::EndTabItem();
					}
				}
				
				ImGui::PopItemWidth();
				ImGui::EndTabBar();
			}

			ImGui::End();
		}
	}


	//---------------------------------------------------------
	//-------- Check if the popup should be shown -------------
	//---------------------------------------------------------
	if (add_showModelDlg || add_showCamDlg || add_showLightDlg)
	{
		ImGui::OpenPopup(ADD_INPUT_POPUP_TITLE);
	}
	else if (simulation_showNumRaysDlg)
	{
		ImGui::OpenPopup("Number of rays");
	}
	else if (simulation_showFileSavedDlg)
	{
		ImGui::OpenPopup("File Saved");
	}

	//---------------------------------------------------
	//------- Begin pop up - MUST BE IN THIS SCOPE ------
	//---------------------------------------------------
	if (ImGui::BeginPopupModal("Number of rays", 0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
		ImGui::InputInt("Number of rays ", &num_of_rays);

		// Add buttons for OK and Cancel
		ImGui::Button("OK");
		if ((ImGui::IsItemClicked() || ImGui::IsKeyReleased(ImGui::GetKeyIndex(ImGuiKey_Enter))))
		{
			GUI_popup_pressedOK = true;
			GUI_popup_pressedCANCEL = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		ImGui::Button("Cancel");
		if (ImGui::IsItemClicked() || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
		{
			GUI_popup_pressedOK = false;
			GUI_popup_pressedCANCEL = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal(ADD_INPUT_POPUP_TITLE, 0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse))
	{
		ImGui::InputText("Name", nameBuffer, IM_ARRAYSIZE(nameBuffer));
		ImGui::InputFloat3("Position (x,y,z)", posBuffer);

		/* Validate name (not empty + not in use) */
		bool isNameValid = strlen(nameBuffer) > 0;
		if (add_showCamDlg)
		{
			for (auto i : cameras)
			{
				if (i != cameras.back())
				{
					isNameValid = isNameValid && strcmp(i->getName().c_str(), nameBuffer);
				}
			}
		}
		else if (add_showModelDlg)
		{
			for (auto i : models)
			{
				if (i != models.back())
				{
					isNameValid = isNameValid && strcmp(i->getName().c_str(), nameBuffer);
				}
			}
		}
		else if (add_showLightDlg)
		{
			auto currLight = lights.back();

			light_type_radio_button = (int)currLight->getLightType();

			ImGui::RadioButton("Ambient", &light_type_radio_button, AMBIENT_LIGHT); ImGui::SameLine();
			ImGui::RadioButton("Point", &light_type_radio_button, POINT_LIGHT); ImGui::SameLine();
			ImGui::RadioButton("Parallel", &light_type_radio_button, PARALLEL_LIGHT); ImGui::SameLine();

			currLight->setLightType(light_type_radio_button);


			vec3& color = currLight->getColor();

			ImVec4 color_local = ImVec4(color.x, color.y, color.z, 1);

			colorPicker(&color_local, "Color", "##LightColor");

			color.x = color_local.x;
			color.y = color_local.y;
			color.z = color_local.z;

		}

		/* Validate position input */
		bool arePositionValuesValid = true;
		for (int i = 0; i < 3; ++i)
		{
			arePositionValuesValid = arePositionValuesValid && !std::isnan(posBuffer[i]) && !std::isinf(posBuffer[i]);
		}


		// Notify the user if position values are not in the specified range
		if (!arePositionValuesValid)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Position values must be float");
		}
		else if (!isNameValid)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Name is not valid (or in-use)");
		}

		// Add buttons for OK and Cancel
		ImGui::Button("OK");
		if ((ImGui::IsItemClicked() || ImGui::IsKeyReleased(ImGui::GetKeyIndex(ImGuiKey_Enter)))
			&& arePositionValuesValid && isNameValid)
		{
			GUI_popup_pressedOK = true;
			GUI_popup_pressedCANCEL = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		ImGui::Button("Cancel");
		if (ImGui::IsItemClicked() || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
		{
			GUI_popup_pressedOK = false;
			GUI_popup_pressedCANCEL = true;
			ImGui::CloseCurrentPopup();
			showTransWindow = false;
		}


		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("File Saved", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("The simulation has been saved successfully.");
		
		ImGui::Button("OK");
		if ((ImGui::IsItemClicked() || ImGui::IsKeyReleased(ImGui::GetKeyIndex(ImGuiKey_Enter))))
		{
			GUI_popup_pressedOK = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	//------------------------------------
	//--------  Handle pop-ups -----------
	//------------------------------------
	if (add_showModelDlg)
	{
		if (GUI_popup_pressedOK)
		{
			auto currModel = models.back();	// Surely loaded new model

			currModel->setName(nameBuffer);
			((MeshModel*)currModel)->setTranslationWorld(vec3(posBuffer[0], posBuffer[1], posBuffer[2]));

			activeModel = models.size() - 1;
			UpdateModelSelection(); //Deselect all others & select activeModel.

			currModel->SetUserInitFinished();
			ResetPopUpFlags();


		}
		else if (GUI_popup_pressedCANCEL)
		{
			activeModel = NOT_SELECTED;
			delete ((MeshModel*)models[models.size() - 1]);
			models.pop_back();

			ResetPopUpFlags();
		}
	}
	else if (add_showCamDlg)
	{
		if (GUI_popup_pressedOK)
		{
			auto currCamera = cameras.back();
			currCamera->setName(nameBuffer);


			currCamera->setStartPosition(vec4(posBuffer[0], posBuffer[1], posBuffer[2], 0));
			currCamera->updateTransform();
			ResetPopUpFlags();
		}
		else if (GUI_popup_pressedCANCEL)
		{
			delete cameras[cameras.size() - 1];
			cameras.pop_back();

			ResetPopUpFlags();
		}
	}
	else if (add_showLightDlg)
	{
		if (GUI_popup_pressedOK)
		{
			auto currLight = lights.back();
			currLight->setName(nameBuffer);
			currLight->setPosition(vec3(posBuffer[0], posBuffer[1], posBuffer[2]));
			ResetPopUpFlags();
			m_renderer->UpdateLightsUBO(true);
		}
		else if (GUI_popup_pressedCANCEL)
		{
			delete lights[lights.size() - 1];
			lights.pop_back();
			ResetPopUpFlags();
			m_renderer->UpdateLightsUBO(true);

		}
	}
	else if (simulation_showNumRaysDlg)
	{
		if (GUI_popup_pressedOK)
		{
			rt->N_Rays_Updated();
			n_rays = this->num_of_rays;
			ResetPopUpFlags();
		}
		else if (GUI_popup_pressedCANCEL) {
			num_of_rays = n_rays;
			ResetPopUpFlags();
		}
	}
	else if (simulation_showFileSavedDlg)
	{
		if (GUI_popup_pressedOK)
			ResetPopUpFlags();
	}
}

void colorPicker(ImVec4* color, std::string button_label, std::string id)
{
	ImGuiColorEditFlags flags = (ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_InputRGB);

	if (saved_palette_init)
	{
		for (int n = 0; n < IM_ARRAYSIZE(saved_palette); n++)
		{
			ImGui::ColorConvertHSVtoRGB(n / 31.0f, 0.8f, 0.8f,
				saved_palette[n].x, saved_palette[n].y, saved_palette[n].z);
			saved_palette[n].w = 1.0f; // Alpha
		}
		saved_palette_init = false;
	}

	bool open_popup = ImGui::ColorButton((button_label+id).c_str(), *color, flags);
	ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
	ImGui::Text(button_label.c_str());
	if (open_popup)
		ImGui::OpenPopup(("Color Palette"  + id).c_str());

	if (ImGui::BeginPopup(("Color Palette" + id).c_str()))
	{
		ImGui::ColorPicker4(id.c_str(), (float*)color, flags | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoAlpha);

		ImGui::EndPopup();
	}
}

void Scene::drawCameraTab()
{
	string name(cameras[activeCamera]->getName());
	ImGui::SeparatorText(name.c_str());

	//ImGui::Checkbox("Render Camera", &cameras[activeCamera]->renderCamera);
	//bool* g_allowClipping = &(cameras[activeCamera]->allowClipping);
	//ImGui::Checkbox("Allow clipping", g_allowClipping);


	float* g_left = &(cameras[activeCamera]->c_left);
	float* g_right = &(cameras[activeCamera]->c_right);
	float* g_top = &(cameras[activeCamera]->c_top);
	float* g_bottom = &(cameras[activeCamera]->c_bottom);
	float* g_zNear = &(cameras[activeCamera]->c_zNear);
	float* g_zFar = &(cameras[activeCamera]->c_zFar);
	float* g_fovy = &(cameras[activeCamera]->c_fovy);
	float* g_aspect = &(cameras[activeCamera]->c_aspect);
	vec4* g_rot = &(cameras[activeCamera]->c_rot);
	vec4* g_trnsl = &(cameras[activeCamera]->c_trnsl);

	vec4* g_rot_view   = &(cameras[activeCamera]->c_rot_viewspace);
	vec4* g_trnsl_view = &(cameras[activeCamera]->c_trnsl_viewspace);


	float prev_left = *g_left;
	float prev_right = *g_right;
	float prev_bottom = *g_bottom;
	float prev_top = *g_top;
	float prev_zNear = *g_zNear;
	float prev_zFar = *g_zFar;

	ImGui::SeparatorText("Projection");
	ImGui::RadioButton("Orthographic", &g_ortho, 1); ImGui::SameLine();
	ImGui::RadioButton("Perspective", &g_ortho, 0);


	ImGui::DragFloat("##Left_CP", g_left, 0.01f, 0, 0, "Left = %.1f "); ImGui::SameLine();
	ImGui::DragFloat("##Right_CP", g_right, 0.01f, 0, 0, "Right = %.1f ");


	ImGui::DragFloat("##Top_CP", g_top, 0.01f, 0, 0, "Top = %.1f "); ImGui::SameLine();
	ImGui::DragFloat("##Bottom_CP", g_bottom, 0.01f, 0, 0, "Bot = %.1f ");


	ImGui::DragFloat("##zNear_CP", g_zNear, 0.01f, 0, 0, "z-Near = %.1f "); ImGui::SameLine();
	ImGui::DragFloat("##zFar_CP", g_zFar, 0.01f, 0, 0, "z-Far = %.1f ");


	if (g_ortho == 1) /* Ortho type */
	{
		if (cameras[activeCamera]->isOrtho == false)
		{
			cameras[activeCamera]->isOrtho = true;
			cameras[activeCamera]->resetProjection();
		}

		if (prev_left != *g_left || prev_right != *g_right ||
			prev_bottom != *g_bottom || prev_top != *g_top ||
			prev_zNear != *g_zNear || prev_zFar != *g_zFar)
		{
			cameras[activeCamera]->setOrtho();
		}
	}
	else /* Perspective type */
	{
		if (cameras[activeCamera]->isOrtho == true)
		{
			cameras[activeCamera]->isOrtho = false;
			cameras[activeCamera]->resetProjection();
		}
		
		float prev_fovy = *g_fovy;
		float prev_aspect = *g_aspect;

		ImGui::DragFloat("##FovY", g_fovy, 0.01f, FOV_RANGE_MIN, FOV_RANGE_MAX, "FovY = %.1f "); ImGui::SameLine();
		ImGui::DragFloat("##Aspect", g_aspect, 0.01f, ASPECT_RANGE_MIN, ASPECT_RANGE_MAX, "Aspect = %.1f "); ImGui::SameLine();
		ImGui::Checkbox("Lock FovY", cameras[activeCamera]->getLockFovyPTR());

		if (prev_fovy != *g_fovy || prev_aspect != *g_aspect) //User changed FOV or Aspect Ratio
		{
			cameras[activeCamera]->setPerspectiveByFov();
		}
		else if( prev_left != *g_left || prev_right != *g_right || prev_bottom != *g_bottom ||
				 prev_top  != *g_top  || prev_zNear != *g_zNear || prev_zFar != *g_zFar )
		{
			if (prev_zNear == *g_zNear)
			{
				//zNear didn't changed - other paramater changed - UnLock fovy.
				cameras[activeCamera]->unLockFovy();
			}
			
			cameras[activeCamera]->setPerspectiveByParams();
		}

	}
	
	if (ImGui::Button("reset"))
	{
		cameras[activeCamera]->resetProjection();
	}


	ImGui::SeparatorText("View space");

	vec4 prev_trnsl_view = *g_trnsl_view;
	vec4 prev_rot_view = *g_rot_view;

	ImGui::Text("Translation (X Y Z)");
	ImGui::DragFloat("##X_VT", &(g_trnsl_view->x), 0.01f, 0, 0, "%.1f"); ImGui::SameLine();
	ImGui::DragFloat("##Y_VT", &(g_trnsl_view->y), 0.01f, 0, 0, "%.1f"); ImGui::SameLine();
	ImGui::DragFloat("##Z_VT", &(g_trnsl_view->z), 0.01f, 0, 0, "%.1f"); ImGui::SameLine();
	if (ImGui::Button("reset##VT"))
	{
		cameras[activeCamera]->ResetTranslation_viewspace();
	}

	ImGui::Text("Rotation (X Y Z)");
	ImGui::DragFloat("##X_VR", &(g_rot_view->x), 0.1f, 0, 0, "%.0f"); ImGui::SameLine();
	ImGui::DragFloat("##Y_VR", &(g_rot_view->y), 0.1f, 0, 0, "%.0f"); ImGui::SameLine();
	ImGui::DragFloat("##Z_VR", &(g_rot_view->z), 0.1f, 0, 0, "%.0f"); ImGui::SameLine();
	if (ImGui::Button("reset##VR"))
	{
		cameras[activeCamera]->ResetRotation_viewspace();
	}

	if (prev_trnsl_view != *g_trnsl_view || prev_rot_view != *g_rot_view)
	{
		cameras[activeCamera]->updateTransform();
	}


	ImGui::SeparatorText("World space");

	vec4 prev_trnsl = *g_trnsl;
	vec4 prev_rot = *g_rot;

	ImGui::Text("Translation (X Y Z)");
	ImGui::DragFloat("##X_MT", &(g_trnsl->x), 0.01f, 0, 0, "%.1f"); ImGui::SameLine();
	ImGui::DragFloat("##Y_MT", &(g_trnsl->y), 0.01f, 0, 0, "%.1f"); ImGui::SameLine();
	ImGui::DragFloat("##Z_MT", &(g_trnsl->z), 0.01f, 0, 0, "%.1f"); ImGui::SameLine();
	if (ImGui::Button("reset##MT"))
	{
		cameras[activeCamera]->ResetTranslation();
	}

	ImGui::Text("Rotation (X Y Z)");
	ImGui::DragFloat("##X_MR", &(g_rot->x), 0.1f, 0, 0, "%.0f"); ImGui::SameLine();
	ImGui::DragFloat("##Y_MR", &(g_rot->y), 0.1f, 0, 0, "%.0f"); ImGui::SameLine();
	ImGui::DragFloat("##Z_MR", &(g_rot->z), 0.1f, 0, 0, "%.0f"); ImGui::SameLine();
	if (ImGui::Button("reset##MR"))
	{
		cameras[activeCamera]->ResetRotation();
	}

	if (prev_trnsl != *g_trnsl || prev_rot != *g_rot)
	{
		cameras[activeCamera]->updateTransform();
	}

	if (cameras.size() > 1)
	{
		// Delete camera
		ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0, 0.6f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0, 0.7f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0, 0.8f, 0.8f));
		if (ImGui::Button("Delete camera"))
		{
			cameras.erase(cameras.begin() + activeCamera);
			activeCamera = max(0, activeCamera - 1);
		}
		ImGui::PopStyleColor(3);
	}
}

void Scene::drawModelTab()
{
	MeshModel* activeMesh = (MeshModel*)models[activeModel];
	string name(models[activeModel]->getName());
	ImGui::SeparatorText(name.c_str());

	vec4* g_trnsl = &(activeMesh->_trnsl);
	vec4* g_rot = &(activeMesh->_rot);

	bool* dispFaceNormal = &(activeMesh->showFaceNormals);
	bool* dispVertexNormal = &(activeMesh->showVertexNormals);
	bool* dispBoundingBox = &(activeMesh->showBoundingBox);

	float* lenFaceNormal = activeMesh->getLengthFaceNormal();
	float* lenVertNormal = activeMesh->getLengthVertexNormal();

	if (ImGui::CollapsingHeader("Model"))
	{
		ImGui::Indent();
		if (ImGui::CollapsingHeader("Model space"))
		{
			//ImGui::SeparatorText("Model space");

			ImGui::Text("Translation (X Y Z)");
			ImGui::DragFloat("##X_MT", &(g_trnsl->x), 0.01f, 0, 0, "%.1f"); ImGui::SameLine();
			ImGui::DragFloat("##Y_MT", &(g_trnsl->y), 0.01f, 0, 0, "%.1f"); ImGui::SameLine();
			ImGui::DragFloat("##Z_MT", &(g_trnsl->z), 0.01f, 0, 0, "%.1f"); ImGui::SameLine();
			if (ImGui::Button("reset##MT"))
			{
				activeMesh->ResetUserTransform_translate_model();
			}

			ImGui::Text("Rotation (X Y Z)");
			ImGui::DragFloat("##X_MR", &(g_rot->x), 0.1f, 0, 0, "%.0f"); ImGui::SameLine();
			ImGui::DragFloat("##Y_MR", &(g_rot->y), 0.1f, 0, 0, "%.0f"); ImGui::SameLine();
			ImGui::DragFloat("##Z_MR", &(g_rot->z), 0.1f, 0, 0, "%.0f"); ImGui::SameLine();
			if (ImGui::Button("reset##MR"))
			{
				activeMesh->ResetUserTransform_rotate_model();
			}

			vec4* g_scale = &(activeMesh->_scale);

			ImGui::Text("Scale (X Y Z)");
			ImGui::Checkbox("keep ratio", &constScaleRatio);
			ImGui::DragFloat("##X_MS", &(g_scale->x), 0.01f, 0, 0, "%.6f"); ImGui::SameLine();
			if (constScaleRatio)
			{
				g_scale->y = g_scale->z = g_scale->x;
			}
			else
			{
				ImGui::DragFloat("##Y_MS", &(g_scale->y), 0.01f, 0, 0, "%.3f"); ImGui::SameLine();
				ImGui::DragFloat("##Z_MS", &(g_scale->z), 0.01f, 0, 0, "%.3f"); ImGui::SameLine();
			}

			if (ImGui::Button("reset##MS"))
			{
				activeMesh->ResetUserTransform_scale_model();
			}
		}

		if (ImGui::CollapsingHeader("World space"))
		{
			//ImGui::SeparatorText("World space");

			vec4* trnsl_w = &(activeMesh->_trnsl_w);
			vec4* rot_w = &(activeMesh->_rot_w);
			vec4* scale_w = &(activeMesh->_scale_w);


			ImGui::Text("Translation (X Y Z)");
			ImGui::DragFloat("##X_WT", &(trnsl_w->x), 0.01f, 0, 0, "%.1f"); ImGui::SameLine();
			ImGui::DragFloat("##Y_WT", &(trnsl_w->y), 0.01f, 0, 0, "%.1f"); ImGui::SameLine();
			ImGui::DragFloat("##Z_WT", &(trnsl_w->z), 0.01f, 0, 0, "%.1f"); ImGui::SameLine();

			if (ImGui::Button("reset##WT"))
			{
				activeMesh->ResetUserTransform_translate_world();
			}




			ImGui::Text("Rotation (X Y Z)");
			ImGui::DragFloat("##X_WR", &(rot_w->x), 0.1f, 0, 0, "%.0f"); ImGui::SameLine();
			ImGui::DragFloat("##Y_WR", &(rot_w->y), 0.1f, 0, 0, "%.0f"); ImGui::SameLine();
			ImGui::DragFloat("##Z_WR", &(rot_w->z), 0.1f, 0, 0, "%.0f"); ImGui::SameLine();
			if (ImGui::Button("reset##WR"))
			{
				activeMesh->ResetUserTransform_rotate_world();
			}


			ImGui::Text("Scale (X Y Z)");
			ImGui::Checkbox("keep ratio##keepRatioWorld", &constScaleRatio_w);
			ImGui::DragFloat("##X_WS", &(scale_w->x), 0.01f, 0, 0, "%.6f"); ImGui::SameLine();
			if (constScaleRatio_w)
			{
				scale_w->y = scale_w->z = scale_w->x;
			}
			else
			{
				ImGui::DragFloat("##Y_WS", &(scale_w->y), 0.01f, 0, 0, "%.3f"); ImGui::SameLine();
				ImGui::DragFloat("##Z_WS", &(scale_w->z), 0.01f, 0, 0, "%.3f"); ImGui::SameLine();
			}
			if (ImGui::Button("reset##WS"))
			{
				activeMesh->ResetUserTransform_scale_world();
			}
		}
		ImGui::Unindent();
	}

	// Delete model
	ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0, 0.6f, 0.6f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0, 0.7f, 0.7f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0, 0.8f, 0.8f));
	if (ImGui::Button("Delete model"))
	{
		models.erase(models.begin() + activeModel);
		activeModel = NOT_SELECTED;
	}
	ImGui::PopStyleColor(3);
}

void Scene::drawLightTab()
{
	Light* currentLight = (Light*)lights[activeLight];
	string name(currentLight->getName());
	ImGui::SeparatorText(name.c_str());
	bool somethingChanged = false;

	vec3* light_pos = currentLight->getPositionPtr();
	vec3* light_dir = currentLight->getDirectionPtr();
	light_type_radio_button = (int)currentLight->getLightType();

	ImGui::RadioButton("Ambient",  &light_type_radio_button, AMBIENT_LIGHT);
	ImGui::RadioButton("Point",    &light_type_radio_button, POINT_LIGHT);  
	ImGui::RadioButton("Parallel", &light_type_radio_button, PARALLEL_LIGHT);

	if ((int)currentLight->getLightType() != light_type_radio_button)
		somethingChanged = true;

	currentLight->setLightType(light_type_radio_button);


	vec3& color = currentLight->getColor();

	ImVec4 color_local = ImVec4(color.x, color.y, color.z, 1);

	colorPicker(&color_local, "Color", "##LightColor");

	if (color != vec3(color_local.x, color_local.y, color_local.z))
		somethingChanged = true;

	color.x = color_local.x;
	color.y = color_local.y;
	color.z = color_local.z;
	
	if (currentLight->getLightType() == POINT_LIGHT)
	{
		ImGui::SeparatorText("Position");

		vec3* pos = currentLight->getPositionPtr();
		vec3 prevPos = *pos;
		ImGui::Text("Position (X Y Z)");
		ImGui::DragFloat("##X_WPL", &(pos->x), 0.01f, 0, 0, "%.1f"); ImGui::SameLine();
		ImGui::DragFloat("##Y_WPL", &(pos->y), 0.01f, 0, 0, "%.1f"); ImGui::SameLine();
		ImGui::DragFloat("##Z_WPL", &(pos->z), 0.01f, 0, 0, "%.1f"); ImGui::SameLine();
		if (ImGui::Button("reset##LP"))
		{
			currentLight->resetPosition();
		}
		if(*pos != prevPos)
			somethingChanged = true;
	}

	if (currentLight->getLightType() == PARALLEL_LIGHT)
	{
		ImGui::SeparatorText("Direction");

		vec3* dir = currentLight->getDirectionPtr();
		vec3 prevDir = *dir;
		ImGui::Text("Direction (X Y Z)");
		ImGui::DragFloat("##X_WDL", &(dir->x), 0.1f, 0, 0, "%.1f"); ImGui::SameLine();
		ImGui::DragFloat("##Y_WDL", &(dir->y), 0.1f, 0, 0, "%.1f"); ImGui::SameLine();
		ImGui::DragFloat("##Z_WDL", &(dir->z), 0.1f, 0, 0, "%.1f"); ImGui::SameLine();
		if (ImGui::Button("reset##LD"))
		{
			currentLight->resetDirection();
		}
		if (*dir != prevDir)
			somethingChanged = true;
	}

	ImGui::SeparatorText("Light Intensity");

	float* la = &(currentLight->La);
	float* ld = &(currentLight->Ld);
	float* ls = &(currentLight->Ls);

	float prevLa = *la;
	float prevLd = *ld;
	float prevLs = *ls;

	if (currentLight->getLightType() == AMBIENT_LIGHT)
	{
		ImGui::Text("Ambient Intensity "); ImGui::SameLine();
		ImGui::DragFloat("##I_amb", la, 0.001f, 0, 10, "%.3f");
	}

	else
	{
		ImGui::Text("Intensity   "); ImGui::SameLine();
		ImGui::DragFloat("##I_dif", ld, 0.001f, 0, 10, "%.6f");
	
		//ImGui::Text("Specular Intensity"); ImGui::SameLine();
		//ImGui::DragFloat("##I_spc", ls, 0.001f, 0, 10, "%.3f");
	}


	if (ImGui::Button("Reset all intensity##RI"))
	{
		*la = DEFUALT_LIGHT_LA_VALUE;
		*ld = DEFUALT_LIGHT_LD_VALUE;
		*ls = DEFUALT_LIGHT_LS_VALUE;
	}
	if (prevLa != *la || prevLd != *ld || prevLs != *ls)
		somethingChanged = true;

	if (somethingChanged)
		m_renderer->UpdateLightsUBO(false);
}

void Scene::drawSimSettingsTab()
{
	bool colorsChanged = false;
	ImGui::SeparatorText("Simulation configuration");

	ImGui::DragInt("Number of rays##TOTAL_RAYS", &num_of_rays); if (n_rays != num_of_rays) n_rays = num_of_rays;

	ImGui::SeparatorText("Method");

	ImGui::RadioButton("CPU", &cpu_mode, 1);
	ImGui::RadioButton("GPU", &cpu_mode, 0);

	ImGui::SeparatorText("Visualizations");

	ImVec4 color_local_hit = ImVec4(hitColor.x, hitColor.y, hitColor.z, 1);
	colorPicker(&color_local_hit, "Rays Hit Color", "##HitColor");
	if (hitColor != vec3(color_local_hit.x, color_local_hit.y, color_local_hit.z))
		colorsChanged = true;
	hitColor.x = color_local_hit.x;
	hitColor.y = color_local_hit.y;
	hitColor.z = color_local_hit.z;

	ImVec4 color_local_mis = ImVec4(misColor.x, misColor.y, misColor.z, 1);
	colorPicker(&color_local_mis, "Rays Miss Color", "##MisColor");
	if (misColor != vec3(color_local_mis.x, color_local_mis.y, color_local_mis.z))
		colorsChanged = true;
	misColor.x = color_local_mis.x;
	misColor.y = color_local_mis.y;
	misColor.z = color_local_mis.z;

	ImVec4 color_local_hitP = ImVec4(hitPColor.x, hitPColor.y, hitPColor.z, 1);
	colorPicker(&color_local_hitP, "Hit Points Color", "##HitPColor");
	if (hitPColor != vec3(color_local_hitP.x, color_local_hitP.y, color_local_hitP.z))
		colorsChanged = true;
	hitPColor.x = color_local_hitP.x;
	hitPColor.y = color_local_hitP.y;
	hitPColor.z = color_local_hitP.z;

	ImGui::Checkbox("Show Hit Rays", &display_rays_hits);
	ImGui::Checkbox("Show Miss Rays", &display_rays_misses);
	ImGui::Checkbox("Show Hit Points", &display_hit_points);
	ImGui::DragFloat("Points size##PTS_SIZE", &pointSize, 0.1f, 0.1f, 100.0f, "%.1f");



	if (colorsChanged)
		rt->UpdateColorsUniforms();
	
	ImGui::SeparatorText("Simulation Actions");
	if (models.size() > 0 && num_of_rays > 0 && rt && rt->route.size() > 0) {
		if (ImGui::Button("Start Simulation##simulate"))
		{
			rt->StartSimulation(cpu_mode);
			selectedTab = SIM_RES_TAB_INDEX;
		}
	}
	else
	{
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
		if (models.size() == 0)
			ImGui::Text("Please load a model.");
		else if (num_of_rays <= 0)
			ImGui::Text("Please set the \"number of rays\" to a positive value.");
		else if (rt && rt->route.size() == 0)
			ImGui::Text("Please use the \"Path\" tab to create a path.");
		ImGui::PopFont();
	}


}

void Scene::drawSimResultsTab()
{
	ImGui::SeparatorText("Simulation Results");
	
	if (rt->sim_result.route_pts == 0) return;

	std::string method		= "Method: ";
	std::string route_pts	= "Path check-points: ";
	std::string rays_pt		= "Rays per check-point: ";
	std::string hits		= "Total Hits: ";
	std::string total_rays	= "Total Rays: ";
	std::string hit_ratio	= "Hit Ratio: ";
	std::string total_poly	= "Total polygons: ";
	std::string time		= "Time [seconds]: ";
	std::string time_mili	= "Time [millisec]: ";
	std::string time_micro	= "Time [microsec]: ";
	
	route_pts	+= std::to_string(rt->sim_result.route_pts);
	hits		+= std::to_string(rt->sim_result.hits);
	rays_pt		+= std::to_string((int) (rt->sim_result.rays / rt->sim_result.route_pts));
	total_rays	+= std::to_string(rt->sim_result.rays);
	hit_ratio	+= floatToStringWithPrecision((float)rt->sim_result.hits * 100.0f / (float)rt->sim_result.rays, 1) + " %%";
	total_poly	+= std::to_string(rt->sim_result.polygons);
	time		+= floatToStringWithPrecision(rt->sim_result.time_seconds, 3);
	time_mili	+= std::to_string(rt->sim_result.time_milli);
	time_micro	+= std::to_string(rt->sim_result.time_micro);
	method		+= rt->sim_result.cpu_mode == 1 ? "CPU" : "GPU";

	ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]); // 2 is the index of the larger font
	ImGui::Text(method.c_str());
	ImGui::Text(route_pts.c_str());
	ImGui::Text(rays_pt.c_str());
	ImGui::Text(hits.c_str());
	ImGui::Text(total_rays.c_str());
	ImGui::Text(hit_ratio.c_str());
	ImGui::Text(total_poly.c_str());
	ImGui::Text(time.c_str());
	ImGui::Text(time_mili.c_str());
	ImGui::Text(time_micro.c_str());
	ImGui::PopFont();

	if (ImGui::Button("Save results##sim_save"))
	{
		std::string directory_main = "Simulations";
		std::filesystem::create_directory(directory_main); // Creates the directory if it doesn't exist

		std::string directory_CPU = directory_main + "/CPU";
		std::filesystem::create_directory(directory_CPU); // Creates the directory if it doesn't exist

		std::string directory_GPU = directory_main + "/GPU";
		std::filesystem::create_directory(directory_GPU); // Creates the directory if it doesn't exist

		// Open a file in write mode
		string fname = (rt->sim_result.cpu_mode == 1 ? directory_CPU : directory_GPU) + "/" + "simulation_results_" + GetCurrentTimeString() + ".txt";
		std::ofstream file(fname);

		// Check if the file is successfully opened
		if (!file.is_open()) {
			std::cerr << "Error: Unable to open file for writing.\n";
			return;
		}

		// Write the results to the file
		file << "Simulation Results:\n";
		file << method << "\n";
		file << route_pts << "\n";
		file << rays_pt << "\n";
		file << hits << "\n";
		file << total_rays << "\n";
		file << hit_ratio << "\n";
		file << total_poly << "\n";
		file << time << "\n";
		file << time_mili << "\n";
		file << time_micro << "\n";
		
		file << "\n" << "List of all Hit points (sorted by length of ray)\nFormat: (x,y,z) - length :" << "\n";
		std::vector<HIT> sorted;

		for (int i = 0; i < rt->getHits().size(); i++) {
			for (const auto& h : rt->getHits()[i]) {
				sorted.push_back(h);
			}
		}

		// Sort by the 'value' field
		std::sort(sorted.begin(), sorted.end(), [](const HIT& a, const HIT& b) {
			return a.distance < b.distance;
			});

		for (const auto& i : sorted) {
			file << std::fixed << std::setprecision(3) << i.hit_point_w << " - " << i.distance << "\n";
		}

		

		// Close the file
		file.close();
		simulation_showFileSavedDlg = true;
		std::cout << "Results successfully saved to " << fname << std::endl;
	}
}

float easeInOutSin(float t) {
	return 0.5 * (1 - cos(t * M_PI));
}

float easeInOutArctan(float t, float k = 6.0f) {
	return (atan(k * (2.0f * t - 1.0f)) / (2.0f * atan(k))) + 0.5f;
}



void threadFunction(Scene* c, vec4 target, bool ascending) {
	const int animationTime = 820; // milliseconds
	const int fps = 100;

	int delta = 1e6 / fps;
	int chunks = ((float)animationTime / 1000.0f) * fps;

	vec4 start = c->GetActiveCamera()->c_trnsl;
	vec4 start_rot = c->GetActiveCamera()->c_rot;
	vec4 target_rot = start_rot + (ascending ? vec4(-45, 0, 0, 0) : vec4(45, 0, 0, 0));
	vec4 delta_pos = target - start;

	for (int i = 0; i <= chunks; i++) {
		float t = (float)i / (float)chunks; // Linear progress [0, 1]
		float eased_t = easeInOutArctan(t); // Apply easing function

		// Interpolate position and rotation using eased progress
		c->GetActiveCamera()->c_trnsl = start + delta_pos * eased_t;
		c->GetActiveCamera()->c_rot = start_rot + (target_rot - start_rot) * eased_t;
		c->GetActiveCamera()->updateTransform();

		std::this_thread::sleep_for(std::chrono::microseconds(delta));
	}
}

void Scene::drawSimRouteTab()
{
	ImGui::SeparatorText("Path Editor");
	bool path_mode = add_path_mode;
	ImGui::Checkbox("Path editor", &add_path_mode);
	if (path_mode != add_path_mode) {
		vec4 lookat_target = GetActiveCamera()->c_trnsl + normalize(vec4(GetActiveCamera()->calculateForwardVector())) * 5;
		lookat_target.w = 0;
		if (add_path_mode == true) {
			std::thread myThread(threadFunction, this, GetActiveCamera()->c_trnsl + vec4(0, 8, 0, 0), true);
			myThread.detach();
		}
		else {
			std::thread myThread(threadFunction, this, GetActiveCamera()->c_trnsl + vec4(0, -8, 0, 0), false);
			myThread.detach();
		}
	}

	ImGui::SeparatorText("Simulation Route");

	ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]); // 2 is the index of the larger font
	for (uint i = 0; i < rt->route.size(); i++)
	{
		std::string line = "Point " + std::to_string(i + 1) + " (x,y,z)";
		//std::string p = "(" + std::to_string(rt->route[i].x) + ", " + std::to_string(rt->route[i].y) + ", " + std::to_string(rt->route[i].z) + ")";
		ImGui::Text(line.c_str()); ImGui::SameLine();
		vec3 local_values = rt->route[i];
		ImGui::DragFloat((string("##X_PT_") + std::to_string(i)).c_str(), &(rt->route[i].x), 0.01f, 0, 0, "%.3f"); ImGui::SameLine();
		ImGui::DragFloat((string("##Y_PT_") + std::to_string(i)).c_str(), &(rt->route[i].y), 0.01f, 0, 0, "%.3f"); ImGui::SameLine();
		ImGui::DragFloat((string("##Z_PT_") + std::to_string(i)).c_str(), &(rt->route[i].z), 0.01f, 0, 0, "%.3f"); ImGui::SameLine();
		if (local_values != rt->route[i]) {
			rt->UpdateRoutePts();
		}
		if (ImGui::Button((string("remove##del_pt_") + std::to_string(i)).c_str()))
		{
			rt->route.erase(rt->route.begin() + i);
			rt->UpdateRoutePts();
		}
	}
	ImGui::PopFont();
}

void Scene::addPointPath(vec2 mousePos)
{
	rt->route.push_back(Get3dPointFromScreen(mousePos));
	rt->UpdateRoutePts();
}

vec3 Scene::Get3dPointFromScreen(vec2 mousePos)
{
	// Step 1: Convert screen coordinates to Normalized Device Coordinates (NDC)
	mousePos -= vec2(viewportX, viewportY + ImGui::GetTextLineHeightWithSpacing() +
		ImGui::GetStyle().FramePadding.y * 2.0f - 5);
	float xNDC = (2.0f * mousePos.x) / viewportWidth - 1.0f;
	float yNDC = 1.0f - (2.0f * mousePos.y) / viewportHeight;

	xNDC = max(min(xNDC, 1.0f), -1.0f);
	yNDC = max(min(yNDC, 1.0f), -1.0f);

	//std::cout << "(debug) NDC coords: " << vec2(xNDC, yNDC) << "\n";

	// Step 2: Generate a ray in world space
	vec4 rayClipNear(xNDC, yNDC, -1.0f, 1.0f);
	vec4 rayClipFar(xNDC, yNDC, 1.0f, 1.0f);

	// Transform clip coordinates to world coordinates
	mat4 inverseVP = InverseMatrix(GetActiveCamera()->projection * GetActiveCamera()->cTransform);
	vec4 rayWorldNear = inverseVP * rayClipNear;
	vec4 rayWorldFar = inverseVP * rayClipFar;

	vec3 rayWorldNear3d = vec3(rayWorldNear.x, rayWorldNear.y, rayWorldNear.z) / rayWorldNear.w;
	vec3 rayWorldFar3d = vec3(rayWorldFar.x, rayWorldFar.y, rayWorldFar.z) / rayWorldFar.w;

	vec3 rayOrigin = rayWorldNear3d;
	vec3 rayDirection = normalize(rayWorldFar3d - rayWorldNear3d);

	// Step 3: Compute intersection with y = 0 plane (XZ plane)
	if (abs(rayDirection.y) < 1e-6f) { // Avoid division by zero if the ray is parallel to the plane
		std::cerr << "Ray is parallel to the y=0 plane; no intersection.\n";
		return vec3(0);
	}

	float t = -rayOrigin.y / rayDirection.y; // Solve for t where y = 0
	vec3 pointOnXZPlane = rayOrigin + t * rayDirection;
	pointOnXZPlane.y = 0;
	return pointOnXZPlane;
}

Camera* Scene::GetActiveCamera()
{
	return (activeCamera == NOT_SELECTED ? nullptr : cameras[activeCamera]);
}

Model* Scene::GetActiveModel()
{
	return (activeModel == NOT_SELECTED ? nullptr : models[activeModel]);
}

Light* Scene::GetActiveLight()
{
	return (activeLight == NOT_SELECTED ? nullptr : lights[activeLight]);
}

void Scene::UpdateModelSelection()
{
	int len = models.size();
	for (int i = 0; i < len; i++)
	{
		models[i]->selected = false;
	}
	models[activeModel]->selected = true;
}

void Scene::UpdateGeneralUniformInGPU()
{
	glUniform1i(glGetUniformLocation(m_renderer->program, "algo_shading"), (int)draw_algo);
	glUniform1i(glGetUniformLocation(m_renderer->program, "numLights"), (int)lights.size());
	glUniform1i(glGetUniformLocation(m_renderer->program, "simulation"), 0);
	glUniform1i(glGetUniformLocation(m_renderer->program, "displayMisses"), 0);
	glUniform1i(glGetUniformLocation(m_renderer->program, "displayHits"), 0);
	glUniform1i(glGetUniformLocation(m_renderer->program, "displayHitPoints"), 0);
	GetActiveCamera()->UpdateProjectionMatInGPU();
}

void Scene::setViewPort(vec4& vp)
{
	viewportX	   = vp.x;
	viewportY	   = vp.y;
	viewportWidth  = vp.z;
	viewportHeight = vp.w;

	std::cout << "viewport = " << vp << "\n";
}

void Scene::resize_callback_handle(int width, int height)
{
	/* Take the main menu bar height into account */
	float mainMenuBarHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.0f;
	height -= (int)mainMenuBarHeight;
	
	/* Take the Transformatins Widnow width into account (if opened) */
	int transormationWindowGap = showTransWindow ? transformationWindowWidth : 0;
	width -= transormationWindowGap;


	// Calculate aspect ratio
	float aspect = (float)width / (float)height;

	// Calculate new viewport size
	int newWidth = width, newHeight = height;
	if (aspect > modelAspectRatio)
		newWidth = (int)((float)height * modelAspectRatio);
	else
		newHeight = (int)((float)width / modelAspectRatio);

	// Calculate viewport position to keep it centered
	int xOffset = (abs(width - newWidth) / 2) + transormationWindowGap;
	int yOffset = (abs(height - newHeight) / 2) + 3;

	// Set viewport
	glViewport(xOffset, yOffset, newWidth, newHeight);

	//Update buffer
	//m_renderer->update(newWidth, newHeight);

	//Update Scene
	setViewPort(vec4(xOffset, yOffset, newWidth, newHeight));
}