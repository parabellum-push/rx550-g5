/* ibprobe: GLES 3.1 image/texel buffer load/store on the G5 (Mesa 26 BE port).
 * Source buffer texture RGBA32UI with known words; three compute copies:
 *   A imageLoad  -> imageStore   (dst image buffer)
 *   B texelFetch -> imageStore
 *   C imageLoad  -> SSBO uint[]
 * Run with EGL_PLATFORM=surfaceless. build: powerpc64-linux-gnu-gcc -mcpu=970 -O1 ibprobe.c -ldl
 */
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>
#define GL_TEXTURE_BUFFER 0x8C2A

#define F(ret, name, args) static ret (*name##_) args
F(EGLDisplay, eglGetPlatformDisplay, (EGLenum, void *, const EGLAttrib *));
F(EGLBoolean, eglInitialize, (EGLDisplay, EGLint *, EGLint *));
F(EGLBoolean, eglBindAPI, (EGLenum));
F(EGLBoolean, eglChooseConfig, (EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *));
F(EGLContext, eglCreateContext, (EGLDisplay, EGLConfig, EGLContext, const EGLint *));
F(EGLBoolean, eglMakeCurrent, (EGLDisplay, EGLSurface, EGLSurface, EGLContext));
F(void *, eglGetProcAddress, (const char *));
F(void, glGenBuffers, (GLsizei, GLuint *));
F(void, glBindBuffer, (GLenum, GLuint));
F(void, glBufferData, (GLenum, GLsizeiptr, const void *, GLenum));
F(void, glGenTextures, (GLsizei, GLuint *));
F(void, glBindTexture, (GLenum, GLuint));
F(void, glActiveTexture, (GLenum));
F(void, glTexBuffer, (GLenum, GLenum, GLuint));
F(void, glBindImageTexture, (GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLenum));
F(void, glBindBufferBase, (GLenum, GLuint, GLuint));
F(GLuint, glCreateShader, (GLenum));
F(void, glShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *));
F(void, glCompileShader, (GLuint));
F(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *));
F(GLuint, glCreateProgram, (void));
F(void, glAttachShader, (GLuint, GLuint));
F(void, glLinkProgram, (GLuint));
F(void, glUseProgram, (GLuint));
F(void, glDispatchCompute, (GLuint, GLuint, GLuint));
F(void, glMemoryBarrier, (GLbitfield));
F(void *, glMapBufferRange, (GLenum, GLintptr, GLsizeiptr, GLbitfield));
F(GLboolean, glUnmapBuffer, (GLenum));
F(void, glFinish, (void));
F(GLenum, glGetError, (void));
F(const GLubyte *, glGetString, (GLenum));

#define N 4
static GLuint prog(const char *src)
{
   GLuint s = glCreateShader_(GL_COMPUTE_SHADER);
   glShaderSource_(s, 1, &src, NULL);
   glCompileShader_(s);
   char log[2048] = {0};
   glGetShaderInfoLog_(s, sizeof(log), NULL, log);
   if (log[0]) printf("shader log: %s\n", log);
   GLuint p = glCreateProgram_();
   glAttachShader_(p, s);
   glLinkProgram_(p);
   return p;
}

static void dump(const char *what, GLenum target, GLuint buf)
{
   glBindBuffer_(target, buf);
   const uint32_t *m = glMapBufferRange_(target, 0, N * 16, GL_MAP_READ_BIT);
   printf("%-28s err=%#x:", what, glGetError_());
   for (unsigned i = 0; m && i < 8; i++) printf(" %08x", m[i]);
   printf("\n");
   glUnmapBuffer_(target);
}

