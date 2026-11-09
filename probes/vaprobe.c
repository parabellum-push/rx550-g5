/* vaprobe: what a VS sees for 2_10_10_10 vertex attributes (XFB capture). EGL_PLATFORM=surfaceless. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#define F(ret, name, args) static ret (*name##_) args
F(EGLDisplay, eglGetPlatformDisplay, (EGLenum, void *, const EGLAttrib *));
F(EGLBoolean, eglInitialize, (EGLDisplay, EGLint *, EGLint *));
F(EGLBoolean, eglBindAPI, (EGLenum));
F(EGLContext, eglCreateContext, (EGLDisplay, EGLConfig, EGLContext, const EGLint *));
F(EGLBoolean, eglMakeCurrent, (EGLDisplay, EGLSurface, EGLSurface, EGLContext));
F(void *, eglGetProcAddress, (const char *));
F(void, glGenBuffers, (GLsizei, GLuint *));
F(void, glBindBuffer, (GLenum, GLuint));
F(void, glBufferData, (GLenum, GLsizeiptr, const void *, GLenum));
F(void, glBindBufferBase, (GLenum, GLuint, GLuint));
F(GLuint, glCreateShader, (GLenum));
F(void, glShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *));
F(void, glCompileShader, (GLuint));
F(GLuint, glCreateProgram, (void));
F(void, glAttachShader, (GLuint, GLuint));
F(void, glLinkProgram, (GLuint));
F(void, glUseProgram, (GLuint));
F(void, glTransformFeedbackVaryings, (GLuint, GLsizei, const GLchar *const *, GLenum));
F(void, glBindAttribLocation, (GLuint, GLuint, const GLchar *));
F(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *));
F(void, glEnableVertexAttribArray, (GLuint));
F(void, glBeginTransformFeedback, (GLenum));
F(void, glEndTransformFeedback, (void));
F(void, glEnable, (GLenum));
F(void, glDrawArrays, (GLenum, GLint, GLsizei));
F(void *, glMapBufferRange, (GLenum, GLintptr, GLsizeiptr, GLbitfield));
F(GLboolean, glUnmapBuffer, (GLenum));
F(GLenum, glGetError, (void));
F(void, glGenVertexArrays, (GLsizei, GLuint *));
F(void, glBindVertexArray, (GLuint));
F(void, glGenFramebuffers, (GLsizei, GLuint *));
F(void, glBindFramebuffer, (GLenum, GLuint));
F(void, glGenRenderbuffers, (GLsizei, GLuint *));
F(void, glBindRenderbuffer, (GLenum, GLuint));
F(void, glRenderbufferStorage, (GLenum, GLenum, GLsizei, GLsizei));
F(void, glFramebufferRenderbuffer, (GLenum, GLenum, GLenum, GLuint));
static GLuint prog, xfb, vb;
static void test(const char *what, GLenum type, GLboolean norm, int stride, const void *data, int bytes)
{
   glBindBuffer_(GL_ARRAY_BUFFER, vb);
   glBufferData_(GL_ARRAY_BUFFER, bytes, data, GL_STATIC_DRAW);
   glVertexAttribPointer_(0, 4, type, norm, stride, 0);
   glEnableVertexAttribArray_(0);
   glBindBuffer_(GL_TRANSFORM_FEEDBACK_BUFFER, xfb);
   glBufferData_(GL_TRANSFORM_FEEDBACK_BUFFER, 2 * 16, NULL, GL_STATIC_READ);
   glBindBufferBase_(GL_TRANSFORM_FEEDBACK_BUFFER, 0, xfb);
   glBeginTransformFeedback_(GL_POINTS);
   glDrawArrays_(GL_POINTS, 0, 2);
   glEndTransformFeedback_();
   const float *m = glMapBufferRange_(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 32, GL_MAP_READ_BIT);
   printf("%-28s err=%#x:", what, glGetError_());
   for (int i = 0; m && i < 8; i++) printf(" %g", m[i]);
   printf("\n");
   glUnmapBuffer_(GL_TRANSFORM_FEEDBACK_BUFFER);
}
int main(void)
{
   void *egl = dlopen("libEGL.so.1", RTLD_NOW);
#define L(n) n##_ = dlsym(egl, #n)
   L(eglGetPlatformDisplay); L(eglInitialize); L(eglBindAPI); L(eglCreateContext); L(eglMakeCurrent); L(eglGetProcAddress);
   EGLDisplay d = eglGetPlatformDisplay_(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
   eglInitialize_(d, NULL, NULL); eglBindAPI_(EGL_OPENGL_ES_API);
   const EGLint cx[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE};
   EGLContext c = eglCreateContext_(d, NULL, EGL_NO_CONTEXT, cx);
   if (!c || !eglMakeCurrent_(d, EGL_NO_SURFACE, EGL_NO_SURFACE, c)) { printf("context failed\n"); return 1; }
#define P(n) n##_ = eglGetProcAddress_(#n)
   P(glGenBuffers); P(glBindBuffer); P(glBufferData); P(glBindBufferBase); P(glCreateShader); P(glShaderSource);
   P(glCompileShader); P(glCreateProgram); P(glAttachShader); P(glLinkProgram); P(glUseProgram);
   P(glTransformFeedbackVaryings); P(glBindAttribLocation); P(glVertexAttribPointer); P(glEnableVertexAttribArray);
   P(glBeginTransformFeedback); P(glEndTransformFeedback); P(glEnable); P(glDrawArrays); P(glMapBufferRange);
   P(glUnmapBuffer); P(glGetError); P(glGenVertexArrays); P(glBindVertexArray);
   P(glGenFramebuffers); P(glBindFramebuffer); P(glGenRenderbuffers); P(glBindRenderbuffer);
   P(glRenderbufferStorage); P(glFramebufferRenderbuffer);
   GLuint fbo, rb; glGenFramebuffers_(1, &fbo); glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
   glGenRenderbuffers_(1, &rb); glBindRenderbuffer_(GL_RENDERBUFFER, rb);
   glRenderbufferStorage_(GL_RENDERBUFFER, GL_RGBA8, 1, 1);
   glFramebufferRenderbuffer_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
   const char *vs = "#version 300 es\nin vec4 a; out vec4 o; void main(){ o = a; gl_Position = vec4(0.0); gl_PointSize = 1.0; }";
   const char *fs = "#version 300 es\nprecision mediump float; out vec4 c; void main(){ c = vec4(1.0); }";
   GLuint v = glCreateShader_(GL_VERTEX_SHADER), f = glCreateShader_(GL_FRAGMENT_SHADER);
   glShaderSource_(v, 1, &vs, NULL); glCompileShader_(v); glShaderSource_(f, 1, &fs, NULL); glCompileShader_(f);
   prog = glCreateProgram_(); glAttachShader_(prog, v); glAttachShader_(prog, f);
   glBindAttribLocation_(prog, 0, "a");
   const char *var = "o"; glTransformFeedbackVaryings_(prog, 1, &var, GL_INTERLEAVED_ATTRIBS);
   glLinkProgram_(prog); glUseProgram_(prog);
   GLuint vao; glGenVertexArrays_(1, &vao); glBindVertexArray_(vao);
   glGenBuffers_(1, &vb); glGenBuffers_(1, &xfb);
   glEnable_(GL_RASTERIZER_DISCARD);
   /* x=1 y=2 z=-3 w=1 ; second vertex x=-1 y=511 z=0 w=-1 */
   uint32_t s[2] = {(1u << 30) | ((0x400u - 3) << 20) | (2u << 10) | 1u,
                    (3u << 30) | (0u << 20) | (511u << 10) | 0x3ffu};
   test("INT_2_10_10_10 1,2,-3,1|-1,511,0,-1", GL_INT_2_10_10_10_REV, GL_FALSE, 4, s, 8);
   uint32_t s16[8] = {s[0], 0, 0, 0, s[1], 0, 0, 0};
   test("  same, stride 16", GL_INT_2_10_10_10_REV, GL_FALSE, 16, s16, 32);
   uint32_t u[2] = {(2u << 30) | (3u << 20) | (2u << 10) | 1u, (3u << 30) | (1023u << 20) | (0u << 10) | 5u};
   test("UINT_2_10_10_10 1,2,3,2|5,0,1023,3", GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, 4, u, 8);
   test("  normalized", GL_UNSIGNED_INT_2_10_10_10_REV, GL_TRUE, 4, u, 8);
   float fl[8] = {1, 2, 3, 4, 5, 6, 7, 8};
   test("FLOAT control 1..8", GL_FLOAT, GL_FALSE, 16, fl, 32);
   return 0;
}
