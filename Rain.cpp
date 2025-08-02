// Header files
#include <windows.h>
#include <mmsystem.h>		// for PlaySound() api
#include <stdio.h>			// for file I/O operations
#include <stdlib.h>			// for exit()
#include <vector>			// for std::vector
#include <map>
#include <random>			// for random points generation
#include <cmath>			
#include <string>			// for string
#include <sstream>			// for string operations
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"		
#include "OGL.h"

// OpenGL header files
#include <GL/glew.h>	
#include <GL/gl.h>
#include "vmath.h"
using namespace vmath;

// freetype
#include <ft2build.h>
#include FT_FREETYPE_H 

// OpenCL header files
#define CL_TARGET_OPENCL_VERSION 300  // Need to specify OpenCL version to avoid mismatch
#include <CL/opencl.h>

#define WIN_WIDTH 800
#define WIN_HEIGHT 600

// libraries
#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "OpenGL32.lib")
#pragma comment(lib, "OpenCL.lib")
#pragma comment(lib, "freetype.lib")
#pragma comment(lib, "winmm.lib")

// Global Function declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Global variables declarations
HWND ghwnd = NULL;
HDC ghdc = NULL;
HGLRC ghrc = NULL;
BOOL gbFullScreen = FALSE;
BOOL gbActiveWindow = TRUE;
FILE* gpFile = NULL;

// Programmable Pipeline related global variables
GLuint shaderProgramObject;
GLuint shaderProgramObjectFont;
GLuint shaderProgramObjectIntroOutro;

enum
{
	AMC_ATTRIBUTE_POSITION = 0,
	AMC_ATTRIBUTE_COLOR,
	AMC_ATTRIBUTE_NORMAL,
	AMC_ATTRIBUTE_TEXTURE0,
	AMC_ATTRIBUTE_SEED,
	AMC_ATTRIBUTE_VELOCITY
};

// to capture width and height from resize
int gWidth;
int gHeight;

// Rain related variables
constexpr auto max_width = 4096;
constexpr auto max_height = 4096;

float raindropVelocities[max_width][max_height][4];
float raindropPositions[max_width][max_height][4];
float raindropSeed[max_width][max_height][4];

int meshWidth = 256;
int meshHeight = 256;

const float gravity = 9.81f;
float timeStep = 0.007f;
float clusterScale = 25.0f;
float veloFactor = 1.0f;
float dt;
static int count = 0;

vmath::vec3 eyePos = vmath::vec3(0.0f, 0.0f, 0.0f);
std::default_random_engine r;
std::uniform_real_distribution<float> uniformDist(-1.0f, 1.0f);

// vaos and vbos
GLuint positionBuffer, velocityBuffer, seedBuffer;
GLuint vao;

GLuint rainTexArray;

// wind
std::random_device rds;
std::mt19937 rs(rds());

GLfloat windDir[10];

mat4 perspectiveProjectionMatrix;
mat4 orthographicProjectionMatrix;

cl_int oclResult;
cl_mem graphicsResource_position = NULL;			// cl_mem is internally void *
cl_mem graphicsResource_velocity = NULL;			// cl_mem is internally void *
cl_mem graphicsResource_seed = NULL;				// cl_mem is internally void *
cl_context oclContext;
cl_command_queue oclCommandQueue;
cl_program oclProgram;
cl_kernel oclKernel;

BOOL onGPU = FALSE;
BOOL wind = FALSE;

// text rendering related variables
struct Character
{
	unsigned int TextureID; // ID handle of the glyph texture
	vmath::ivec2 Size;      // Size of glyph
	vmath::ivec2 Bearing;   // Offset from baseline to left/top of glyph
	unsigned int Advance;   // Horizontal offset to advance to next glyph
};

std::map<GLchar, Character> Characters;
unsigned int VAO, VBO;
unsigned int texture;

double framesPerSec = 0.0;

// intro and outro 
// textures
GLuint texture_amc;
GLuint texture_title;
GLuint texture_ankit;
GLuint texture_references;
GLuint texture_specialthanks;
GLuint texture_ignited;
GLuint texture_thankyou;

// vao and vbos
GLuint vao_intro;
GLuint vbo_position;
GLuint vbo_texture;

// fade in / fade out
float fadeduration = 0.0f;
float fadeoutduration = 1.0f;
int texturecounter = 0;

int sceneCounter = 0;

GLfloat tx, ty, tz;
GLfloat sx, sy, sz;
GLfloat tstepsize = 0.1f;

// Entry Point function
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int iCmdShow)
{
	// Function declarations
	// This sequence becuase they are called in same order
	int initialize(void);
	void display(void);
	void update(void);
	void uninitialize(void);
	void ToggleFullScreen(void);

	// Variable declarations
	WNDCLASSEX wndclass;
	MSG msg;
	TCHAR szAppName[] = TEXT("MyWindow");
	HWND hwnd;

	int cxScreen, cyScreen;
	BOOL bDone = FALSE;
	int iRetVal = 0;

	LARGE_INTEGER StartingTime, EndingTime;
	LARGE_INTEGER Frequency;
	double delta = 0.0f;

	// Code
	if (fopen_s(&gpFile, "Log.txt", "w") != 0)
	{
		MessageBox(NULL, TEXT("Creation of Log file failed. Exitting..."), TEXT("File I/O Error"), MB_OK);
		exit(0);
	}
	else
	{
		fprintf(gpFile, "INFO: Log file is successfully Created.\n");
	}

	//Initializing members of WNDCLASSEX
	wndclass.cbSize = sizeof(WNDCLASSEX);
	wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wndclass.lpfnWndProc = WndProc;
	wndclass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));
	wndclass.hInstance = hInstance;
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));
	wndclass.lpszClassName = szAppName;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndclass.lpszMenuName = 0;

	// Register our class
	RegisterClassEx(&wndclass);

	//Get Monitor width and height
	cxScreen = GetSystemMetrics(SM_CXSCREEN);
	cyScreen = GetSystemMetrics(SM_CYSCREEN);

	cxScreen = cxScreen - WIN_WIDTH;
	cyScreen = cyScreen - WIN_HEIGHT;

	// Create Window in the memory
	hwnd = CreateWindowEx(WS_EX_APPWINDOW,
		szAppName,
		TEXT("Rain Particles Rendering Using OpenGL-OpenCL InterOP"),
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
		cxScreen / 2,
		cyScreen / 2,
		WIN_WIDTH,
		WIN_HEIGHT,
		NULL,
		NULL,
		hInstance,
		NULL);

	ghwnd = hwnd;

	// Initialize
	iRetVal = initialize();
	if (iRetVal == -1)
	{
		fprintf(gpFile, "ERROR: ChoosePixelFormat() failed.\n");
		uninitialize();
	}
	else if (iRetVal == -2)
	{
		fprintf(gpFile, "ERROR: SetPixelFormat() failed.\n");
		uninitialize();
	}
	else if (iRetVal == -3)
	{
		fprintf(gpFile, "ERROR: Creating OpenGL wglCreateContext() failed.\n");
		uninitialize();
	}
	else if (iRetVal == -4)
	{
		fprintf(gpFile, "ERROR: Making OpenGL context as current context wglMakeCurrent() failed.\n");
		uninitialize();
	}
	else if (iRetVal == -5)
	{
		fprintf(gpFile, "ERROR: Initialization of glewInit() failed.\n");
		uninitialize();
	}
	else
	{
		fprintf(gpFile, "INFO: Initialization success !!.\n");
	}
	//Show the Window
	ShowWindow(hwnd, iCmdShow);

	//Foregrounding and focusing the window
	SetForegroundWindow(hwnd);		// Bring the window on top in z order

	SetFocus(hwnd);					// Set the focus on our window

	ToggleFullScreen();

	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&StartingTime);

	double freq = 1.0 / (double)Frequency.QuadPart;
	double timePerFrame = 1.0 / 61;

	// Game Loop
	while (bDone == FALSE)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				bDone = TRUE;
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			if (gbActiveWindow == TRUE)
			{
				// Render the scene.
				display();

				//Update the scene
				update();

				QueryPerformanceCounter(&EndingTime);
				delta = (double)(EndingTime.QuadPart - StartingTime.QuadPart) * freq;

				while (delta < timePerFrame)
				{
					QueryPerformanceCounter(&EndingTime);
					delta = (double)(EndingTime.QuadPart - StartingTime.QuadPart) * freq;
				}

				StartingTime = EndingTime;
				framesPerSec = 1 / delta;

			}
		}
	}

	uninitialize();
	return ((int)msg.wParam);

}

