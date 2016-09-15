#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <X11/X.h>
#include <X11/Xlib.h>

#include <GL/glew.h>
#include <GL/freeglut.h>

#include <GL/glx.h>

#include "scheduler.h"
#include "blockSum.h"

int threadSize;

GLuint bufferIndex[3];

GLuint blockMatrixShader;

typedef struct
{
	char* data;
	GLint size;
} ShaderData;

void checkError()
{
	GLenum status = glGetError();

	if (status != GL_NO_ERROR)
	{
		printf("Error: %s (%i)", gluErrorString(status), status);
		exit(EXIT_FAILURE);
	}
}

ShaderData* readShader(const char *filename)
{
	FILE* file = fopen(filename, "rb");

	// get the length of the file
	fseek(file, 0, SEEK_END);
	long size = ftell(file);

	fseek(file, 0, SEEK_SET);

	// load the data
	ShaderData* sd = malloc(sizeof(ShaderData));
	sd->data = malloc(size + 1);
	sd->size = (GLint)size;

	if (fread(sd->data, 1, size, file) != size)
	{
		printf("Error reading the shader");
		exit(-1);
	}

	// close the file
	fclose(file);

	// add the null terminator
	sd->data[size] = '\0';

	return sd;
}

GLuint compileShader(const char *filename)
{
	// read the file
	ShaderData* shaderData = readShader(filename);

	if (shaderData->data == NULL)
	{
		printf("Error loading file");
		exit(EXIT_FAILURE);
	}

	// create the compute shader
	GLuint compute = glCreateShader(GL_COMPUTE_SHADER);

	// bind the shader to the program
	GLuint program = glCreateProgram();

	// read the shader
	glShaderSource(compute, 1, (const GLchar**)&shaderData->data, &shaderData->size);

	// compile the shader
	glCompileShader(compute);

	// check for compile errors
	GLint result;

	glGetShaderiv(compute, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE)
	{
		GLsizei length = 0;

		glGetShaderiv(compute, GL_INFO_LOG_LENGTH, &length);

		GLchar* log = (GLchar*)malloc(length);
		glGetShaderInfoLog(compute, length, &length, log);

		printf("Cannot compile shader.\n%s", log);

		exit(EXIT_FAILURE);
	}

	// now attach the shader to the program
	glAttachShader(program, compute);

	// check the linker had no errors
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &result);
	if (result != GL_TRUE)
	{
		GLsizei length = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

		GLchar* log = (GLchar*)malloc(length);
		glGetProgramInfoLog(program, length, &length, log);

		printf("Error in shader linker.\n%s", log);
		exit(EXIT_FAILURE);
	}

	return program;
}

void waitForGPU()
{
	GLsync syncObject = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	GLenum status = glClientWaitSync(syncObject, GL_SYNC_FLUSH_COMMANDS_BIT, 1000 * 1000 * 1000);
	if (status == GL_WAIT_FAILED || status == GL_TIMEOUT_EXPIRED)
	{
		printf("GPU is taking too long.");
		exit(EXIT_FAILURE);
	}

	glMemoryBarrier(GL_ALL_BARRIER_BITS);
	glDeleteSync(syncObject);
}

void windowlessOpenGL()
{
        Display *dpy;
        Window root;
        GLint attr[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
        XVisualInfo *vi;
        GLXContext glc;

        // open the display
        if (!(dpy = XOpenDisplay(NULL)))
        {
                fprintf(stderr, "cannot connect to X server\n\n");
                exit(1);
        }
 
        // get root window
        root = DefaultRootWindow(dpy);
  
        // get the attr vis
        if(!(vi = glXChooseVisual(dpy, 0, attr)))
        {
                fprintf(stderr, "no appropriate visual found\n\n");
                exit(1);
        }
 
        // create a context using the root window
        if (!(glc = glXCreateContext(dpy, vi, NULL, GL_TRUE)))
        {
                fprintf(stderr, "failed to create context\n\n");
                exit(1);
        }

        glXMakeCurrent(dpy, root, glc);
}

void setupGPU(void* data)
{
	// init glut and OpenGL
	int argc = 1;
	char *argv[1] = { (char*)"balance" };

	glutInit(&argc, argv);

	glEnable(GL_ARB_compute_shader);

	windowlessOpenGL();

	// init glew
	GLenum err = glewInit();

	if (err != GLEW_OK)
	{
		printf("Error: %s", glewGetErrorString(err));
		exit(EXIT_FAILURE);
	}

	// compile the balance shader
	blockMatrixShader = compileShader("mmult.comp");
}

void destroyGPU(void* data)
{
	// remove the program
	glDeleteProgram(blockMatrixShader);

	// delete the buffers
	glDeleteBuffers(3, bufferIndex);
}

void multiplyGPU(void* data)
{
	SchedPass* sp = (SchedPass*)data;

	int* A = sp->A;
	int* B = sp->B;
	pthread_mutex_t* groupLock = sp->groupLock;
	pthread_cond_t* groupSignal = sp->groupSignal;
	int* groupProgress = sp->groupProgress;

	int dimension = sp->dimension;
	int blocksPerGroup = sp->blocksPerGroup;
	int matrixWidth = blocksPerGroup * dimension;
	int* writeBack = sp->writeBack;
	int* outputSpot = sp->outputSpot;
	
	// calculate thread size
	threadSize = dimension / 16;
	
	// set the matrix size
	glProgramUniform1ui(blockMatrixShader, 0, dimension);

	glGenBuffers(1, &bufferIndex[0]);

	// setup the shader input buffer
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bufferIndex[0]);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint) * dimension * dimension, 0, GL_STATIC_DRAW);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint) * dimension * dimension, A);

	glGenBuffers(1, &bufferIndex[1]);

	// setup the shader input buffer
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bufferIndex[1]);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint) * dimension * dimension, 0, GL_STATIC_DRAW);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint) * dimension * dimension, B);

	glGenBuffers(1, &bufferIndex[2]);

	// setup the shader input buffer
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bufferIndex[2]);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint) * dimension * dimension, 0, GL_DYNAMIC_DRAW);
	
	// setup the balance shader
	glUseProgram(blockMatrixShader);

	// run the shader
	glDispatchCompute(threadSize, threadSize, 1);

	// wait for the shader to finish execution
	waitForGPU();

	// check that dispatch did not cause an error
	checkError();

	// get the data from the shader
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferIndex[2]);
	GLint* ssbo = (GLint*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);

	// copy over the data
	memcpy(writeBack, ssbo, sizeof(GLuint) * dimension * dimension);

	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	
	// sum up the block and delete excess data
	blockSum(data);
}
