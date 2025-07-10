/*
 * This confidential and proprietary software should be used
 * under the licensing agreement from Allwinner Technology.

 * Copyright (C) 2020-2030 Allwinner Technology Limited
 * All rights reserved.

 * Author:zhengwanyu <zhengwanyu@allwinnertech.com>

 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from Allwinner Technology Limited.
 */
#ifndef _RESHADER_H_
#define _RESHADER_H_

#include <stdio.h>
#include <iostream>
#include <sstream>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

using namespace std;

#define SHADER_DEBUG 0

#define SD_INF(...) \
do { \
	if (SHADER_DEBUG) \
		printf(__VA_ARGS__); \
} while(0);

class REShaderProgram {
public:
	REShaderProgram(const char *vertex, const char *fragment)
	{
		int ret;

		vsId = createShader(GL_VERTEX_SHADER, vertex);
		fsId = createShader(GL_FRAGMENT_SHADER, fragment);
		if ((vsId < 0) || (fsId < 0)) {
			cerr << "create shader failed: " << vsId << fsId << endl;
			return;
		}

		programId = glCreateProgram();
		if (!programId) {
			cerr << "Error: failed to create shader_program!" << endl;
			return;
		}

		glAttachShader(programId, vsId);
		glAttachShader(programId, fsId);

		glBindAttribLocation(programId, 0, "aPosition");
		glBindAttribLocation(programId, 1, "aTexCoord");

		glLinkProgram(programId);
		glGetProgramiv(programId, GL_LINK_STATUS, &ret);
		if (!ret) {
			char *log;

			cerr << "Error: shader_program linking failed!\n"
								<< endl;
			glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &ret);
			if (ret > 1) {
				log = (char *)malloc(ret);
				glGetProgramInfoLog(programId, ret, NULL, log);
				cerr << log << endl;
				free(log);
			}
			return;
		}

		glDeleteShader(vsId);
		glDeleteShader(fsId);

		glUseProgram(programId);
	}

	~REShaderProgram()
	{
		vsId = 0;
		fsId = 0;
		programId = 0;
	}

	unsigned int getProgramId()
	{
		return programId;
	}

	void use()
	{
		glUseProgram(programId);
		SD_INF("[SD]use shader program, id:%d\n", programId);
	}

	void setMat4(const string &name, const glm::mat4 &mat) const
	{
		SD_INF("[SD]program id:%d setMat4:%s\n",
				programId, name.c_str());

		SD_INF("%f %f %f %f\n", mat[0][0], mat[1][0], mat[2][0], mat[3][0]);
		SD_INF("%f %f %f %f\n", mat[0][1], mat[1][1], mat[2][1], mat[3][1]);
		SD_INF("%f %f %f %f\n", mat[0][2], mat[1][2], mat[2][2], mat[3][2]);
		SD_INF("%f %f %f %f\n", mat[0][3], mat[1][3], mat[2][3], mat[3][3]);

		glUniformMatrix4fv(glGetUniformLocation(programId, name.c_str()),
					1, GL_FALSE, &mat[0][0]);
	}

	void setInt(const string &name, int value) const
	{ 
		glUniform1i(glGetUniformLocation(programId, name.c_str()), value);
	}

private:
	unsigned int vsId, fsId;
	GLuint programId;

	static int createShader(int type, const char *source)
	{
		GLint ret;
		int id;

		id = glCreateShader(type);
		if (!id) {
			cerr << "glCreateShader vertex failed" << endl;
			return -1;
		}

		glShaderSource(id, 1, &source, NULL);
		glCompileShader(id);

		glGetShaderiv(id, GL_COMPILE_STATUS, &ret);
		if (!ret) {
			char *log;

			cerr << "Error: shader compilation failed, type:"
			     << type << endl;
			glGetShaderiv(id, GL_INFO_LOG_LENGTH, &ret);

			if (ret > 1) {
				log = (char *)malloc(ret);
				glGetShaderInfoLog(id, ret, NULL, log);
				cerr << log << endl;
				free(log);
			}

			return -1;
		}
	
		return id;
	}
};

#endif