//Callback function definition
LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	// Function declarations
	void ToggleFullScreen(void);
	void resize(int, int);
	void uninitialize(void);

	// Code
	switch (iMsg)
	{

	case WM_SETFOCUS:
		gbActiveWindow = TRUE;
		break;
	case WM_KILLFOCUS:
		gbActiveWindow = FALSE;
		break;
	case WM_ERASEBKGND:

		return 0; // Tell os not to eraseB
	case WM_CHAR:
		fprintf(gpFile, "INFO: WM_CHAR has arrived.\n");
		switch (wParam)
		{
		case 'C':
		case 'c':
			onGPU = FALSE;
			break;
		case 'G':
		case 'g':
			onGPU = TRUE;
			break;
		case 'E':
		case 'e':
			sceneCounter = 2;
			break;
		case 'F':
		case 'f':
			ToggleFullScreen();
			break;
		
		case 'x':
			tx = tx + tstepsize;
			break;
		case 'X':
			tx = tx - tstepsize;
			break;

		case 'y':
			ty = ty + tstepsize;
			break;
		case 'Y':
			ty = ty - tstepsize;
			break;

		case 'z':
			tz = tz + tstepsize;
			break;
		case 'Z':
			tz = tz - tstepsize;
			break;

		case 's':
			tstepsize = tstepsize + 0.1;
			break;
		case 'S':
			tstepsize = tstepsize - 0.1;
			break;
		
		case 'w':
		case 'W':
			if (wind == FALSE)
				wind = TRUE;
			else
				wind = FALSE;
			break;

		default:
			break;
		}
		break;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case 27: // Hexadecimal value of ESC key
			DestroyWindow(hwnd);
			break;
		case 49: // 1

			meshWidth = 512;
			meshHeight = 512;
			timeStep = 0.005f;

			glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
			glBufferData(GL_ARRAY_BUFFER, meshWidth * meshHeight * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);

			glBindBuffer(GL_ARRAY_BUFFER, velocityBuffer);
			glBufferData(GL_ARRAY_BUFFER, meshWidth * meshHeight * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);

			graphicsResource_position = clCreateFromGLBuffer(oclContext, CL_MEM_WRITE_ONLY, positionBuffer, &oclResult);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clCreateFromGLBuffer() failed for position buffer.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}

			graphicsResource_velocity = clCreateFromGLBuffer(oclContext, CL_MEM_WRITE_ONLY, velocityBuffer, &oclResult);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clCreateFromGLBuffer() failed for velocity buffer.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}
			
			break;
		case 50: //2 
			meshWidth = 1024;
			meshHeight = 1024;

			timeStep = 0.005f;

			glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
			glBufferData(GL_ARRAY_BUFFER, meshWidth * meshHeight * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);

			glBindBuffer(GL_ARRAY_BUFFER, velocityBuffer);
			glBindBuffer(GL_ARRAY_BUFFER, velocityBuffer);
			glBufferData(GL_ARRAY_BUFFER, meshWidth * meshHeight * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);

			graphicsResource_position = clCreateFromGLBuffer(oclContext, CL_MEM_WRITE_ONLY, positionBuffer, &oclResult);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clCreateFromGLBuffer() failed for position buffer.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}

			graphicsResource_velocity = clCreateFromGLBuffer(oclContext, CL_MEM_WRITE_ONLY, velocityBuffer, &oclResult);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clCreateFromGLBuffer() failed for velocity buffer.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}
			break;

		case 51: // 3
			meshWidth = 2048;
			meshHeight = 2048;

			timeStep = 0.01f;

			glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
			glBufferData(GL_ARRAY_BUFFER, meshWidth * meshHeight * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);

			glBindBuffer(GL_ARRAY_BUFFER, velocityBuffer);
			glBindBuffer(GL_ARRAY_BUFFER, velocityBuffer);
			glBufferData(GL_ARRAY_BUFFER, meshWidth * meshHeight * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);

			graphicsResource_position = clCreateFromGLBuffer(oclContext, CL_MEM_WRITE_ONLY, positionBuffer, &oclResult);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clCreateFromGLBuffer() failed for position buffer.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}

			graphicsResource_velocity = clCreateFromGLBuffer(oclContext, CL_MEM_WRITE_ONLY, velocityBuffer, &oclResult);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clCreateFromGLBuffer() failed for velocity buffer.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}
			break;
		case 52: // 4
			meshWidth = 4096;
			meshHeight = 4096;

			timeStep = 0.01f;

			glBindVertexArray(vao);
			glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
			glBufferData(GL_ARRAY_BUFFER, meshWidth * meshHeight * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);

			glBindBuffer(GL_ARRAY_BUFFER, velocityBuffer);
			glBufferData(GL_ARRAY_BUFFER, meshWidth * meshHeight * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);

			glBindVertexArray(0);

			graphicsResource_position = clCreateFromGLBuffer(oclContext, CL_MEM_WRITE_ONLY, positionBuffer, &oclResult);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clCreateFromGLBuffer() failed for position buffer.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}

			graphicsResource_velocity = clCreateFromGLBuffer(oclContext, CL_MEM_WRITE_ONLY, velocityBuffer, &oclResult);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clCreateFromGLBuffer() failed for velocity buffer.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}
			break;		

		default:
			break;
		}
		break;
	case WM_SIZE:
		resize(LOWORD(lParam), HIWORD(lParam)); // width and height, LOWORD of lParam is width HIWORD of lParam is Height
		break;
	case WM_CLOSE:								//First WM_CLOSE the WM_DESTROY
		DestroyWindow(hwnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		break;
	}
	return (DefWindowProc(hwnd, iMsg, wParam, lParam));
}

// Full screen function implementation
void ToggleFullScreen(void)
{
	// Variable Declarations
	static DWORD dwStyle;
	static WINDOWPLACEMENT wp;
	MONITORINFO mi;

	// Code
	wp.length = sizeof(WINDOWPLACEMENT);

	if (gbFullScreen == FALSE)
	{
		dwStyle = GetWindowLong(ghwnd, GWL_STYLE);
		if (dwStyle & WS_OVERLAPPEDWINDOW)
		{
			mi.cbSize = sizeof(MONITORINFO);

			if (GetWindowPlacement(ghwnd, &wp) && GetMonitorInfo(MonitorFromWindow(ghwnd, MONITORINFOF_PRIMARY), &mi))
			{
				SetWindowLong(ghwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
				SetWindowPos(ghwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOZORDER | SWP_FRAMECHANGED);
			}
			ShowCursor(TRUE);
			gbFullScreen = TRUE;
		}
	}
	else
	{
		SetWindowLong(ghwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(ghwnd, &wp);
		SetWindowPos(ghwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);
		ShowCursor(TRUE);
		gbFullScreen = FALSE;
	}
}

int initializeFontRendering(void)
{
	// Function declarations
	void uninitialize(void);
	void resize(int, int);

	GLint status;
	GLint infoLogLength;
	char* log = NULL;

	// Vertex Shader
	const GLchar* vertexShaderSourceCode =
		"#version 460 core\n"\
		"layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>\n"\
		"out vec2 TexCoords;\n"\
		"\n"\
		"uniform mat4 projection;\n"\
		"\n"\
		"void main()\n"\
		"{\n"\
		"    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);\n"\
		"    TexCoords = vertex.zw;\n"\
		"}";

	// Creating shader obejct
	GLuint vertexShaderObject = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShaderObject, 1, (const GLchar**)&vertexShaderSourceCode, NULL);

	glCompileShader(vertexShaderObject);

	glGetShaderiv(vertexShaderObject, GL_COMPILE_STATUS, &status);

	if (status == GL_FALSE)
	{
		// Getting the length of log of compilation status
		glGetShaderiv(vertexShaderObject, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 0)
		{
			// Allocate enough memory to the buffer to hold the compilation log
			log = (char*)malloc(infoLogLength);
			if (log != NULL)
			{
				GLsizei written;

				// Get the compilation log into the allocated buffer
				glGetShaderInfoLog(vertexShaderObject, infoLogLength, &written, log);

				// Display the contents of log
				fprintf(gpFile, "Vertex Shader Compilation Log: %s\n", log);

				// Free the allocated buffer
				free(log);

				// Exit the application due to error
				uninitialize();
			}
		}
	}

	// Fragment Shader
	const GLchar* fragmentShaderSourceCode =
		"#version 460 core\n"\
		"in vec2 TexCoords;\n"\
		"out vec4 color;\n"\
		"\n"\
		"uniform sampler2D text;\n"\
		"uniform vec3 textColor;\n"\
		"\n"\
		"void main()\n"\
		"{    \n"\
		"    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);\n"\
		"    color = vec4(textColor, 1.0) * sampled;\n"\
		"}";

	GLuint fragmentShaderObject = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShaderObject, 1, (const GLchar**)&fragmentShaderSourceCode, NULL);
	glCompileShader(fragmentShaderObject);

	status = 0;
	infoLogLength = 0;
	log = NULL;

	glGetShaderiv(fragmentShaderObject, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		glGetShaderiv(fragmentShaderObject, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 0)
		{
			log = (char*)malloc(infoLogLength);
			if (log != NULL)
			{
				GLsizei written;

				glGetShaderInfoLog(fragmentShaderObject, infoLogLength, &written, log);
				fprintf(gpFile, "Fragment Shader Compilation Log : %s\n", log);
				free(log);
				uninitialize();
			}
		}
	}

	// Shader program object
	shaderProgramObjectFont = glCreateProgram();

	// Attach desired
	glAttachShader(shaderProgramObjectFont, vertexShaderObject);
	glAttachShader(shaderProgramObjectFont, fragmentShaderObject);

	// Link the shader program object
	glLinkProgram(shaderProgramObjectFont);

	status = 0;
	infoLogLength = 0;
	log = NULL;

	glGetProgramiv(shaderProgramObjectFont, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		glGetProgramiv(shaderProgramObjectFont, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 0)
		{
			log = (char*)malloc(infoLogLength);
			if (log != NULL)
			{
				GLsizei written;

				glGetProgramInfoLog(shaderProgramObjectFont, infoLogLength, &written, log);
				fprintf(gpFile, "Shader Program Link Log: %s\n", log);
				free(log);
				uninitialize();
			}
		}
	}

	glEnable(GL_TEXTURE_2D);

	orthographicProjectionMatrix = mat4::identity();

	FT_Library ft;
	if (FT_Init_FreeType(&ft))
	{
		fprintf(gpFile, "ERROR::FREETYPE: Could not init FreeType Library.\n");
		return -1;
	}

	FT_Face face;
	if (FT_New_Face(ft, "fonts\\BitterPro-Regular.ttf", 0, &face))
	{
		fprintf(gpFile, "ERROR::FREETYPE: Failed to load font.\n");
		return -1;
	}
	// set size to load glyphs as
	FT_Set_Pixel_Sizes(face, 0, 48);

	// disable byte-alignment restriction
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// load first 128 characters of ASCII set
	for (unsigned char c = 0; c < 128; c++)
	{
		// Load character glyph 
		if (FT_Load_Char(face, c, FT_LOAD_RENDER))
		{
			fprintf(gpFile, "ERROR::FREETYTPE: Failed to load Glyph\n");
			continue;
		}
		// generate texture
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RED,
			face->glyph->bitmap.width,
			face->glyph->bitmap.rows,
			0,
			GL_RED,
			GL_UNSIGNED_BYTE,
			face->glyph->bitmap.buffer
		);
		// set texture options
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// now store character for later use
		Character character =
		{
			texture,
			vmath::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
			vmath::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
			static_cast<unsigned int>(face->glyph->advance.x)
		};
		Characters.insert(std::pair<char, Character>(c, character));
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	// destroy FreeType once we're finished
	FT_Done_Face(face);
	FT_Done_FreeType(ft);

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	resize(WIN_WIDTH, WIN_HEIGHT);
	return(0);
}

