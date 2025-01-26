//#include "stdafx.h"
#include "GL/glew.h"
#include "GL/freeglut.h"
#include "GL/freeglut_ext.h"
#include "InitShader.h"
#include <iostream>
#include <cctype> // For isprint function

// Create a NULL-terminated string by reading the provided file
static char* readShaderSource(const char* shaderFile)
{
    FILE* fp = fopen(shaderFile, "r");
    if (fp == NULL) {
        return NULL;
    }

    // Get the file size
    fseek(fp, 0L, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    // Allocate memory for the file content
    char* buf = new char[size + 1];
    fread(buf, 1, size, fp);
    buf[size] = '\0'; // Ensure null termination
    fclose(fp);

    // Step 1: Remove non-ASCII and non-printable characters
    long writeIndex = 0;
    for (long i = 0; i < size; ++i) {
        if (isprint(static_cast<unsigned char>(buf[i])) || isspace(static_cast<unsigned char>(buf[i]))) {
            buf[writeIndex++] = buf[i];
        }
    }
    buf[writeIndex] = '\0'; // Null-terminate the cleaned string

    // Step 2: Trim leading and trailing whitespace
    // Find the first non-whitespace character
    char* start = buf;
    while (*start && isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }

    // Find the last non-whitespace character
    char* end = buf + writeIndex - 1;
    while (end > start && isspace(static_cast<unsigned char>(*end))) {
        --end;
    }
    *(end + 1) = '\0'; // Null-terminate after the last valid character

    // Step 3: Copy trimmed content to a new buffer
    char* trimmedBuf = new char[strlen(start) + 1];
    strcpy(trimmedBuf, start);
    delete[] buf; // Free the original buffer

    return trimmedBuf;
}

// Create a GLSL program object from vertex and fragment shader files
GLuint InitShader(const char* vShaderFile, const char* fShaderFile)
{
    struct Shader {
	const char*  filename;
	GLenum       type;
	GLchar*      source;
    }  shaders[2] = {
	{ vShaderFile, GL_VERTEX_SHADER, NULL },
	{ fShaderFile, GL_FRAGMENT_SHADER, NULL }
    };

    GLuint program = glCreateProgram();
    
    for ( int i = 0; i < 2; ++i ) {
		Shader& s = shaders[i];
		s.source = readShaderSource( s.filename );
		if ( shaders[i].source == NULL ) {
			std::cerr << "Failed to read " << s.filename << std::endl;
			exit( EXIT_FAILURE );
		}
	
		GLuint shader = glCreateShader( s.type );
		glShaderSource( shader, 1, (const GLchar**) &s.source, NULL );
		glCompileShader( shader );

		GLint  compiled;
		glGetShaderiv( shader, GL_COMPILE_STATUS, &compiled );
		if ( !compiled ) {
			std::cerr << s.filename << " failed to compile:" << std::endl;
			GLint  logSize;
			glGetShaderiv( shader, GL_INFO_LOG_LENGTH, &logSize );
			char* logMsg = new char[logSize];
			glGetShaderInfoLog( shader, logSize, NULL, logMsg );
			std::cerr << logMsg << std::endl;
			delete [] logMsg;

			exit( EXIT_FAILURE );
		}

		delete [] s.source;

		glAttachShader( program, shader );
    }

    /* link  and error check */

	// Specify the varyings to capture
	const char* varyings[] = { "hitResult.origin_w", "hitResult.hit_point_w", "hitResult.distance", "hitResult.padding" };
	glTransformFeedbackVaryings(program, 4, varyings, GL_INTERLEAVED_ATTRIBS);

    glLinkProgram(program);

    GLint  linked;
    glGetProgramiv( program, GL_LINK_STATUS, &linked );
    if ( !linked ) {
	std::cerr << "Shader program failed to link" << std::endl;
	GLint  logSize;
	glGetProgramiv( program, GL_INFO_LOG_LENGTH, &logSize);
	char* logMsg = new char[logSize];
	glGetProgramInfoLog( program, logSize, NULL, logMsg );
	std::cerr << logMsg << std::endl;
	delete [] logMsg;

	exit( EXIT_FAILURE );
    }

    /* use program object */
    glUseProgram(program);

    return program;
}
