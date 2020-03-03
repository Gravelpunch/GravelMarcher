// Include standard headers
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <Windows.h>

#include <vector>

#include <iostream>
using namespace std;

#include <chrono>
using namespace std::chrono;

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>
GLFWwindow* window;

// Include GLM
#include <glm.hpp>
using namespace glm;
#include <gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/string_cast.hpp>
#include <gtx/quaternion.hpp>

GLuint loadShaderProgram(const char * vertex_file_path, const char * fragment_file_path);
template <typename T>
GLuint bufferVertexData(T data[], unsigned int dataSize);
GLuint compileShader(const char * shaderPath, GLenum shaderType);
//GLuint createEmptyTexture(unsigned short texWidth, unsigned short texHeight);
GLuint loadComputeShaderProgram(const char * computeFilePath);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

//---------------------------------Mouse motion variables--------------------------------------

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
double oldMouseXPos = 0.0;
double oldMouseYPos = 0.0;
float cameraRotSpeed = -0.001f;
float cameraYaw = 0.0f;
float cameraPitch = 0.0f;

//--------------------------------Key motion variables---------------------------------------

vec3 cameraPos = vec3(0, 0, 0);
float cameraSpeed = 17.0f/3.0f;
bool wDown = false;
bool aDown = false;
bool sDown = false;
bool dDown = false;
bool shiftDown = false;
bool spaceDown = false;

const unsigned short width = 1920;
const unsigned short height = 1200;

const float verticalFieldOfView = 45.0f;

//------------------------------------World Variables------------------------------------

//How long, in seconds, the 'day' is (the period the sun takes to revolve a full 360 degrees)
const float dayLength = 120;

//The maximum angular elevation, in degrees, the sun achieves in a day.
const float sunMaxElevation = 50;

//The color of the sky at night
const vec3 skyColorNight = vec3(0, 0.05, 0.1);

//The color of the sky at day
const vec3 skyColorDay = vec3(0.7, 0.8, 1);

//A constant that determines how quickly the sky color changes between its night and day color.
//A higher value will mean faster, a lower value will mean slower.
const float skyColorLogB = 5.0f;