int initializeIntroOutro(void)
{
	// Function declarations
	void uninitialize(void);
	BOOL loadGLTexture_stbi(GLuint*, char*);
	void resize(int, int);

	// Variable declarations
	GLint status;
	GLint infoLogLength;
	char* log = NULL;

	// Vertex Shader
	const GLchar* vertexShaderSourceCode =
		"#version 460 core" \
		"\n" \
		"in vec4 a_position;" \
		"in vec2 a_texcoord;" \
		"uniform mat4 u_modelMatrix;" \
		"uniform mat4 u_viewMatrix;" \
		"uniform mat4 u_projectionMatrix;" \
		"out vec2 a_texcoord_out;" \
		"void main(void)" \
		"{" \
		"gl_Position = u_projectionMatrix * u_viewMatrix * u_modelMatrix * a_position;" \
		"a_texcoord_out = a_texcoord;" \
		"}";

	// Creating shader obejct
	GLuint vertexShaderObject = glCreateShader(GL_VERTEX_SHADER);

	glShaderSource(vertexShaderObject, 1, (const GLchar**)&vertexShaderSourceCode, NULL);
	glCompileShader(vertexShaderObject);

	glGetShaderiv(vertexShaderObject, GL_COMPILE_STATUS, &status);

	if (status == GL_FALSE)
	{
		// Getting the length of log of compilation status
		glGetShaderiv(vertexShaderObject, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 0)
		{
			// Allocate enough memory to the buffer to hold the compilation log
			log = (char*)malloc(infoLogLength);
			if (log != NULL)
			{
				GLsizei written;

				// Get the compilation log into the allocated buffer
				glGetShaderInfoLog(vertexShaderObject, infoLogLength, &written, log);

				// Display the contents of log
				fprintf(gpFile, "Vertex Shader Compilation Log: %s\n", log);
				free(log);
				uninitialize();
			}
		}
	}

	// Fragment Shader
	const GLchar* fragmentShaderSourceCode =
		"#version 460 core" \
		"\n" \
		"in vec2 a_texcoord_out;" \
		"uniform sampler2D u_textureSampler;" \
		"uniform float time;" \
		"out vec4 FragColor;" \
		"void main()\n"\
		"{\n"\
		"    // Sample the texture and apply the alpha value\n"\
		"    vec4 texColor = texture(u_textureSampler, a_texcoord_out);\n"\
		"    FragColor = vec4(texColor.rgb, time);\n"\
		"}";

	GLuint fragmentShaderObject = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShaderObject, 1, (const GLchar**)&fragmentShaderSourceCode, NULL);
	glCompileShader(fragmentShaderObject);

	status = 0;
	infoLogLength = 0;
	log = NULL;

	glGetShaderiv(fragmentShaderObject, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		glGetShaderiv(fragmentShaderObject, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 0)
		{
			log = (char*)malloc(infoLogLength);
			if (log != NULL)
			{
				GLsizei written;

				glGetShaderInfoLog(fragmentShaderObject, infoLogLength, &written, log);
				fprintf(gpFile, "Fragment Shader Compilation Log : %s\n", log);
				free(log);
				uninitialize();
			}
		}
	}

	// Shader program object
	shaderProgramObjectIntroOutro = glCreateProgram();

	// Attach desired
	glAttachShader(shaderProgramObjectIntroOutro, vertexShaderObject);
	glAttachShader(shaderProgramObjectIntroOutro, fragmentShaderObject);

	// Pre-Linking
	glBindAttribLocation(shaderProgramObjectIntroOutro, AMC_ATTRIBUTE_POSITION, "a_position"); // Be AWARE
	glBindAttribLocation(shaderProgramObjectIntroOutro, AMC_ATTRIBUTE_TEXTURE0, "a_texcoord");

	// Link the shader program object
	glLinkProgram(shaderProgramObjectIntroOutro);

	status = 0;
	infoLogLength = 0;
	log = NULL;

	glGetProgramiv(shaderProgramObjectIntroOutro, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		glGetProgramiv(shaderProgramObjectIntroOutro, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 0)
		{
			log = (char*)malloc(infoLogLength);
			if (log != NULL)
			{
				GLsizei written;

				glGetProgramInfoLog(shaderProgramObjectIntroOutro, infoLogLength, &written, log);
				fprintf(gpFile, "Shader Program Link Log: %s\n", log);
				free(log);
				uninitialize();
			}
		}
	}

	// Declaration of vertex arrays
	const GLfloat position[] =
	{
		1.0f, 1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f
	};

	const GLfloat texcoord[] =
	{
		1.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f,
	};

	// VAO and VBO related code
	glGenVertexArrays(1, &vao_intro);
	glBindVertexArray(vao_intro);

	// VBO for position
	glGenBuffers(1, &vbo_position);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_position);
	glBufferData(GL_ARRAY_BUFFER, sizeof(position), position, GL_STATIC_DRAW);
	glVertexAttribPointer(AMC_ATTRIBUTE_POSITION, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(AMC_ATTRIBUTE_POSITION);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// VBO for texture
	glGenBuffers(1, &vbo_texture);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_texture);
	glBufferData(GL_ARRAY_BUFFER, sizeof(texcoord), texcoord, GL_STATIC_DRAW);
	glVertexAttribPointer(AMC_ATTRIBUTE_TEXTURE0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(AMC_ATTRIBUTE_TEXTURE0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	// Depth and Clear color Changes
	if (loadGLTexture_stbi(&texture_amc, "textures\\credits\\1_astromedicomp.png") == FALSE)
	{
		fprintf(gpFile, "ERROR: Loading of credits\\1_astromedicomp.png failed.\n");
		uninitialize();
	}
	if (loadGLTexture_stbi(&texture_title, "textures\\credits\\2_raintitle.png") == FALSE)
	{
		fprintf(gpFile, "ERROR: Loading of credits\\2_raintitle.pnge failed.\n");
		uninitialize();
	}
	if (loadGLTexture_stbi(&texture_ankit, "textures\\credits\\3_presentedby.png") == FALSE)
	{
		fprintf(gpFile, "ERROR: Loading credits\\3_presentedby.png failed.\n");
		uninitialize();
	}
	if (loadGLTexture_stbi(&texture_references, "textures\\credits\\4_references.png") == FALSE)
	{
		fprintf(gpFile, "ERROR: Loading of 4_references failed.\n");
		uninitialize();
	}

	if (loadGLTexture_stbi(&texture_specialthanks, "textures\\credits\\5_special_thanks.png") == FALSE)
	{
		fprintf(gpFile, "ERROR: Loading of blend map texture failed.\n");
		uninitialize();
	}

	if (loadGLTexture_stbi(&texture_ignited, "textures\\credits\\6_ignitedby.png") == FALSE)
	{
		fprintf(gpFile, "ERROR: Loading of credits\\5_ignitedby.png failed.\n");
		uninitialize();
	}
	if (loadGLTexture_stbi(&texture_thankyou, "textures\\credits\\7_thankyou.png") == FALSE)
	{
		fprintf(gpFile, "ERROR: Loading of credits\\5_ignitedby.png failed.\n");
		uninitialize();
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	resize(WIN_WIDTH, WIN_HEIGHT);
	return(0);
}

int initialize(void)
{
	// Function declarations
	void printGLInfo(void);
	void printOpenCLInfo(void);
	void uninitialize(void);
	void resize(int, int);
	BOOL loadTextureArray(GLuint * texture, const char* path, GLsizei noOfTextures);

	// Variable declarations
	PIXELFORMATDESCRIPTOR pfd;
	int iPixelFormatIndex = 0;
	GLint status;
	GLint infoLogLength;
	char* log = NULL;

	TCHAR str[255];

	// Device properties
	cl_platform_id oclPlatformID;
	unsigned int dev_count = 0;
	cl_device_id* oclDeviceIDs = NULL;
	cl_device_id oclDeviceID;

	// Code
	ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));	 //memset((void*)&pfd, NULL, sizeof(PIXELFORMATDESCRIPTOR))
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;									// Support to OPENGL 1.x version only from microsoft.
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cRedBits = 8;
	pfd.cGreenBits = 8;
	pfd.cBlueBits = 8;
	pfd.cAlphaBits = 8;
	pfd.cDepthBits = 32;

	// GetDC
	ghdc = GetDC(ghwnd);

	//Choose pixel format
	iPixelFormatIndex = ChoosePixelFormat(ghdc, &pfd);
	if (iPixelFormatIndex == 0)
		return -1;

	//Set the choosen pixel format
	if (SetPixelFormat(ghdc, iPixelFormatIndex, &pfd) == FALSE)
		return -2;

	// Create Open GL rendering context
	ghrc = wglCreateContext(ghdc);
	if (ghrc == NULL)
		return -3;

	//Make the rendering context as current context
	if (wglMakeCurrent(ghdc, ghrc) == FALSE)
		return -4;

	// GLEW initialization
	if (glewInit() != GLEW_OK)
		return -5;

	// print OpenCL infor
	printOpenCLInfo();

	// 1. OpenCL initialization
	// get platform ID
	oclResult = clGetPlatformIDs(2, &oclPlatformID, NULL);
	if (oclResult != CL_SUCCESS)
	{
		fprintf(gpFile, "ERROR: clGetPlatformIDs() failed.\n");
		uninitialize();
		exit(EXIT_FAILURE);
	}

	// 2. get GPU device ID
	// step 1 - get total GPU device count
	oclResult = clGetDeviceIDs(oclPlatformID, CL_DEVICE_TYPE_GPU, 0, NULL, &dev_count);
	if (oclResult != CL_SUCCESS)
	{
		fprintf(gpFile, "ERROR: clGetDeviceIDs() failed to get device count.\n");
		uninitialize();
		exit(EXIT_FAILURE);
	}
	else if (dev_count == 0)
	{
		fprintf(gpFile, "ERROR: No OpenCL supported device.\n");
		uninitialize();
		exit(EXIT_FAILURE);
	}

	// step 2 - create memory for the array of device IDs for dev count
	oclDeviceIDs = (cl_device_id*)malloc(sizeof(cl_device_id) * dev_count);
	// here there should be error checking for malloc
	// step 3 - get the count in oclDeviceIDs array
	oclResult = clGetDeviceIDs(oclPlatformID, CL_DEVICE_TYPE_GPU, dev_count, oclDeviceIDs, NULL);
	if (oclResult != CL_SUCCESS)
	{
		fprintf(gpFile, "ERROR: clGetDeviceIDs() failed.\n");
		uninitialize();
		exit(EXIT_FAILURE);
	}

	// step 4 - take the 0th device from the oclDeviceIDs selected device
	oclDeviceID = oclDeviceIDs[0];
	// step 5 - free the memory for oclDeviceIDs
	free(oclDeviceIDs);

	// 3. create OpenCL context for the selected OpenCL device
	// step 1 - Create context properties array
	cl_context_properties oclContextProperties[] =
	{
		CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
		CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
		CL_CONTEXT_PLATFORM, (cl_context_properties)oclPlatformID,
		0
	};

	// step 2 - create the actual OpenCL context
	oclContext = clCreateContext(oclContextProperties, 1, &oclDeviceID, NULL, NULL, &oclResult);
	if (oclResult != CL_SUCCESS)
	{
		fprintf(gpFile, "ERROR: clCreateContext() failed.\n");
		uninitialize();
		exit(EXIT_FAILURE);
	}

	// 4. create command queue
	oclCommandQueue = clCreateCommandQueueWithProperties(oclContext, oclDeviceID, 0, &oclResult);
	if (oclResult != CL_SUCCESS)
	{
		fprintf(gpFile, "ERROR: clCreateCommandQueueWithProperties() failed.\n");
		uninitialize();
		exit(EXIT_FAILURE);
	}

	// 5. create OpenCL program from OpenCL kernel source code
	// step 1 - write OpenCL kernel source code
	const char* oclKernelSourceCode =
		"__kernel void rainKernel(\n"\
		"    __global float4* raindropPositions,				// Global memory for raindrop velocities\n"\
		"    __global float4* raindropVelocities,				// Global memory for raindrop positions\n"\
		"    const float gravity,								// Total number of raindrops\n"\
		"    const float timeStep,								// Gravity constant\n"\
		"    unsigned int width, unsigned int height)           // Time step for the simulation\n"\
		"{\n"
		"    unsigned int i = get_global_id(0);				    // Global ID along the x-axis\n"\
		"    unsigned int j = get_global_id(1);				    // Global ID along the y-axis\n"\
		"\n"\
		"    // Calculate the linear index in the 2D arrays\n"\
		"	 int index = j * width + i;\n"\
		"\n"\
		"    // Update raindrop velocities\n"\
		"    raindropVelocities[index].y -= gravity * timeStep;\n"\
		"    raindropVelocities[index].x *= timeStep;\n"\
		"    raindropVelocities[index].z *= timeStep;\n"\
		"    raindropVelocities[index].w *= timeStep;\n"\
		"\n"\
		"    // Update raindrop positions\n"\
		"\n"\
		"    raindropPositions[index].x += raindropVelocities[index].x * timeStep;\n"\
		"    raindropPositions[index].y += raindropVelocities[index].y * timeStep;\n"\
		"    raindropPositions[index].z += raindropVelocities[index].z * timeStep;\n"\
		"    raindropPositions[index].w += raindropVelocities[index].w * timeStep;\n"\
		"    if (raindropPositions[index].y < -25.0f)"\
		"    {"\
		"        // Reset the raindrop's position and velocity\n"\
		"        raindropPositions[index].y = 20.0f; // Place it back at the top\n"\
		"    }\n"\
		"}";

	// step 2 - create actual OpenCL program from above source code
	oclProgram = clCreateProgramWithSource(oclContext, 1, (const char**)&oclKernelSourceCode, NULL, &oclResult);
	if (oclResult != CL_SUCCESS)
	{
		fprintf(gpFile, "ERROR: clCreateProgramWithSource() failed.\n");
		uninitialize();
		exit(EXIT_FAILURE);
	}

	// 6. build program
	oclResult = clBuildProgram(oclProgram, 0, NULL, "-cl-fast-relaxed-math", NULL, NULL);
	if (oclResult != CL_SUCCESS)
	{
		fprintf(gpFile, "ERROR: clBuildProgram() failed: %d\n", oclResult);
		char buffer[1024]{};
		size_t length;
		oclResult = clGetProgramBuildInfo(oclProgram, oclDeviceID, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &length);
		if (oclResult != CL_SUCCESS)
		{
			fprintf(gpFile, "ERROR: clGetProgramBuildInfo() failed.\n");
		}
		fprintf(gpFile, "INFO: Program build log :%s\n", buffer);
		uninitialize();
		exit(EXIT_FAILURE);
	}

	// 7. create OpenCL kernel
	oclKernel = clCreateKernel(oclProgram, "rainKernel", &oclResult);
	if (oclResult != CL_SUCCESS)
	{
		fprintf(gpFile, "ERROR: clCreateKernel() failed.\n");
		uninitialize();
		exit(EXIT_FAILURE);
	}

	// Print OpenGL info
	printGLInfo();

	// Vertex Shader
	const GLchar* vertexShaderSourceCode =
		"#version 460 core\n"
		"\n"
		"layout(location = 0) in vec4 a_position;\n"
		"layout(location = 1) in vec4 a_vertexSeed;\n"\
		"layout(location = 2) in vec4 a_vertexVelo;\n"\
		"\n"
		"out VertexData\n"
		"{\n"
		"    float texArrayID;\n"
		"    vec4 velocity;\n"
		"} vertex;\n"
		"\n"
		"void main(void)\n"
		"{\n"
		"    vertex.texArrayID = a_vertexSeed.w;\n"
		"    vertex.velocity = a_vertexVelo;\n"
		"    gl_Position = a_position;\n"
		"}";

	// Creating shader obejct
	GLuint vertexShaderObject = glCreateShader(GL_VERTEX_SHADER);

	// Give shader code to shader object
	glShaderSource(vertexShaderObject, 1, (const GLchar**)&vertexShaderSourceCode, NULL);

	// Compile the shader code (inline compiler)
	glCompileShader(vertexShaderObject);

	// Getting the shader compilation status 
	glGetShaderiv(vertexShaderObject, GL_COMPILE_STATUS, &status);

	if (status == GL_FALSE)
	{
		// Getting the length of log of compilation status
		glGetShaderiv(vertexShaderObject, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 0)
		{
			// Allocate enough memory to the buffer to hold the compilation log
			log = (char*)malloc(infoLogLength);
			if (log != NULL)
			{
				GLsizei written;

				// Get the compilation log into the allocated buffer
				glGetShaderInfoLog(vertexShaderObject, infoLogLength, &written, log);

				// Display the contents of log
				fprintf(gpFile, "ERROR: Vertex Shader Compilation Log: %s", log);

				// Free the allocated buffer
				free(log);

				// Exit the application due to error
				uninitialize();
			}
		}
	}

	// Geometry Shader
	const GLchar* geometryShaderSourceCode =
		"#version 460 core\n"
		"\n"
		"precision highp float;" \
		"layout(points) in;\n"
		"layout(triangle_strip, max_vertices = 4) out;\n"
		"\n"
		"in VertexData\n"
		"{\n"
		"    float texArrayID;\n"
		"    vec4 velocity;\n"
	    "} vertex[];\n"
	    "\n"
	    "uniform mat4 projection;\n"\
	    "uniform mat4 view;\n"\
	    "uniform vec3 eyePosition;\n"
	    "uniform vec3 windDir;\n"
	    "uniform float dt;\n"
	    "\n"
	    "out vec3 fragmentTexCoords;\n"
		"out float randEnlight;\n"
		"out float texArrayID;\n"
		"out vec3 particlePosition;\n"
		"out vec3 particleVelocity;\n"
		"\n"
		"// GS for billboard technique (make two triangles from point).\n"
		"void main(void)\n"
		"{\n"
		"    //streak size\n"
		"    float height = 0.6;\n"
		"    float width = height / 25.0;\n"
		"\n"
		"    vec3 pos = gl_in[0].gl_Position.xyz;\n"
		"    vec3 toCamera = normalize(eyePosition - pos);\n"
		"    vec3 up = vec3(0.0, 1.0, 0.0);\n"
		"    vec3 right = cross(toCamera, up) * width * length(eyePosition - pos) * 0.5;\n"
		"\n"
		"    //bottom left\n"
		"    pos -= right;\n"
		"    fragmentTexCoords.xy = vec2(0, 0);\n"
		"    fragmentTexCoords.z = vertex[0].texArrayID;\n"
		"    randEnlight = vertex[0].velocity.w;\n"
		"    particlePosition = pos;\n"
		"    particleVelocity = vertex[0].velocity.xyz;\n"
		"    texArrayID = vertex[0].texArrayID;\n"
		"    gl_Position = projection * view * vec4(pos + (windDir * dt), 1.0);\n"
		"    EmitVertex();\n"
		"\n"
		"    //top left\n"
		"    pos.y += height;\n"
		"    fragmentTexCoords.xy = vec2(0, 1);\n"
		"    fragmentTexCoords.z = vertex[0].texArrayID;\n"
		"    randEnlight = vertex[0].velocity.w;\n"
		"    particlePosition = pos;\n"
		"    particleVelocity = vertex[0].velocity.xyz;\n"
		"    texArrayID = vertex[0].texArrayID;\n"
		"    gl_Position = projection * view * vec4(pos, 1.0);\n"
		"    EmitVertex();\n"
		"\n"
		"    //bottom right\n"
		"    pos.y -= height;\n"
		"    pos += right;\n"
		"    fragmentTexCoords.xy = vec2(1, 0);\n"
		"    fragmentTexCoords.z = vertex[0].texArrayID;\n"
		"    randEnlight = vertex[0].velocity.w;\n"
		"    particlePosition = pos;\n"
		"    particleVelocity = vertex[0].velocity.xyz;\n"
		"    texArrayID = vertex[0].texArrayID;\n"
		"    gl_Position = projection * view * vec4(pos + (windDir * dt), 1.0);\n"
		"    EmitVertex();\n"
		"\n"
		"    //top right\n"
		"    pos.y += height;\n"
		"    fragmentTexCoords.xy = vec2(1, 1);\n"
		"    fragmentTexCoords.z = vertex[0].texArrayID;\n"
		"    randEnlight = vertex[0].velocity.w;\n"
		"    particlePosition = pos;\n"
		"    particleVelocity = vertex[0].velocity.xyz;\n"
		"    texArrayID = vertex[0].texArrayID;\n"
		"    gl_Position = projection * view * vec4(pos, 1.0);\n"
		"    EmitVertex();\n"
		"\n"
		"    EndPrimitive();\n"
		"}";
	GLuint geometryShaderObject = glCreateShader(GL_GEOMETRY_SHADER);
	glShaderSource(geometryShaderObject, 1, (const GLchar**)&geometryShaderSourceCode, NULL);
	glCompileShader(geometryShaderObject);

	status = 0;
	infoLogLength = 0;
	log = NULL;

	glGetShaderiv(geometryShaderObject, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		glGetShaderiv(geometryShaderObject, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 0)
		{
			log = (char*)malloc(infoLogLength);
			if (log != NULL)
			{
				GLsizei written;

				glGetShaderInfoLog(geometryShaderObject, infoLogLength, &written, log);
				fprintf(gpFile, "Geometry Shader Compilation Log : %s\n", log);
				free(log);
				uninitialize();
			}
		}
	}

	// Fragment Shader
	const GLchar* fragmentShaderSourceCode =

		"#version 460 core\n"
		"\n"
		"#extension GL_EXT_gpu_shader4 : enable\n"
		"\n"
		"in vec3 fragmentTexCoords;\n"
		"in float randEnlight;\n"
		"in float texArrayID;\n"
		"in vec3 particlePosition;\n"
		"in vec3 particleVelocity;\n"
		"uniform float textureIndex;\n"
		"\n"
		"uniform sampler2DArray rainTex;\n"
		"uniform sampler1D rainfactors;\n"
		"uniform vec3 eyePosition;\n"
		"\n"
		"\n"
		"out vec4 finalColor;\n"
		"\n"
		"void main(void)\n"
		"{\n"
		"    finalColor = vec4(texture(rainTex, vec3(fragmentTexCoords.xy, textureIndex)).r, texture(rainTex, vec3(fragmentTexCoords.xy, textureIndex)).r, texture(rainTex, vec3(fragmentTexCoords.xy, textureIndex)).r, 1.);\n"
		"\n"
		"}";

	GLuint fragmentShaderObject = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShaderObject, 1, (const GLchar**)&fragmentShaderSourceCode, NULL);
	glCompileShader(fragmentShaderObject);

	status = 0;
	infoLogLength = 0;
	log = NULL;

	glGetShaderiv(fragmentShaderObject, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		glGetShaderiv(fragmentShaderObject, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 0)
		{
			log = (char*)malloc(infoLogLength);
			if (log != NULL)
			{
				GLsizei written;

				glGetShaderInfoLog(fragmentShaderObject, infoLogLength, &written, log);
				fprintf(gpFile, "INFO: Fragment Shader Compilation Log : %s\n", log);
				free(log);
				uninitialize();
			}
		}
	}

	// Shader program object
	shaderProgramObject = glCreateProgram();

	// Attach desired
	glAttachShader(shaderProgramObject, vertexShaderObject);
	glAttachShader(shaderProgramObject, geometryShaderObject);
	glAttachShader(shaderProgramObject, fragmentShaderObject);

	// Link the shader program object
	glLinkProgram(shaderProgramObject);

	status = 0;
	infoLogLength = 0;
	log = NULL;

	glGetProgramiv(shaderProgramObject, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		glGetProgramiv(shaderProgramObject, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 0)
		{
			log = (char*)malloc(infoLogLength);
			if (log != NULL)
			{
				GLsizei written;

				glGetProgramInfoLog(shaderProgramObject, infoLogLength, &written, log);
				fprintf(gpFile, "INFO: Shader Program Link Log: %s\n", log);
				free(log);
				uninitialize();
			}
		}
	}

	for (unsigned int i = 0; i < max_width; i++)
	{
		for (unsigned int j = 0; j < max_height; j++)
		{
			for (unsigned int k = 0; k < 4; k++)
			{

				if (k == 0)
				{
					raindropPositions[i][j][k] = 10.0f;
					raindropVelocities[i][j][k] = 10.0f;
					raindropSeed[i][j][k] = 10.0f;

				}
				if (k == 1)
				{
					raindropPositions[i][j][k] = 10.0f;
					raindropVelocities[i][j][k] = 10.0f;
					raindropSeed[i][j][k] = 10.0f;
				}
				if (k == 2)
				{
					raindropPositions[i][j][k] = 10.0f;
					raindropVelocities[i][j][k] = 10.0f;
					raindropSeed[i][j][k] = 10.0f;
				}
				if (k == 3)
				{
					raindropPositions[i][j][k] = 1.0f;			// w of homogenous coordinate
					raindropVelocities[i][j][k] = 1.0f;
					raindropSeed[i][j][k] = 10.0f;

				}
			}
		}
	}

	for (int i = 0; i < meshWidth; i++)
	{
		for (int j = 0; j < meshHeight; j++)
		{
			for (unsigned int k = 0; k < 4; k++)
			{
				float x, y, z;
				do
				{
					x = (uniformDist(r)) * (clusterScale + i * 5);
					y = (uniformDist(r) + 1.0f) * clusterScale;
					z = (uniformDist(r)) * (clusterScale + i * 5);
				} while ((z < -10.0f && z > 200.6f));

				if (k == 0)
				{
					raindropSeed[i][j][k] = x;

				}
				if (k == 1)
				{
					raindropSeed[i][j][k] = y;
				}
				if (k == 2)
				{
					raindropSeed[i][j][k] = z;

				}
				if (k == 3)
				{
					raindropSeed[i][j][k] = 1.0f;
				}
			}
		}
	}

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &positionBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
	glBufferData(GL_ARRAY_BUFFER, meshWidth * meshHeight * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glGenBuffers(1, &velocityBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, velocityBuffer);
	glBufferData(GL_ARRAY_BUFFER, meshWidth * meshHeight * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glGenBuffers(1, &seedBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, seedBuffer);
	glBufferData(GL_ARRAY_BUFFER, meshWidth * meshHeight * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);


	// wind
	float windForce = 20.0; // You can set your desired wind force value
	float windRand = static_cast<float>(r()) / static_cast<float>(r.max()) * windForce;

	for (int i = 0; i < 10 / 2; i++) 
	{
		float fraction = static_cast<float>(i) / static_cast<float>(10);
		windDir[0] = sin(fraction) * windRand;
		windDir[10 - i - 1] = sin(fraction) * windRand;
		windDir[2] = sin(fraction) * windRand;
		windDir[10 - i - 1] = sin(fraction) * windRand;
	}

	loadTextureArray(&rainTexArray, "C:\\RTR4.0\\GL_Effects\\HPP\\RainOpenCL\\textures\\cv0_vPos(0).png", 370);

	//// Create opencl-opengl interoperability resource
	graphicsResource_position = clCreateFromGLBuffer(oclContext, CL_MEM_WRITE_ONLY, positionBuffer, &oclResult);
	if (oclResult != CL_SUCCESS)
	{
		fprintf(gpFile, "ERROR: clCreateFromGLBuffer() failed for position buffer.\n");
		uninitialize();
		exit(EXIT_FAILURE);
	}

	graphicsResource_velocity = clCreateFromGLBuffer(oclContext, CL_MEM_WRITE_ONLY, velocityBuffer, &oclResult);
	if (oclResult != CL_SUCCESS)
	{
		fprintf(gpFile, "ERROR: clCreateFromGLBuffer() failed for velocity buffer.\n");
		uninitialize();
		exit(EXIT_FAILURE);
	}

	graphicsResource_seed = clCreateFromGLBuffer(oclContext, CL_MEM_READ_ONLY, seedBuffer, &oclResult);
	if (oclResult != CL_SUCCESS)
	{
		fprintf(gpFile, "ERROR: clCreateFromGLBuffer() failed for velocity buffer.\n");
		uninitialize();
		exit(EXIT_FAILURE);
	}

	// Depth and Clear color Changes
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glClearColor(0.10f, 0.10f, 0.10f, 1.0f);

	glEnable(GL_PROGRAM_POINT_SIZE);
	glEnable(GL_POINT_SPRITE);
	glEnable(GL_TEXTURE_2D_ARRAY);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_1D);

	perspectiveProjectionMatrix = mat4::identity();

	initializeFontRendering();
	initializeIntroOutro();

	PlaySound(MAKEINTRESOURCE(MYMUSIC_INTRO), NULL, SND_RESOURCE | SND_ASYNC);
	//PlaySound(MAKEINTRESOURCE(MYMUSIC_OUTRO), NULL, SND_RESOURCE | SND_ASYNC);

	resize(WIN_WIDTH, WIN_HEIGHT);
	return(0);
}