int main(void)
{
   void *egl = dlopen("libEGL.so.1", RTLD_NOW);
#define L(n) n##_ = dlsym(egl, #n)
   L(eglGetPlatformDisplay); L(eglInitialize); L(eglBindAPI); L(eglChooseConfig);
   L(eglCreateContext); L(eglMakeCurrent); L(eglGetProcAddress);
   EGLDisplay d = eglGetPlatformDisplay_(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
   eglInitialize_(d, NULL, NULL);
   eglBindAPI_(EGL_OPENGL_ES_API);
   const EGLint cx[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 1, EGL_NONE};
   EGLContext c = eglCreateContext_(d, NULL, EGL_NO_CONTEXT, cx);
   if (!c || !eglMakeCurrent_(d, EGL_NO_SURFACE, EGL_NO_SURFACE, c)) { printf("context failed\n"); return 1; }
#define P(n) n##_ = eglGetProcAddress_(#n)
   P(glGenBuffers); P(glBindBuffer); P(glBufferData); P(glGenTextures); P(glBindTexture); P(glActiveTexture);
   P(glTexBuffer); P(glBindImageTexture); P(glBindBufferBase); P(glCreateShader); P(glShaderSource);
   P(glCompileShader); P(glGetShaderInfoLog); P(glCreateProgram); P(glAttachShader); P(glLinkProgram);
   P(glUseProgram); P(glDispatchCompute); P(glMemoryBarrier); P(glMapBufferRange); P(glUnmapBuffer);
   P(glFinish); P(glGetError); P(glGetString);
   printf("renderer: %s\n", glGetString_(GL_RENDERER));

   uint32_t init[N * 4];
   for (unsigned i = 0; i < N * 4; i++) init[i] = 0x11223300u + i;
   GLuint b[4], t[3];
   glGenBuffers_(4, b);
   glGenTextures_(3, t);
   for (unsigned i = 0; i < 3; i++) {
      glBindBuffer_(GL_TEXTURE_BUFFER, b[i]);
      glBufferData_(GL_TEXTURE_BUFFER, sizeof(init), i == 0 ? init : NULL, GL_STATIC_DRAW);
      glBindTexture_(GL_TEXTURE_BUFFER, t[i]);
      glTexBuffer_(GL_TEXTURE_BUFFER, GL_RGBA32UI, b[i]);
   }
   glBindBuffer_(GL_SHADER_STORAGE_BUFFER, b[3]);
   glBufferData_(GL_SHADER_STORAGE_BUFFER, sizeof(init), NULL, GL_STATIC_DRAW);
   dump("source", GL_TEXTURE_BUFFER, b[0]);

   const char *ha = "#version 310 es\n#extension GL_EXT_texture_buffer : require\nlayout(local_size_x=1) in;\n"
      "layout(rgba32ui, binding=0) readonly uniform highp uimageBuffer src;\n"
      "layout(rgba32ui, binding=1) writeonly uniform highp uimageBuffer dst;\n"
      "void main(){ int i=int(gl_GlobalInvocationID.x); imageStore(dst, i, imageLoad(src, i)); }\n";
   const char *hb = "#version 310 es\n#extension GL_EXT_texture_buffer : require\nlayout(local_size_x=1) in;\n"
      "uniform highp usamplerBuffer srct;\n"
      "layout(rgba32ui, binding=1) writeonly uniform highp uimageBuffer dst;\n"
      "void main(){ int i=int(gl_GlobalInvocationID.x); imageStore(dst, i, texelFetch(srct, i)); }\n";
   const char *hc = "#version 310 es\n#extension GL_EXT_texture_buffer : require\nlayout(local_size_x=1) in;\n"
      "layout(rgba32ui, binding=0) readonly uniform highp uimageBuffer src;\n"
      "layout(std430, binding=0) buffer O { uint o[]; };\n"
      "void main(){ int i=int(gl_GlobalInvocationID.x); uvec4 v=imageLoad(src, i);\n"
      " o[4*i]=v.x; o[4*i+1]=v.y; o[4*i+2]=v.z; o[4*i+3]=v.w; }\n";

   glBindImageTexture_(0, t[0], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32UI);
   glBindImageTexture_(1, t[1], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32UI);
   glUseProgram_(prog(ha));
   glDispatchCompute_(N, 1, 1);
   glMemoryBarrier_(GL_ALL_BARRIER_BITS);
   dump("A imageLoad->imageStore", GL_TEXTURE_BUFFER, b[1]);

   glActiveTexture_(GL_TEXTURE0);
   glBindTexture_(GL_TEXTURE_BUFFER, t[0]);
   glBindImageTexture_(1, t[2], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32UI);
   glUseProgram_(prog(hb));
   glDispatchCompute_(N, 1, 1);
   glMemoryBarrier_(GL_ALL_BARRIER_BITS);
   dump("B texelFetch->imageStore", GL_TEXTURE_BUFFER, b[2]);

   glBindBufferBase_(GL_SHADER_STORAGE_BUFFER, 0, b[3]);
   glUseProgram_(prog(hc));
   glDispatchCompute_(N, 1, 1);
   glMemoryBarrier_(GL_ALL_BARRIER_BITS);
   dump("C imageLoad->SSBO", GL_SHADER_STORAGE_BUFFER, b[3]);
   return 0;
}