int main(void)
{
	// Initialise GLFW
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		getchar();
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 0);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow(width, height, "GravelMarcher", NULL, NULL);
	if (window == NULL) {
		fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n");
		getchar();
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	glfwSetCursorPosCallback(window, cursorPosCallback);

	// Initialize GLEW
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		getchar();
		glfwTerminate();
		return -1;
	}

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

	//Sets up key callback
	glfwSetKeyCallback(window, keyCallback);

	// Dark blue background
	glClearColor(0.0f, 1.0f, 0.0f, 0.0f);

	//Sets up the VAO
	GLuint vertexArrayID;
	glGenVertexArrays(1, &vertexArrayID);
	glBindVertexArray(vertexArrayID);

	//Sets up the shader program
	GLuint shaderProgramID = loadShaderProgram("VertexMarcher.glsl", "FragmentMarcher.glsl");

	//The screen-space coordinates that make up the quad
	float quadVertices[] = {
		-1.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
	};
	GLuint verticesBufferID = bufferVertexData(quadVertices, sizeof(quadVertices));

	//The UVs of the quad
	float quadUVs[] = {
		0, 0,
		1, 0,
		0, 1,
		1, 1
	};
	GLuint uvsBufferID = bufferVertexData(quadUVs, sizeof(quadUVs));

	//The ray directions of the quad
	float screenTop = tan(radians(verticalFieldOfView / 2));
	float aspectRatio = ((float)width) / ((float)height);
	float screenRight = screenTop * aspectRatio;
	float quadRayDirections[] = {
		-screenRight, -screenTop, -1,
		screenRight, -screenTop, -1,
		-screenRight, screenTop, -1,
		screenRight, screenTop, -1,
	};
	GLuint rayDirectionBufferID = bufferVertexData(quadRayDirections, sizeof(quadRayDirections));

	//The triangles that make up the quad
	unsigned short quadIndices[] = {
		0, 3, 1,
		0, 3, 2
	};
	GLuint indicesBufferID;
	glGenBuffers(1, &indicesBufferID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesBufferID);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);	

	//------------------------Uniform handles-------------------------

	GLuint matCameraToWorldID = glGetUniformLocation(shaderProgramID, "matCameraToWorld");
	GLuint sunDirectionID = glGetUniformLocation(shaderProgramID, "sunDirection");
	GLuint skyColorID = glGetUniformLocation(shaderProgramID, "skyColor");

	//-----------------------------------Sets up the actual world-----------------------------------
	vec3 sunRevolutionAxis = vec3(0, sinf(radians(90 - sunMaxElevation)), cosf(radians(90 - sunMaxElevation)));

	double lastTime = glfwGetTime();
	double startTime = glfwGetTime();
	do {

		//Finds delta time

		double currentTime = glfwGetTime();
		float deltaTime = float(currentTime - lastTime);
		lastTime = currentTime;
		float timeSinceStart = float(currentTime - startTime);

		//Finds the camera rotation
		
		mat4 matCameraYaw = rotate(mat4(1.0f), cameraYaw, vec3(0, 1, 0));
		mat4 matCameraPitch = rotate(mat4(1.0f), cameraPitch, vec3(1, 0, 0));

		//Finds the camera translation

		if (wDown && !sDown) {
			cameraPos -= vec3(matCameraYaw * matCameraPitch * vec4(0, 0, 1, 0)) * cameraSpeed * deltaTime;
		}
		else if (!wDown && sDown) {
			cameraPos += vec3(matCameraYaw * matCameraPitch * vec4(0, 0, 1, 0)) * cameraSpeed * deltaTime;
		}

		if (aDown && !dDown) {
			cameraPos -= vec3(matCameraYaw * matCameraPitch * vec4(1, 0, 0, 0)) * cameraSpeed * deltaTime;
		}
		else if (!aDown && dDown) {
			cameraPos += vec3(matCameraYaw * matCameraPitch * vec4(1, 0, 0, 0)) * cameraSpeed * deltaTime;
		}

		if (shiftDown && !spaceDown) {
			cameraPos -= vec3(0, 1, 0) * cameraSpeed * deltaTime;
		}
		else if (!shiftDown && spaceDown) {
			cameraPos += vec3(0, 1, 0) * cameraSpeed * deltaTime;
		}

		mat4 matCameraTranslation = translate(mat4(1.0f), cameraPos);

		//Unifies the camera transform
		mat4 matCameraToWorld = matCameraTranslation * matCameraYaw * matCameraPitch;

		//Finds the sun direction and sky color.
		float daysSinceStart = timeSinceStart / dayLength;
		vec3 skyColor = mix(skyColorNight, skyColorDay, 1.0f / (1 + exp(-skyColorLogB * sin(2 * pi<float>() * daysSinceStart))));
		vec3 sunDirection = vec3(rotate(mat4(1.0f), 2 * pi<float>() * daysSinceStart, sunRevolutionAxis) * vec4(1, 0, 0, 0));

		
		//--------------------Uniform handoffs-------------------------------
		glUniformMatrix4fv(matCameraToWorldID, 1, FALSE, &matCameraToWorld[0][0]);
		glUniform3fv(sunDirectionID, 1, &sunDirection[0]);
		glUniform3fv(skyColorID, 1, &skyColor[0]);
		
		//Run the shaders
		glDispatchCompute(width, height, 1);

		//Don't continue until the ray-marcher has finished
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		
		//Switch to the image-drawing shader
		glUseProgram(shaderProgramID);
		//Send the vertex position data to the shader program
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, verticesBufferID);
		glVertexAttribPointer(
			0,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0
		);

		//Send the uv data to the shader program
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, uvsBufferID);
		glVertexAttribPointer(
			1,
			2,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0
		);

		//Send the ray direction data to the shader program
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, rayDirectionBufferID);
		glVertexAttribPointer(
			2,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0
		);
		
		//Draw the full screen quad
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesBufferID);
		glDrawElements(
			GL_TRIANGLES,
			6,
			GL_UNSIGNED_SHORT,
			(void*)0
		);

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
		glfwWindowShouldClose(window) == 0);

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
	double deltaX = xpos - oldMouseXPos;
	cameraYaw += deltaX * cameraRotSpeed;
	oldMouseXPos = xpos;

	double deltaY = ypos - oldMouseYPos;
	cameraPitch += deltaY * cameraRotSpeed;
	oldMouseYPos = ypos;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {

	//The W key
	if (key == GLFW_KEY_W) {
		if (action == GLFW_PRESS) {
			wDown = true;
		}
		else if (action == GLFW_RELEASE) {
			wDown = false;
		}
	}

	//The A key
	if (key == GLFW_KEY_A) {
		if (action == GLFW_PRESS) {
			aDown = true;
		}
		else if (action == GLFW_RELEASE) {
			aDown = false;
		}
	}

	//The S key
	if (key == GLFW_KEY_S) {
		if (action == GLFW_PRESS) {
			sDown = true;
		}
		else if (action == GLFW_RELEASE) {
			sDown = false;
		}
	}

	//The D key
	if (key == GLFW_KEY_D) {
		if (action == GLFW_PRESS) {
			dDown = true;
		}
		else if (action == GLFW_RELEASE) {
		dDown = false;
		}
	}

	//The Shift key
	if (key == GLFW_KEY_LEFT_SHIFT) {
		if (action == GLFW_PRESS) {
			shiftDown = true;
		}
		else if (action == GLFW_RELEASE) {
			shiftDown = false;
		}
	}

	//The Space key
	if (key == GLFW_KEY_SPACE) {
		if (action == GLFW_PRESS) {
			spaceDown = true;
		}
		else if (action == GLFW_RELEASE) {
			spaceDown = false;
		}
	}
}