void resize(int width, int height)
{
	// Code
	gWidth = width;
	gHeight = height;

	if (height == 0)
		height = 1; // To avoid divided by Zero (Illegal instruction) in future codes 

	glViewport(0, 0, (GLsizei)width, (GLsizei)height);
	perspectiveProjectionMatrix = vmath::perspective(45.0f, (GLfloat)width / (GLfloat)height, 0.1f, 10000.0f);
}

void display(void)
{
	// function declarations
	void uninitialize(void);
	void drawText(std::string text, float x, float y, float scale, vmath::vec3 color);

	// variable declarations
	size_t globalWorkSize[2]{};
	
	char str[255] = { 0 };
	char meshSize[255] = { 0 };
	char raindrops[255] = { 0 };
	char FPS[255] = { 0 };
	char cpuToggle[255] = { 0 };
	char gpuToggle[255] = { 0 };
	char windtoggle[255] = { 0 };
	vmath::vec3 fpsColor;

	//sprintf(str, "Raindrops MeshWidth = %d, MeshHeight = %d", meshWidth, meshHeight);
	//SetWindowTextA(ghwnd, str);
	// 
	sprintf(str, "tx = %.3f, ty = %.3f, tz = %.3f | currentFPS = %f", tx, ty, tz, framesPerSec);
	SetWindowTextA(ghwnd, str);

	// Transformations
	mat4 translationMatrix = mat4::identity();
	mat4 modelMatrix = mat4::identity();
	mat4 viewMatrix = mat4::identity();

	// Code
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	if (sceneCounter == 0)
	{
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glUseProgram(shaderProgramObjectIntroOutro);

		translationMatrix = vmath::translate(0.0f, 0.0f, -2.0f);

		glUniformMatrix4fv(glGetUniformLocation(shaderProgramObjectIntroOutro, "u_modelMatrix"), 1, GL_FALSE, modelMatrix * translationMatrix);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgramObjectIntroOutro, "u_viewMatrix"), 1, GL_FALSE, viewMatrix);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgramObjectIntroOutro, "u_projectionMatrix"), 1, GL_FALSE, perspectiveProjectionMatrix);

		glActiveTexture(GL_TEXTURE0);
		glUniform1i(glGetUniformLocation(shaderProgramObjectIntroOutro, "u_textureSampler"), 0);
		if (texturecounter == 0)
		{
			glBindTexture(GL_TEXTURE_2D, texture_amc);
			if (fadeduration < 1.0f)
				glUniform1f(glGetUniformLocation(shaderProgramObjectIntroOutro, "time"), fadeduration);
			else
				glUniform1f(glGetUniformLocation(shaderProgramObjectIntroOutro, "time"), fadeoutduration);
		}
		else if (texturecounter == 1)
		{
			glBindTexture(GL_TEXTURE_2D, texture_title);
			if (fadeduration < 1.0f)
			{
				glUniform1f(glGetUniformLocation(shaderProgramObjectIntroOutro, "time"), fadeduration);
				fadeoutduration = 5.0f;
			}
			else
				glUniform1f(glGetUniformLocation(shaderProgramObjectIntroOutro, "time"), fadeoutduration);
		}
		glBindVertexArray(vao_intro);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);

		glUseProgram(0);
	}
	else if (sceneCounter == 2)
	{
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		if(count == 1)
			PlaySound(MAKEINTRESOURCE(MYMUSIC_OUTRO), NULL, SND_RESOURCE | SND_ASYNC);
		count = 2;
		glUseProgram(shaderProgramObjectIntroOutro);

		translationMatrix = vmath::translate(0.0f, 0.0f, -2.0f);

		glUniformMatrix4fv(glGetUniformLocation(shaderProgramObjectIntroOutro, "u_modelMatrix"), 1, GL_FALSE, modelMatrix * translationMatrix);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgramObjectIntroOutro, "u_viewMatrix"), 1, GL_FALSE, viewMatrix);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgramObjectIntroOutro, "u_projectionMatrix"), 1, GL_FALSE, perspectiveProjectionMatrix);

		glActiveTexture(GL_TEXTURE0);
		glUniform1i(glGetUniformLocation(shaderProgramObjectIntroOutro, "u_textureSampler"), 0);
		if (texturecounter == 2)
		{
			glBindTexture(GL_TEXTURE_2D, texture_ankit);
			if (fadeduration < 1.0f)
			{
				glUniform1f(glGetUniformLocation(shaderProgramObjectIntroOutro, "time"), fadeduration);
				fadeoutduration = 5.0f;
			}
			else
				glUniform1f(glGetUniformLocation(shaderProgramObjectIntroOutro, "time"), fadeoutduration);
		}
		else if (texturecounter == 3)
		{
			glBindTexture(GL_TEXTURE_2D, texture_references);
			glUniformMatrix4fv(glGetUniformLocation(shaderProgramObjectIntroOutro, "u_modelMatrix"), 1, GL_FALSE, modelMatrix * vmath::translate(0.0f, 0.0f, -2.5f));
			if (fadeduration < 1.0f)
			{
				glUniform1f(glGetUniformLocation(shaderProgramObjectIntroOutro, "time"), fadeduration);
				fadeoutduration = 5.0f;
			}
			else
				glUniform1f(glGetUniformLocation(shaderProgramObjectIntroOutro, "time"), fadeoutduration);
		}
		else if (texturecounter == 4)
		{
			glBindTexture(GL_TEXTURE_2D, texture_specialthanks);
			if (fadeduration < 1.0f)
			{
				glUniform1f(glGetUniformLocation(shaderProgramObjectIntroOutro, "time"), fadeduration);
				fadeoutduration = 5.0f;
			}
			else
				glUniform1f(glGetUniformLocation(shaderProgramObjectIntroOutro, "time"), fadeoutduration);
		}
		else if (texturecounter == 5)
		{
			glBindTexture(GL_TEXTURE_2D, texture_ignited);
			if (fadeduration < 1.0f)
			{
				glUniform1f(glGetUniformLocation(shaderProgramObjectIntroOutro, "time"), fadeduration);
				fadeoutduration = 5.0f;
			}
			else
				glUniform1f(glGetUniformLocation(shaderProgramObjectIntroOutro, "time"), fadeoutduration);
		}
		else if (texturecounter == 6)
		{
			glBindTexture(GL_TEXTURE_2D, texture_thankyou);
			if (fadeduration < 1.0f)
			{
				glUniform1f(glGetUniformLocation(shaderProgramObjectIntroOutro, "time"), fadeduration);
				fadeoutduration = 5.0f;
			}
			else
				glUniform1f(glGetUniformLocation(shaderProgramObjectIntroOutro, "time"), fadeoutduration);
		}
		glBindVertexArray(vao_intro);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);

		glUseProgram(0);
	}
	else
	{
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glUseProgram(shaderProgramObjectFont);

		if (gWidth <= gHeight)
		{
			orthographicProjectionMatrix = vmath::ortho(-100.0f,
				100.0f,
				-100.0f * (GLfloat)gHeight / (GLfloat)gWidth,
				100.0f * (GLfloat)gHeight / (GLfloat)gWidth,
				-100.0f,
				100.0f);
		}
		else
		{
			orthographicProjectionMatrix = vmath::ortho(-100.0 * (GLfloat)gWidth / (GLfloat)gHeight,
				100.0 * (GLfloat)gWidth / (GLfloat)gHeight,
				-100.0f,
				100.0f,
				-100.0f,
				100.0f);
		}

		glUniformMatrix4fv(glGetUniformLocation(shaderProgramObjectFont, "projection"), 1, GL_FALSE, orthographicProjectionMatrix);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);
		glUniform1i(glGetUniformLocation(shaderProgramObject, "text"), 0);

		wsprintf(meshSize, TEXT("Mesh Size: %d * %d * 4"), meshWidth, meshHeight);
		wsprintf(raindrops, TEXT("Rain Drops: %d"), meshWidth * meshHeight * 4);
		wsprintf(FPS, TEXT("FPS: %d"), (int)framesPerSec);

		if (onGPU == TRUE)
			drawText("GPU - NVIDIA GeForce RTX 4080 Laptop GPU (OpenCL)", -169.4f, 90.6, 0.09f, vmath::vec3(0.0, 1.0f, 0.0f));
		else
			drawText("CPU - Intel(R) Core(TM) i9-13980HX", -169.4f, 90.6, 0.09f, vmath::vec3(1.0, 1.0f, 0.0f));
		drawText(meshSize, -169.4f, 90.6 - 6.8, 0.09f, vmath::vec3(1.0, 1.0f, 1.0f));
		drawText(raindrops, -169.4f, 90.6 - 6.8 * 2, 0.09f, vmath::vec3(1.0, 1.0f, 1.0f));
		drawText("Button Controls", -169.4, -80.4, 0.06f, vmath::vec3(1.0, 1.0f, 1.0f));
		drawText("'c' - CPU", -169.4f, -84.9, 0.06f, vmath::vec3(1.0, 1.0f, 1.0f));
		drawText("'g' - GPU", -169.4f, -89.4, 0.06f, vmath::vec3(1.0, 1.0f, 1.0f));
		drawText("'w' - Wind", -169.4f, -93.9, 0.06f, vmath::vec3(1.0, 1.0f, 1.0f));

		if (framesPerSec <= 30.0 && framesPerSec >= 10.0)
			fpsColor = vec3(1.0f, 1.0f, 0.0f);
		else if (framesPerSec <= 10.0f)
			fpsColor = vec3(1.0f, 0.0f, 0.0f);
		else
			fpsColor = vec3(0.0f, 1.0f, 0.0f);

		drawText(FPS, -169.4f, 90.6 - 6.8 * 3, 0.09f, fpsColor);
		drawText(raindrops, -169.4f, 90.6 - 6.8 * 2, 0.09f, vmath::vec3(1.0, 1.0f, 1.0f));

		// Unuse the shader program object
		//glDisable(GL_BLEND);
		glUseProgram(0);

		glUseProgram(shaderProgramObject);

		// Transformations
		translationMatrix = mat4::identity();
		modelMatrix = mat4::identity();

		viewMatrix = mat4::identity();
		vec3 eyePosition = vec3(0.0f, 2.0f, 5.0f);
		vec3 center = vec3(0.0f, 0.0f, -0.0f);
		vec3 up = vec3(0.0f, 1.0f, 0.0f);
		viewMatrix = lookat(eyePosition, center, up);
		vec3 windDirection = vec3(0.0f, -1.0f, 0.0f);
		vec3 sunPosition = vec3(0.0f, 10.0f, 10.0f);
		vec3 sunColor = vec3(1.0f, 1.0f, 0.0f);

		glUniformMatrix4fv(glGetUniformLocation(shaderProgramObject, "view"), 1, GL_FALSE, viewMatrix);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgramObject, "projection"), 1, GL_FALSE, perspectiveProjectionMatrix);

		glUniform3fv(glGetUniformLocation(shaderProgramObject, "eyePosition"), 1, vec3(0.0f, 5.0f, 2.0f));
		glUniform3fv(glGetUniformLocation(shaderProgramObject, "sunDir"), 1, sunPosition);
		glUniform3fv(glGetUniformLocation(shaderProgramObject, "sunColor"), 1, sunColor);
		//glUniform3fv(glGetUniformLocation(shaderProgramObject, "dt"), 1, dt);
		glUniform1f(glGetUniformLocation(shaderProgramObject, "textureIndex"), dt);
		glUniform1f(glGetUniformLocation(shaderProgramObject, "sunIntensity"), 1000.0f);

		if (wind == TRUE)
		{
			glUniform3fv(glGetUniformLocation(shaderProgramObject, "windDir"), 1, windDir);
			glUniform1f(glGetUniformLocation(shaderProgramObject, "dt"), 0.07);
		}
		else
		{
			glUniform3fv(glGetUniformLocation(shaderProgramObject, "windDir"), 1, vec3(0.0f, 0.0f, 0.0f));
			glUniform1f(glGetUniformLocation(shaderProgramObject, "dt"), 0.00);
		}
		glBindVertexArray(vao);
		if (onGPU == TRUE)
		{
			// OpenCL related code
			// 1. set OpenCL kernel parameters
			// step 1 - passing 0th parameter
			oclResult = clSetKernelArg(oclKernel, 0, sizeof(cl_mem), (void*)&graphicsResource_position);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clSetKernelArg() failed for 0th parameter.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}

			// 2nd paramter
			oclResult = clSetKernelArg(oclKernel, 1, sizeof(cl_mem), (void*)&graphicsResource_velocity);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clSetKernelArg() failed for 1st parameter.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}

			// 3rd parameter
			oclResult = clSetKernelArg(oclKernel, 2, sizeof(float), &gravity);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clSetKernelArg() failed for 2nd parameter.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}

			// 4th parameter
			timeStep = 0.004f;
			oclResult = clSetKernelArg(oclKernel, 3, sizeof(float), &timeStep);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clSetKernelArg() failed for 3rd parameter.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}

			// 5th parameter
			oclResult = clSetKernelArg(oclKernel, 4, sizeof(unsigned int), &meshWidth);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clSetKernelArg() failed for 4th parameter.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}
			// 6th parameter
			oclResult = clSetKernelArg(oclKernel, 5, sizeof(unsigned int), &meshHeight);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clSetKernelArg() failed for 4th parameter.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}

			// 2. Enqueue graphics resource into the command queue
			oclResult = clEnqueueAcquireGLObjects(oclCommandQueue, 1, &graphicsResource_position, 0, NULL, NULL);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clEnqueueAcquireGLObjects() failed.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}

			oclResult = clEnqueueAcquireGLObjects(oclCommandQueue, 1, &graphicsResource_velocity, 0, NULL, NULL);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clEnqueueAcquireGLObjects() failed.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}

			// run the OpenCL kernel
			globalWorkSize[0] = meshWidth;
			globalWorkSize[1] = meshWidth;

			oclResult = clEnqueueNDRangeKernel(oclCommandQueue, oclKernel, 2, NULL, globalWorkSize, NULL, 0, NULL, NULL);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clEnqueueNDRangeKernel() failed.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}

			// release aquired GL objects
			oclResult = clEnqueueReleaseGLObjects(oclCommandQueue, 1, &graphicsResource_position, 0, NULL, NULL);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clEnqueueReleaseGLObjects() failed.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}
			oclResult = clEnqueueReleaseGLObjects(oclCommandQueue, 1, &graphicsResource_velocity, 0, NULL, NULL);
			if (oclResult != CL_SUCCESS)
			{
				fprintf(gpFile, "ERROR: clEnqueueReleaseGLObjects() failed.\n");
				uninitialize();
				exit(EXIT_FAILURE);
			}

			// finish the command queue
			clFinish(oclCommandQueue);

			// GPU vbo related code
			glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
			glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
			glEnableVertexAttribArray(0);

			glBindBuffer(GL_ARRAY_BUFFER, velocityBuffer);
			glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, NULL);
			glEnableVertexAttribArray(2);

		}
		else
		{
			if (count == 0)
			{
				for (int i = 0; i < meshWidth; i++)
				{
					for (int j = 0; j < meshHeight; j++)
					{
						for (unsigned int k = 0; k < 4; k++)
						{
							float x, y, z;
							do
							{
								x = (uniformDist(r)) * (clusterScale + i * 5);
								y = (uniformDist(r) + 1.0f) * clusterScale;
								z = (uniformDist(r)) * (clusterScale + i * 5);
							} while ((z < 10.0f && z > -2.6f) && (x < 2.2f && x > -2.2f));

							if (k == 0)
							{
								raindropPositions[i][j][k] = x;
								raindropVelocities[i][j][k] = veloFactor * (uniformDist(r) / 100.0f);

							}
							if (k == 1)
							{
								raindropPositions[i][j][k] = y;
								raindropVelocities[i][j][k] = veloFactor * ((uniformDist(r) + 1.0f) / 20.0f);
							}
							if (k == 2)
							{
								raindropPositions[i][j][k] = z;
								raindropVelocities[i][j][k] = veloFactor * (uniformDist(r) / 100.0f);

							}
							if (k == 3)
							{
								raindropPositions[i][j][k] = 1.0f;			// w of homogenous coordinate
								raindropVelocities[i][j][k] = 1.0f;

							}
						}
					}
				}
				//PlaySound(MAKEINTRESOURCE(MYMUSIC_SECOND), NULL, SND_RESOURCE | SND_ASYNC);
			}
			count = 1;

			for (int i = 0; i < meshWidth; i++)
			{
				for (int j = 0; j < meshHeight; j++)
				{
					raindropVelocities[i][j][1] -= gravity * timeStep;
					raindropVelocities[i][j][0] *= timeStep;
					raindropVelocities[i][j][2] *= timeStep;
					raindropVelocities[i][j][3] *= timeStep;

					raindropPositions[i][j][0] += raindropVelocities[i][j][0] * timeStep;
					raindropPositions[i][j][1] += raindropVelocities[i][j][1] * timeStep;
					raindropPositions[i][j][2] -= raindropVelocities[i][j][2] * timeStep;
					raindropPositions[i][j][3] += raindropVelocities[i][j][3] * timeStep;

					if (raindropPositions[i][j][1] < -25.0f)
					{
						// Reset the raindrop's position and velocity
						raindropPositions[i][j][1] = 25.0f; // Place it back at the top
					}
				}
			}
			glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
			glBufferData(GL_ARRAY_BUFFER, meshWidth * meshHeight * 4 * sizeof(float), raindropPositions, GL_DYNAMIC_DRAW);
			glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
			glEnableVertexAttribArray(0);

			glBindBuffer(GL_ARRAY_BUFFER, velocityBuffer);
			glBufferData(GL_ARRAY_BUFFER, meshWidth * meshHeight * 4 * sizeof(float), raindropVelocities, GL_DYNAMIC_DRAW);
			glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, NULL);
			glEnableVertexAttribArray(2);

			glBindBuffer(GL_ARRAY_BUFFER, seedBuffer);
			glBufferData(GL_ARRAY_BUFFER, meshWidth* meshHeight * 4 * sizeof(float), raindropSeed, GL_DYNAMIC_DRAW);
			glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, NULL);
			glEnableVertexAttribArray(1);
		}

		// rain sampler 2D Array
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, rainTexArray);
		glUniform1i(glGetUniformLocation(shaderProgramObject, "rainTex"), 0);
		glDrawArrays(GL_POINTS, 0, meshWidth * meshHeight);

		glBindVertexArray(0);

		glUseProgram(0);
	}
	SwapBuffers(ghdc);
}

