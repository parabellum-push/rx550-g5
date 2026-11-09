/* cpprobe: glCopyImageSubData between RGB9E5 and R32UI (raw 32-bit copies). EGL_PLATFORM=surfaceless. */
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
F(void, glTexStorage2D, (GLenum, GLsizei, GLenum, GLsizei, GLsizei));
F(void, glTexStorage3D, (GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei));
F(void, glTexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *));
F(void, glGenFramebuffers, (GLsizei, GLuint *));
F(void, glBindFramebuffer, (GLenum, GLuint));
F(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint));
F(void, glFramebufferTextureLayer, (GLenum, GLenum, GLuint, GLint, GLint));
F(void, glReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *));
F(void, glCopyImageSubData, (GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei));
F(GLenum, glGetError, (void));
static GLuint fb;
static void read_u32(const char *what, GLuint t, GLenum target)
{
   uint32_t px[4] = {0};
   glBindFramebuffer_(GL_FRAMEBUFFER, fb);
   if (target == GL_TEXTURE_3D) glFramebufferTextureLayer_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, t, 0, 0);
   else glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t, 0);
   glReadPixels_(0, 0, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT, px);
   printf("%-34s err=%#x  %08x\n", what, glGetError_(), px[0]);
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
   P(glGenTextures); P(glBindTexture); P(glTexStorage2D); P(glTexStorage3D); P(glTexSubImage2D); P(glGenFramebuffers);
   P(glBindFramebuffer); P(glFramebufferTexture2D); P(glFramebufferTextureLayer); P(glReadPixels); P(glCopyImageSubData);
   P(glGetError);
   glGenFramebuffers_(1, &fb);
   GLuint e, u, u2, e2, u3;
   glGenTextures_(1, &e); glBindTexture_(GL_TEXTURE_2D, e); glTexStorage2D_(GL_TEXTURE_2D, 1, GL_RGB9_E5, 4, 4);
   glGenTextures_(1, &u); glBindTexture_(GL_TEXTURE_2D, u); glTexStorage2D_(GL_TEXTURE_2D, 1, GL_R32UI, 4, 4);
   glGenTextures_(1, &u2); glBindTexture_(GL_TEXTURE_2D, u2); glTexStorage2D_(GL_TEXTURE_2D, 1, GL_R32UI, 4, 4);
   glGenTextures_(1, &e2); glBindTexture_(GL_TEXTURE_2D, e2); glTexStorage2D_(GL_TEXTURE_2D, 1, GL_RGB9_E5, 4, 4);
   glGenTextures_(1, &u3); glBindTexture_(GL_TEXTURE_3D, u3); glTexStorage3D_(GL_TEXTURE_3D, 1, GL_R32UI, 4, 4, 1);
   uint32_t w[16]; for (int i = 0; i < 16; i++) w[i] = 0x12345678u;
   glBindTexture_(GL_TEXTURE_2D, e);
   glTexSubImage2D_(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, w);
   printf("upload rgb9e5 word 0x12345678 err=%#x\n", glGetError_());
   glCopyImageSubData_(e, GL_TEXTURE_2D, 0, 0, 0, 0, u, GL_TEXTURE_2D, 0, 0, 0, 0, 4, 4, 1);
   read_u32("rgb9e5 -> r32ui 2D", u, GL_TEXTURE_2D);
   glCopyImageSubData_(e, GL_TEXTURE_2D, 0, 0, 0, 0, u3, GL_TEXTURE_3D, 0, 0, 0, 0, 4, 4, 1);
   read_u32("rgb9e5 -> r32ui 3D", u3, GL_TEXTURE_3D);
   glBindTexture_(GL_TEXTURE_2D, u);
   glTexSubImage2D_(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RED_INTEGER, GL_UNSIGNED_INT, w);
   read_u32("r32ui upload 0x12345678", u, GL_TEXTURE_2D);
   glCopyImageSubData_(u, GL_TEXTURE_2D, 0, 0, 0, 0, e2, GL_TEXTURE_2D, 0, 0, 0, 0, 4, 4, 1);
   glCopyImageSubData_(e2, GL_TEXTURE_2D, 0, 0, 0, 0, u2, GL_TEXTURE_2D, 0, 0, 0, 0, 4, 4, 1);
   read_u32("r32ui -> rgb9e5 -> r32ui", u2, GL_TEXTURE_2D);
   return 0;
}
