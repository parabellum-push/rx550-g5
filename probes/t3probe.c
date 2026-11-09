/* t3probe: 3D R32UI texture upload -> FBO layer readback, then compute imageAtomicAdd / imageStore.
 * EGL_PLATFORM=surfaceless. build: powerpc64-linux-gnu-gcc -mcpu=970 -O1 t3probe.c -ldl */
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>
#define F(ret, name, args) static ret (*name##_) args
F(EGLDisplay, eglGetPlatformDisplay, (EGLenum, void *, const EGLAttrib *));
F(EGLBoolean, eglInitialize, (EGLDisplay, EGLint *, EGLint *));
F(EGLBoolean, eglBindAPI, (EGLenum));
F(EGLContext, eglCreateContext, (EGLDisplay, EGLConfig, EGLContext, const EGLint *));
F(EGLBoolean, eglMakeCurrent, (EGLDisplay, EGLSurface, EGLSurface, EGLContext));
F(void *, eglGetProcAddress, (const char *));
F(void, glGenTextures, (GLsizei, GLuint *));
F(void, glBindTexture, (GLenum, GLuint));
F(void, glTexStorage3D, (GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei));
F(void, glTexSubImage3D, (GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void *));
F(void, glGenFramebuffers, (GLsizei, GLuint *));
F(void, glBindFramebuffer, (GLenum, GLuint));
F(void, glFramebufferTextureLayer, (GLenum, GLenum, GLuint, GLint, GLint));
F(void, glReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *));
F(void, glBindImageTexture, (GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLenum));
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
F(GLenum, glGetError, (void));
F(GLenum, glCheckFramebufferStatus, (GLenum));
#define W 64
#define D 8
static GLuint fb;
static void readlayer(const char *what, GLuint t, int layer)
{
   uint32_t px[W * W * 4] = {0};
   glBindFramebuffer_(GL_FRAMEBUFFER, fb);
   glFramebufferTextureLayer_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, t, 0, layer);
   GLenum st = glCheckFramebufferStatus_(GL_FRAMEBUFFER);
   glReadPixels_(0, 0, W, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT, px);
   printf("%-26s L%d fb=%#x err=%#x:", what, layer, st, glGetError_());
   for (unsigned i = 0; i < W; i++) printf(" %08x", px[i * 4]);
   printf("\n");
}
static void run(const char *src)
{
   GLuint s = glCreateShader_(GL_COMPUTE_SHADER);
   glShaderSource_(s, 1, &src, NULL); glCompileShader_(s);
   char log[2048] = {0}; glGetShaderInfoLog_(s, sizeof(log), NULL, log); if (log[0]) printf("log: %s\n", log);
   GLuint p = glCreateProgram_(); glAttachShader_(p, s); glLinkProgram_(p); glUseProgram_(p);
   glDispatchCompute_(W, W, D);
   glMemoryBarrier_(GL_ALL_BARRIER_BITS);
}
int main(void)
{
   void *egl = dlopen("libEGL.so.1", RTLD_NOW);
#define L(n) n##_ = dlsym(egl, #n)
   L(eglGetPlatformDisplay); L(eglInitialize); L(eglBindAPI); L(eglCreateContext); L(eglMakeCurrent); L(eglGetProcAddress);
   EGLDisplay d = eglGetPlatformDisplay_(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
   eglInitialize_(d, NULL, NULL); eglBindAPI_(EGL_OPENGL_ES_API);
   const EGLint cx[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 1, EGL_NONE};
   EGLContext c = eglCreateContext_(d, NULL, EGL_NO_CONTEXT, cx);
   if (!c || !eglMakeCurrent_(d, EGL_NO_SURFACE, EGL_NO_SURFACE, c)) { printf("context failed\n"); return 1; }
#define P(n) n##_ = eglGetProcAddress_(#n)
   P(glGenTextures); P(glBindTexture); P(glTexStorage3D); P(glTexSubImage3D); P(glGenFramebuffers); P(glBindFramebuffer);
   P(glFramebufferTextureLayer); P(glReadPixels); P(glBindImageTexture); P(glCreateShader); P(glShaderSource);
   P(glCompileShader); P(glGetShaderInfoLog); P(glCreateProgram); P(glAttachShader); P(glLinkProgram); P(glUseProgram);
   P(glDispatchCompute); P(glMemoryBarrier); P(glGetError); P(glCheckFramebufferStatus);
   static uint32_t data[W * W * D];
   for (unsigned i = 0; i < W * W * D; i++) data[i] = 0x11220000u + i;
   GLuint t[2];
   glGenTextures_(2, t); glGenFramebuffers_(1, &fb);
   for (int k = 0; k < 2; k++) {
      glBindTexture_(GL_TEXTURE_3D, t[k]);
      glTexStorage3D_(GL_TEXTURE_3D, 1, GL_R32UI, W, W, D);
      glTexSubImage3D_(GL_TEXTURE_3D, 0, 0, 0, 0, W, W, D, GL_RED_INTEGER, GL_UNSIGNED_INT, data);
   }
   for (int l = 0; l < D; l++) readlayer("upload", t[0], l);
   glBindImageTexture_(0, t[0], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
   run("#version 310 es\n#extension GL_OES_shader_image_atomic : require\nlayout(local_size_x=1) in;\nlayout(r32ui, binding=0) uniform highp uimage3D img;\n"
       "void main(){ imageAtomicAdd(img, ivec3(gl_GlobalInvocationID), 0x100u); }\n");
   for (int l = 0; l < D; l++) readlayer("atomicAdd 0x100", t[0], l);
   glBindImageTexture_(0, t[1], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32UI);
   run("#version 310 es\nlayout(local_size_x=1) in;\nlayout(r32ui, binding=0) writeonly uniform highp uimage3D img;\n"
       "void main(){ ivec3 p=ivec3(gl_GlobalInvocationID); imageStore(img, p, uvec4(0xAB000000u + uint(p.z*256 + p.y*16 + p.x))); }\n");
   for (int l = 0; l < D; l++) readlayer("store AB00zyx", t[1], l);
   return 0;
}