void update(void)
{
	// Code
	if (sceneCounter == 0)
	{
		if (texturecounter == 0)
		{
			if (fadeduration <= 7.0f)
			{
				fadeduration = fadeduration + 0.01f;
				if (fadeduration >= 7.0f)
					fadeduration = 2.0f;

				if (fadeduration >= 2.0f)
					fadeoutduration = fadeoutduration - 0.005f;
				if (fadeoutduration <= 0.0f)
				{
					fadeoutduration = 0.0f;

					// reset 
					fadeduration = 0.0f;
					texturecounter = 1;
				}
			}
		}
		else if (texturecounter == 1)
		{
			if (fadeduration <= 5.0f)
			{
				fadeduration = fadeduration + 0.01f;
				if (fadeduration >= 5.0f)
				{
					fadeduration = 1.0f;
				}
				if (fadeduration >= 1.0f)
					fadeoutduration = fadeoutduration - 0.005f;

				fadeoutduration = fadeoutduration - 0.005f;
				if (fadeoutduration <= 0.0f)
				{
					fadeoutduration = 0.0f;

					// reset 
					fadeduration = 0.0f;
					texturecounter = 2;
					sceneCounter = 1;
				}
			}
		}
	}
	if (texturecounter == 2 && sceneCounter == 2)
	{
		if (fadeduration <= 5.0f)
		{
			fadeduration = fadeduration + 0.005f;
			if (fadeduration >= 5.0f)
			{
				fadeduration = 1.0f;
			}
			if (fadeduration >= 1.0f)
				fadeoutduration = fadeoutduration - 0.005f;

			fadeoutduration = fadeoutduration - 0.005f;
			if (fadeoutduration <= 0.0f)
			{
				fadeoutduration = 0.0f;

				// reset 
				fadeduration = 0.0f;
				texturecounter = 3;
			}
		}
	}
	else if (texturecounter == 3)
	{
		if (fadeduration <= 5.0f)
		{
			fadeduration = fadeduration + 0.01f;
			if (fadeduration >= 5.0f)
			{
				fadeduration = 1.0f;
			}
			if (fadeduration >= 1.0f)
				fadeoutduration = fadeoutduration - 0.005f;

			fadeoutduration = fadeoutduration - 0.005f;
			if (fadeoutduration <= 0.0f)
			{
				fadeoutduration = 0.0f;

				// reset 
				fadeduration = 0.0f;
				texturecounter = 4;
			}
		}
	}
	else if (texturecounter == 4)
	{
		if (fadeduration <= 5.0f)
		{
			fadeduration = fadeduration + 0.01f;
			if (fadeduration >= 5.0f)
			{
				fadeduration = 1.0f;
			}
			if (fadeduration >= 1.0f)
				fadeoutduration = fadeoutduration - 0.005f;

			fadeoutduration = fadeoutduration - 0.005f;
			if (fadeoutduration <= 0.0f)
			{
				fadeoutduration = 0.0f;

				// reset 
				fadeduration = 0.0f;
				texturecounter = 5;
			}
		}
	}
	else if (texturecounter == 5)
	{
		if (fadeduration <= 5.0f)
		{
			fadeduration = fadeduration + 0.01f;
			if (fadeduration >= 5.0f)
			{
				fadeduration = 1.0f;
			}
			if (fadeduration >= 1.0f)
				fadeoutduration = fadeoutduration - 0.005f;

			fadeoutduration = fadeoutduration - 0.005f;
			if (fadeoutduration <= 0.0f)
			{
				fadeoutduration = 0.0f;

				// reset 
				fadeduration = 0.0f;
				texturecounter = 6;
			}
		}
	}
	else if (texturecounter == 6)
	{
		if (fadeduration <= 5.0f)
		{
			fadeduration = fadeduration + 0.01f;
			if (fadeduration >= 5.0f)
			{
				fadeduration = 1.0f;
			}
			if (fadeduration >= 1.0f)
				fadeoutduration = fadeoutduration - 0.005f;

			fadeoutduration = fadeoutduration - 0.005f;
			if (fadeoutduration <= 0.0f)
			{
				fadeoutduration = 0.0f;

				// reset 
				fadeduration = 0.0f;
				texturecounter = 7;
			}
		}
	}

	dt = dt + 0.1f;
	if (dt >= 370.0f)
		dt = 1.0f;
}

