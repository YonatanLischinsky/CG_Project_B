#include "Renderer.h"
#include "CG_skel_w_glfw.h"
#include "InitShader.h"
#include "GL\freeglut.h"
#include "MeshModel.h"
#include "RayTransmitter.h"


extern Scene* scene;
extern RayTransmitter* rt;

Renderer::Renderer(int width, int height, GLFWwindow* window) :
	m_width(width), m_height(height)
{
	m_window = window;
	InitOpenGLRendering();

}

Renderer::~Renderer(void)
{
	
}

void Renderer::SwapBuffers()
{
	glfwSwapBuffers(m_window);
}

void Renderer::CreateTexture()
{
}

void Renderer::updateTexture()
{
}

vec2 Renderer::GetWindowSize()
{
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	return vec2(width, height);
}

vec2 Renderer::GetBufferSize()
{
	return vec2(m_width, m_height);
}

void Renderer::clearBuffer()
{
	glClearColor(DEFAULT_BACKGROUND_COLOR, DEFAULT_BACKGROUND_COLOR, DEFAULT_BACKGROUND_COLOR, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	float wireFrameColor = 1 - DEFAULT_BACKGROUND_COLOR;
	glUniform3f(glGetUniformLocation(program, "wireframeColor"), wireFrameColor, wireFrameColor, wireFrameColor);
}

void Renderer::drawModel(DrawAlgo draw_algo, Model* model, mat4& cTransform)
{
	if (!model ) return;
	MeshModel* pModel = (MeshModel*)model;

	pModel->UpdateModelViewInGPU( cTransform, scene->GetActiveCamera()->rotationMat_normals);
	pModel->UpdateMaterialinGPU();
	pModel->UpdateTextureInGPU();
	pModel->UpdateAnimationInGPU();

	if (draw_algo == WIRE_FRAME)
	{
		glBindVertexArray(pModel->VAOs[VAO_VERTEX_WIREFRAME]);
		glDrawArrays(GL_LINES, 0, pModel->GetBuffer_len(MODEL_WIREFRAME));
	}
	else
	{
		glBindVertexArray(pModel->VAOs[VAO_VERTEX_TRIANGLE]);
		glDrawArrays(GL_TRIANGLES, 0, pModel->GetBuffer_len(MODEL_TRIANGLES));
	}

	if (pModel->showBoundingBox)
	{
		glBindVertexArray(pModel->VAOs[VAO_VERTEX_BBOX]);
		glUniform1i(glGetUniformLocation(program, "displayBBox"), 1);
		glDrawArrays(GL_LINES, 0, pModel->GetBuffer_len(BBOX));
		glUniform1i(glGetUniformLocation(program, "displayBBox"), 0);
	}

	if (pModel->showVertexNormals)
	{
		glBindVertexArray(pModel->VAOs[VAO_VERTEX_VNORMAL]);
		glUniform1i(glGetUniformLocation(program, "displayVnormal"), 1);
		glUniform1f(glGetUniformLocation(program, "vnFactor"), *pModel->getLengthVertexNormal());
		glDrawArrays(GL_LINES, 0, pModel->GetBuffer_len(V_NORMAL));
		glUniform1i(glGetUniformLocation(program, "displayVnormal"), 0);
	}

	if (pModel->showFaceNormals)
	{
		glBindVertexArray(pModel->VAOs[VAO_VERTEX_FNORMAL]);
		glUniform1i(glGetUniformLocation(program, "displayFnormal"), 1);
		glUniform1f(glGetUniformLocation(program, "fnFactor"), *pModel->getLengthFaceNormal());
		glDrawArrays(GL_LINES, 0, pModel->GetBuffer_len(F_NORMAL));
		glUniform1i(glGetUniformLocation(program, "displayFnormal"), 0);
	}
	
	glBindVertexArray(0);

}

void Renderer::drawRays(mat4& cTransform)
{
	if (!rt ) return;

	rt->UpdateModelViewInGPU(scene->GetActiveCamera()->cTransform, scene->GetActiveCamera()->rotationMat_normals);

	glBindVertexArray(rt->VAO);
	glUniform1i(glGetUniformLocation(program, "displayRays"), 1);
	glDrawArrays(GL_LINES, 0, rt->GetBufferLen());
	glUniform1i(glGetUniformLocation(program, "displayRays"), 0);
	

	glBindVertexArray(0);

}

void Renderer::UpdateLightsUBO(bool reallocate_ubo)
{
	glBindBuffer(GL_UNIFORM_BUFFER, UBO_lights);
	if (reallocate_ubo)
	{
		// Calculate size of the buffer
		size_t bufferSize = sizeof(LightProperties) * scene->lights.size();

		// Allocate memory for the buffer
		glBufferData(GL_UNIFORM_BUFFER, bufferSize, NULL, GL_STATIC_DRAW);
	}

	// Bind the buffer to a binding point
	GLuint bindingPoint_Lights = 0; // Choose a binding point
	glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint_Lights, UBO_lights);

	// Map the buffer to client memory
	LightProperties* lightData = (LightProperties*)glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);

	// Populate the buffer with light properties
	for (UINT i = 0; i < scene->lights.size(); i++)
	{
		scene->lights[i]->updateDirCameraSpace(scene->GetActiveCamera()->cTransform);
		scene->lights[i]->updatePosCameraSpace(scene->GetActiveCamera()->cTransform);

		lightData[i].position	= vec4(scene->lights[i]->getPositionCameraSpace() , 1);
		lightData[i].dir		= vec4(scene->lights[i]->getDirectionCameraSpace(), 1);
		lightData[i].color		= vec4(scene->lights[i]->getColor()				  , 1);
		lightData[i].La			= scene->lights[i]->La;
		lightData[i].Ld			= scene->lights[i]->Ld;
		lightData[i].Ls			= scene->lights[i]->Ls;
		lightData[i].type		= scene->lights[i]->getLightType();
	}

	// Unmap the buffer to actually write into the GPU.
	glUnmapBuffer(GL_UNIFORM_BUFFER);
}

void Renderer::InitOpenGLRendering()
{
	program = InitShader("vshader.glsl", "fshader.glsl");

	//Enable Z-Buffer
	glEnable(GL_DEPTH_TEST);

	
	// Create and bind a uniform buffer object
	glGenBuffers(1, &UBO_lights);
	glBindBuffer(GL_UNIFORM_BUFFER, UBO_lights);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(LightProperties), NULL, GL_STATIC_DRAW);

}