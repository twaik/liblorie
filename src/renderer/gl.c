#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h> // GLES error descriotions

static const char gSimpleVS[] =
    "attribute vec4 position;\n"
    "attribute vec2 texCoords;\n"
    "varying vec2 outTexCoords;\n"
    "\nvoid main(void) {\n"
    "   outTexCoords = texCoords;\n"
    "   gl_Position = position;\n"
    "}\n\n";
static const char gSimpleFS[] =
    "precision mediump float;\n\n"
    "varying vec2 outTexCoords;\n"
    "uniform sampler2D texture;\n"
    "\nvoid main(void) {\n"
    //"   gl_FragColor = texture2D(texture, outTexCoords);\n"
    "   gl_FragColor = texture2D(texture, outTexCoords).bgra;\n"
    //"   gl_FragColor = vec4(outTexCoords.x/outTexCoords.y,outTexCoords.y/outTexCoords.x, 0.0, 0.0);\n"
    "}\n\n";

const GLfloat gTriangleVertices[] = { 0.0f, 0.5f, -0.5f, -0.5f,
        0.5f, -0.5f };

#define FLOAT_SIZE_BYTES 4;
const GLint TRIANGLE_VERTICES_DATA_STRIDE_BYTES = 5 * FLOAT_SIZE_BYTES;
const GLfloat gTriangleVerticesData[] = {
    // X, Y, Z, U, V
    -1.0f, -1.0f, 0, 0.f, 1.f,
    1.0f, -1.0f, 0, 0.f, 0.f,
    -1.0f,  1.0f, 0, 1.f, 0.f,
    1.0f,   1.0f, 0, 1.f, 1.f,
};

static void checkEGLError() {
	char* error = NULL;
	switch(eglGetError()) {
		#define E(code, desc) case code: error = desc; break;
		E(EGL_SUCCESS, "No error");
		E(EGL_NOT_INITIALIZED, "EGL not initialized or failed to initialize");
		E(EGL_BAD_ACCESS, "Resource inaccessible");
		E(EGL_BAD_ALLOC, "Cannot allocate resources");
		E(EGL_BAD_ATTRIBUTE, "Unrecognized attribute or attribute value");
		E(EGL_BAD_CONTEXT, "Invalid EGL context");
		E(EGL_BAD_CONFIG, "Invalid EGL frame buffer configuration");
		E(EGL_BAD_CURRENT_SURFACE, "Current surface is no longer valid");
		E(EGL_BAD_DISPLAY, "Invalid EGL display");
		E(EGL_BAD_SURFACE, "Invalid surface");
		E(EGL_BAD_MATCH, "Inconsistent arguments");
		E(EGL_BAD_PARAMETER, "Invalid argument");
		E(EGL_BAD_NATIVE_PIXMAP, "Invalid native pixmap");
		E(EGL_BAD_NATIVE_WINDOW, "Invalid native window");
		E(EGL_CONTEXT_LOST, "Context lost");
		#undef E
		default: error = "Unknown error";
	}
	LOGE("EGL: %s", error);
}

static void checkGlError(const char* op, int line) {
    GLint error;
    char *desc = NULL;
    for (error = glGetError(); error; error = glGetError()) {
		switch (error) {
			#define E(code) case code: desc = #code; break;
			E(GL_INVALID_ENUM);
			E(GL_INVALID_VALUE);
			E(GL_INVALID_OPERATION);
			E(GL_STACK_OVERFLOW_KHR);
			E(GL_STACK_UNDERFLOW_KHR);
			E(GL_OUT_OF_MEMORY);
			E(GL_INVALID_FRAMEBUFFER_OPERATION);
			E(GL_CONTEXT_LOST_KHR);
			#undef E
		}
        LOGE("GL: %s after %s() (line %d)", desc, op, line);
    }
}

#define checkGlError(op) checkGlError(op, __LINE__)
static GLuint loadShader(GLenum shaderType, const char* pSource) {
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char* buf = (char*) malloc(infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    LOGE("Could not compile shader %d:\n%s\n", shaderType, buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }
    return shader;
}

GLuint createProgram(const char* pVertexSource, const char* pFragmentSource) {
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        return 0;
    }

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        checkGlError("glAttachShader");
        glAttachShader(program, pixelShader);
        checkGlError("glAttachShader");
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    LOGE("Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}