void uninitialize(void)
{
	// Function declarations
	void ToggleFullScreen(void);

	// Code
	if (gbFullScreen)
	{
		ToggleFullScreen(); // Make it normal then uninitialize the window and then Destroy Window.
	}
	// Deletion and uninitialization of vbo
	if (velocityBuffer)
	{
		if (graphicsResource_position)
		{
			clReleaseMemObject(graphicsResource_position);
			graphicsResource_position = NULL;
		}
		glDeleteBuffers(1, &velocityBuffer);
		velocityBuffer = 0;
	}
	if (positionBuffer)
	{
		if (graphicsResource_position)
		{
			clReleaseMemObject(graphicsResource_position);
			graphicsResource_position = NULL;
		}
		glDeleteBuffers(1, &positionBuffer);
		positionBuffer = 0;
	}
	if (vao)
	{
		glDeleteVertexArrays(1, &vao);
		vao = 0;
	}
	if (oclKernel)
	{
		clReleaseKernel(oclKernel);
		oclKernel = NULL;
	}

	if (oclProgram)
	{
		clReleaseProgram(oclProgram);
		oclProgram = NULL;
	}

	if (oclCommandQueue)
	{
		clReleaseCommandQueue(oclCommandQueue);
		oclCommandQueue = NULL;
	}

	if (oclContext)
	{
		clReleaseContext(oclContext);
		oclContext = NULL;
	}

	// Shaders uninitialization
	if (shaderProgramObjectIntroOutro)
	{
		glUseProgram(shaderProgramObjectIntroOutro);

		GLsizei numAttachedShaders;
		glGetProgramiv(shaderProgramObjectIntroOutro, GL_ATTACHED_SHADERS, &numAttachedShaders);

		GLuint* shaderObjects = NULL;
		shaderObjects = (GLuint*)malloc(numAttachedShaders * sizeof(GLuint));

		glGetAttachedShaders(shaderProgramObjectIntroOutro, numAttachedShaders, &numAttachedShaders, shaderObjects);
		for (GLsizei i = 0; i < numAttachedShaders; i++)
		{
			glDetachShader(shaderProgramObjectIntroOutro, shaderObjects[i]);
			glDeleteShader(shaderObjects[i]);
			shaderObjects[i] = 0;
		}
		free(shaderObjects);
		shaderObjects = NULL;

		glUseProgram(0);
		glDeleteProgram(shaderProgramObjectIntroOutro);
		shaderProgramObjectFont = 0;
	}

	if (shaderProgramObjectFont)
	{
		glUseProgram(shaderProgramObjectFont);

		GLsizei numAttachedShaders;
		glGetProgramiv(shaderProgramObjectFont, GL_ATTACHED_SHADERS, &numAttachedShaders);

		GLuint* shaderObjects = NULL;
		shaderObjects = (GLuint*)malloc(numAttachedShaders * sizeof(GLuint));

		glGetAttachedShaders(shaderProgramObjectFont, numAttachedShaders, &numAttachedShaders, shaderObjects);
		for (GLsizei i = 0; i < numAttachedShaders; i++)
		{
			glDetachShader(shaderProgramObjectFont, shaderObjects[i]);
			glDeleteShader(shaderObjects[i]);
			shaderObjects[i] = 0;
		}
		free(shaderObjects);
		shaderObjects = NULL;

		glUseProgram(0);
		glDeleteProgram(shaderProgramObjectFont);
		shaderProgramObjectFont = 0;
	}

	if (shaderProgramObject)
	{
		glUseProgram(shaderProgramObject);

		GLsizei numAttachedShaders;
		glGetProgramiv(shaderProgramObject, GL_ATTACHED_SHADERS, &numAttachedShaders);

		GLuint* shaderObjects = NULL;
		shaderObjects = (GLuint*)malloc(numAttachedShaders * sizeof(GLuint));

		glGetAttachedShaders(shaderProgramObject, numAttachedShaders, &numAttachedShaders, shaderObjects);
		for (GLsizei i = 0; i < numAttachedShaders; i++)
		{
			glDetachShader(shaderProgramObject, shaderObjects[i]);
			glDeleteShader(shaderObjects[i]);
			shaderObjects[i] = 0;
		}
		free(shaderObjects);
		shaderObjects = NULL;

		glUseProgram(0);
		glDeleteProgram(shaderProgramObject);
		shaderProgramObject = 0;
	}

	if (wglGetCurrentContext() == ghrc)
	{
		wglMakeCurrent(NULL, NULL);
	}
	if (ghrc)
	{
		wglDeleteContext(ghrc);
		ghrc = NULL;
	}
	if (ghdc)
	{
		ReleaseDC(ghwnd, ghdc);
		ghdc = NULL;
	}
	if (ghwnd)
	{
		DestroyWindow(ghwnd);
		ghwnd = NULL;		// Uninitialize can be called in any other message hence destroying the window here
	}
	if (gpFile)
	{
		fprintf(gpFile, "INFO: Log file is successfully Closed.\n");
		fclose(gpFile);
		gpFile = NULL;
	}
}