template <typename T>
GLuint bufferVertexData(T data[], unsigned int dataSize) {
	GLuint returnID;
	glGenBuffers(1, &returnID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, returnID);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, dataSize, data, GL_STATIC_DRAW);

	return returnID;
}

#include <fstream>
#include <algorithm>
#include <sstream>
#include <vector>

GLuint loadShaderProgram(const char * vertex_file_path, const char * fragment_file_path) {

	// Create the shaders
	GLuint VertexShaderID = compileShader(vertex_file_path, GL_VERTEX_SHADER);
	GLuint FragmentShaderID = compileShader(fragment_file_path, GL_FRAGMENT_SHADER);

	// Link the program
	printf("Linking program\n");
	GLuint ProgramID = glCreateProgram();
	glAttachShader(ProgramID, VertexShaderID);
	glAttachShader(ProgramID, FragmentShaderID);
	glLinkProgram(ProgramID);

	GLint Result = GL_FALSE;
	int InfoLogLength;

	// Check the program
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (InfoLogLength > 0) {
		std::vector<char> ProgramErrorMessage(InfoLogLength + 1);
		glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
		printf("%s\n", &ProgramErrorMessage[0]);
	}

	glDetachShader(ProgramID, VertexShaderID);
	glDetachShader(ProgramID, FragmentShaderID);

	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);

	return ProgramID;
}

GLuint compileShader(const char * shaderPath, GLenum shaderType) {
	GLuint shaderID = glCreateShader(shaderType);
	// Read the Vertex Shader code from the file
	std::string shaderCode;
	std::ifstream shaderStream(shaderPath, std::ios::in);
	if (shaderStream.is_open()) {
		std::stringstream sstr;
		sstr << shaderStream.rdbuf();
		shaderCode = sstr.str();
		shaderStream.close();
	}
	else {
		printf("Impossible to open %s. Remember the path origin is the same folder as the EXE!", shaderPath);
		getchar();
		return 0;
	}

	// Compile Vertex Shader
	printf("Compiling shader : %s\n", shaderPath);
	char const * shaderSourcePointer = shaderCode.c_str();
	glShaderSource(shaderID, 1, &shaderSourcePointer, NULL);
	glCompileShader(shaderID);

	GLint Result = GL_FALSE;
	int InfoLogLength;

	// Check Vertex Shader
	glGetShaderiv(shaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (InfoLogLength > 0) {
		std::vector<char> VertexShaderErrorMessage(InfoLogLength + 1);
		glGetShaderInfoLog(shaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
		printf("%s\n", &VertexShaderErrorMessage[0]);
	}

	return shaderID;
}

GLuint loadComputeShaderProgram(const char * computeFilePath) {
	GLuint computeShaderID = compileShader(computeFilePath, GL_COMPUTE_SHADER);

	GLuint programID = glCreateProgram();
	glAttachShader(programID, computeShaderID);
	glLinkProgram(programID);

	GLint result = GL_FALSE;
	int infoLogLength;

	glGetProgramiv(programID, GL_LINK_STATUS, &result);
	glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &infoLogLength);
	if (infoLogLength > 0) {
		std::vector<char> programErrorMessage(infoLogLength + 1);
		glGetProgramInfoLog(programID, infoLogLength, NULL, &programErrorMessage[0]);
		printf("%s\n", &programErrorMessage[0]);
	}

	glDetachShader(programID, computeShaderID);
	glDeleteShader(computeShaderID);

	return programID;
}