void printGLInfo(void)
{
	// Local Variable Declarations
	GLint numExtensions = 0;

	// Code
	fprintf(gpFile, "OpenGL Vendor: %s\n", glGetString(GL_VENDOR));
	fprintf(gpFile, "OpenGL Renderer: %s\n", glGetString(GL_RENDERER));
	fprintf(gpFile, "OpenGL Version: %s\n", glGetString(GL_VERSION));
	fprintf(gpFile, "GLSL Version : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);

	fprintf(gpFile, "Number of Supported Extensions: %d\n", numExtensions);

	for (int i = 0; i < numExtensions; i++)
	{
		fprintf(gpFile, "%s\n", glGetStringi(GL_EXTENSIONS, i));
	}
}

void printOpenCLInfo(void)
{
	// code
	fprintf(gpFile, "OpenCL INFORMATION: \n");
	fprintf(gpFile, "=====================================================================\n");
	cl_int result;
	cl_platform_id ocl_platform_id;
	cl_uint dev_count;
	cl_device_id* ocl_device_ids;
	char oclPlatformInfo[512];
	cl_uint ocl_num_platforms;

	// Get first platform ID
	result = clGetPlatformIDs(1, &ocl_platform_id, &ocl_num_platforms);
	if (result != CL_SUCCESS)
	{
		fprintf(gpFile, "ERROR: clGetPlatformIDs() Failed.\n");
		exit(EXIT_FAILURE);
	}

	// Get GPU device count
	result = clGetDeviceIDs(ocl_platform_id, CL_DEVICE_TYPE_GPU, 1, NULL, &dev_count);
	if (result != CL_SUCCESS)
	{
		fprintf(gpFile, "ERROR: clGetDeviceIDs() Failed.\n");
		exit(EXIT_FAILURE);
	}
	else if (dev_count == 0)
	{
		fprintf(gpFile, "ERROR: There is No OpenCL Supported Device on this System.\n");
		exit(EXIT_FAILURE);
	}
	else
	{
		// Get Platform name
		clGetPlatformInfo(ocl_platform_id, CL_PLATFORM_NAME, 500, &oclPlatformInfo, NULL);
		fprintf(gpFile, "OpenCL Supporting GPU Platform Name : %s\n", oclPlatformInfo);

		clGetPlatformInfo(ocl_platform_id, CL_PLATFORM_VERSION, 500, &oclPlatformInfo, NULL);
		fprintf(gpFile, "OpenCL Supporting GPU Platform Name : %s\n", oclPlatformInfo);

		//  Print supporting Device Number
		fprintf(gpFile, "Total Number Of OpenCL Supporting GPU Device/Devices on this System : %d\n", dev_count);

		// Allocate memory to hold devices ids
		ocl_device_ids = (cl_device_id*)malloc(sizeof(cl_device_id) * dev_count);

		//Get IDs into allocated Buffer
		clGetDeviceIDs(ocl_platform_id, CL_DEVICE_TYPE_GPU, dev_count, ocl_device_ids, NULL);

		char ocl_dev_prop[1024];
		int i;

		for (i = 0; i < (int)dev_count; i++)
		{
			fprintf(gpFile, "\n");
			fprintf(gpFile, "***************** GPU DEVICE GENERAL INFORMATION ******************\n");
			fprintf(gpFile, "===================================================================\n");
			fprintf(gpFile, "Number of the ocl_num_platforms                                : %d\n", ocl_num_platforms);
			fprintf(gpFile, "GPU Device Number                                              : %d\n", i);
			clGetDeviceInfo(ocl_device_ids[i], CL_DEVICE_NAME, sizeof(ocl_dev_prop), &ocl_dev_prop, NULL);
			fprintf(gpFile, "GPU Device Name                                                : %s\n", ocl_dev_prop);
			clGetDeviceInfo(ocl_device_ids[i], CL_DEVICE_VENDOR, sizeof(ocl_dev_prop), &ocl_dev_prop, NULL);
			fprintf(gpFile, "GPU Device Vendor                                              : %s\n", ocl_dev_prop);
			clGetDeviceInfo(ocl_device_ids[i], CL_DRIVER_VERSION, sizeof(ocl_dev_prop), &ocl_dev_prop, NULL);
			fprintf(gpFile, "GPU Device Driver Version                                      : %s\n", ocl_dev_prop);
			clGetDeviceInfo(ocl_device_ids[i], CL_DEVICE_VERSION, sizeof(ocl_dev_prop), &ocl_dev_prop, NULL);
			fprintf(gpFile, "GPU Device OpenCL Version                                      : %s\n", ocl_dev_prop);
			cl_uint clock_frequency;
			clGetDeviceInfo(ocl_device_ids[i], CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(clock_frequency), &clock_frequency, NULL);
			fprintf(gpFile, "GPU Device Clock Rate                                          : %u\n", clock_frequency);

			fprintf(gpFile, "\n");
			fprintf(gpFile, "***************** GPU DEVICE MEMORY INFORMATION *******************\n");
			fprintf(gpFile, "===================================================================\n");
			cl_ulong mem_size;
			clGetDeviceInfo(ocl_device_ids[i], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(mem_size), &mem_size, NULL);
			fprintf(gpFile, "GPU Device Global Memory                                         : %llu Bytes\n", (unsigned long long)mem_size);
			clGetDeviceInfo(ocl_device_ids[i], CL_DEVICE_LOCAL_MEM_SIZE, sizeof(mem_size), &mem_size, NULL);
			fprintf(gpFile, "GPU Device Local Memory                                          : %llu Bytes\n", (unsigned long long)mem_size);
			clGetDeviceInfo(ocl_device_ids[i], CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(mem_size), &mem_size, NULL);
			fprintf(gpFile, "GPU Device Constant Buffer Size                                  : %llu Bytes\n", (unsigned long long)mem_size);
			fprintf(gpFile, "\n");
			fprintf(gpFile, "***************** GPU DEVICE COMPUTE INFORMATION ******************\n");
			fprintf(gpFile, "===================================================================\n");
			cl_uint compute_units;
			clGetDeviceInfo(ocl_device_ids[i], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(compute_units), &compute_units, NULL);
			fprintf(gpFile, "GPU Device Number of Parallel Processors Cores                    : %u\n", compute_units);

			size_t workgroup_size;
			clGetDeviceInfo(ocl_device_ids[i], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(workgroup_size), &workgroup_size, NULL);
			fprintf(gpFile, "GPU Device Work Group Size                                        : %u\n", (unsigned int)workgroup_size);

			size_t workitem_dims;
			clGetDeviceInfo(ocl_device_ids[i], CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(workitem_dims), &workitem_dims, NULL);
			fprintf(gpFile, "GPU Device Work Item Dimensions                                   : %u\n", (unsigned int)workitem_dims);

			size_t workitem_size[3];
			clGetDeviceInfo(ocl_device_ids[i], CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(workitem_size), &workitem_size, NULL);
			fprintf(gpFile, "GPU Device Work Item Sizes                                        : %u\n", (unsigned int)workitem_size[0], (unsigned int)workitem_size[1], (unsigned int)workitem_size[2]);

		}
		free(ocl_device_ids);
	}
}

std::string getPathForTexture(int index)
{
	// Construct the file name pattern: cv0_vPos(index).png
	std::stringstream ss;
	ss << "C:\\RTR4.0\\GL_Effects\\HPP\\RainOpenCL\\textures\\cv0_vPos(" << index << ").png";
	return ss.str();
}

BOOL loadTextureArray(GLuint* texture, const char* path, GLsizei noOfTextures)
{
	int width = 0, height = 0;
	int components;
	TCHAR str[255];

	// Generate a texture object
	//glGenTextures(1, texture);
	//glActiveTexture(GL_TEXTURE12);
	//glBindTexture(GL_TEXTURE_2D_ARRAY, *texture);
	glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, texture);

	// Load and populate textures
	for (int i = 0; i < noOfTextures; i++)
	{
		// Construct the path for the current texture
		std::string texturePath = getPathForTexture(i);
		//std::string texturePath = "assets\\textures\\rain\\cv0_vPos(0).png";

		unsigned char* imageData = stbi_load(texturePath.c_str(), &width, &height, &components, 0);
		fprintf(gpFile, "INFO: Texture array \nPath: %s", texturePath.c_str());
		if (imageData == NULL)
		{
			wsprintf(str, TEXT("Failed to load texture array \nPath: %S\n"), texturePath.c_str());
			fprintf(gpFile, "ERROR: Failed to load texture array \nPath: %s", texturePath.c_str());
			MessageBox(NULL, str, TEXT("Load Texture Array Error"), MB_OK | MB_ICONERROR);
			return FALSE;
		}
		glTextureStorage3D(*texture, 1, GL_RGB8, width, height, noOfTextures);
		//glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RED, width, height, noOfTextures, 0, GL_RED, GL_FLOAT, nullptr);
		// Upload the image data to the texture array
		glTextureSubImage3D(*texture, 0, 0, 0, i, width, height, 1, GL_RED, GL_UNSIGNED_BYTE, imageData);
		glBindTextureUnit(12, *texture);

		// Free the image data
		if (imageData)
			stbi_image_free(imageData);
	}

	// Set texture filtering and wrapping modes if needed
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_REPEAT);

	// Unbind the texture
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	//stbi_image_free(imageData);
	return TRUE;
}

BOOL loadGLTexture_stbi(GLuint* texture, char* filename)
{
	// Variable declarations
	int width, height;
	int components;

	unsigned char* imageData = stbi_load(filename, &width, &height, &components, STBI_rgb_alpha);

	BOOL bResult = FALSE;

	// Code
	if (imageData)
	{
		bResult = TRUE;
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glGenTextures(1, texture);
		glBindTexture(GL_TEXTURE_2D, *texture);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData);
		//glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);

		stbi_image_free(imageData);
	}

	return bResult;
}

void drawText(std::string text, float x, float y, float scale, vmath::vec3 color)
{
	// activate corresponding render state	
	glUseProgram(shaderProgramObjectFont);
	glUniform3f(glGetUniformLocation(shaderProgramObjectFont, "textColor"), color[0], color[1], color[2]);
	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(VAO);

	// iterate through all characters
	std::string::const_iterator c;
	for (c = text.begin(); c != text.end(); c++)
	{
		Character ch = Characters[*c];

		float xpos = x + ch.Bearing[0] * scale;
		float ypos = y - (ch.Size[1] - ch.Bearing[1]) * scale;

		float w = ch.Size[0] * scale;
		float h = ch.Size[1] * scale;
		// update VBO for each character
		float vertices[6][4] = {
			{ xpos,     ypos + h,   0.0f, 0.0f },
			{ xpos,     ypos,       0.0f, 1.0f },
			{ xpos + w, ypos,       1.0f, 1.0f },

			{ xpos,     ypos + h,   0.0f, 0.0f },
			{ xpos + w, ypos,       1.0f, 1.0f },
			{ xpos + w, ypos + h,   1.0f, 0.0f }
		};
		// render glyph texture over quad
		glBindTexture(GL_TEXTURE_2D, ch.TextureID);
		// update content of VBO memory
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); // be sure to use glBufferSubData and not glBufferData

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		// render quad
		glDrawArrays(GL_TRIANGLES, 0, 6);
		// now advance cursors for next glyph (note that advance is number of 1/64 pixels)
		x += (ch.Advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
	}
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
}