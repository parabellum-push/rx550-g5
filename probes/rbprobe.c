/* rbprobe: isolate RGBA8 readback problems on the G5 (Mesa 26 BE port).
 * 1) upload known bytes to a texture, attach to FBO, glReadPixels
 * 2) glClear to a colour, glReadPixels
 * 3) draw a quad with a constant fragment colour, glReadPixels
 * Loads EGL/GLES via dlopen; build: powerpc64-linux-gnu-gcc -mcpu=970 -O1 rbprobe.c -ldl
 */
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#define F(ret, name, args) static ret (*p_##name) args
F(EGLDisplay, eglGetPlatformDisplay, (EGLenum, void *, const EGLAttrib *));
F(EGLBoolean, eglInitialize, (EGLDisplay, EGLint *, EGLint *));
F(EGLBoolean, eglBindAPI, (EGLenum));
F(EGLBoolean, eglChooseConfig, (EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *));
F(EGLContext, eglCreateContext, (EGLDisplay, EGLConfig, EGLContext, const EGLint *));
F(EGLBoolean, eglMakeCurrent, (EGLDisplay, EGLSurface, EGLSurface, EGLContext));
F(void *, eglGetProcAddress, (const char *));
F(EGLSurface, eglCreatePbufferSurface, (EGLDisplay, EGLConfig, const EGLint *));
F(EGLBoolean, eglGetConfigAttrib, (EGLDisplay, EGLConfig, EGLint, EGLint *));
#define G(ret, name, args) static ret (*name##_) args
G(void, glGenTextures, (GLsizei, GLuint *));
G(void, glBindTexture, (GLenum, GLuint));
G(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *));
G(void, glTexParameteri, (GLenum, GLenum, GLint));
G(void, glGenFramebuffers, (GLsizei, GLuint *));
G(void, glBindFramebuffer, (GLenum, GLuint));
G(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint));
G(GLenum, glCheckFramebufferStatus, (GLenum));
G(void, glReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *));
G(void, glClearColor, (GLfloat, GLfloat, GLfloat, GLfloat));
G(void, glClear, (GLbitfield));
G(void, glViewport, (GLint, GLint, GLsizei, GLsizei));
G(GLuint, glCreateShader, (GLenum));
G(void, glShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *));
G(void, glCompileShader, (GLuint));
G(GLuint, glCreateProgram, (void));
G(void, glAttachShader, (GLuint, GLuint));
G(void, glLinkProgram, (GLuint));
G(void, glUseProgram, (GLuint));
G(void, glBindAttribLocation, (GLuint, GLuint, const GLchar *));
G(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *));
G(void, glEnableVertexAttribArray, (GLuint));
G(void, glDrawArrays, (GLenum, GLint, GLsizei));
G(void, glFinish, (void));
G(GLenum, glGetError, (void));
G(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *));
G(GLint, glGetUniformLocation, (GLuint, const GLchar *));
G(void, glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat));

static void dump(const char *what)
{
   uint8_t px[4 * 4] = {0};
   glReadPixels_(0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, px);
   printf("%-34s err=%#x  px0=%3u %3u %3u %3u  px1=%3u %3u %3u %3u\n", what, glGetError_(),
          px[0], px[1], px[2], px[3], px[4], px[5], px[6], px[7]);
}

int main(int argc, char **argv)
{
   const int use_pbuffer = argc > 1;
   void *egl = dlopen("libEGL.so.1", RTLD_NOW);
   if (!egl) { printf("no libEGL: %s\n", dlerror()); return 1; }
#define L(n) p_##n = dlsym(egl, #n)
   L(eglGetPlatformDisplay); L(eglInitialize); L(eglBindAPI); L(eglChooseConfig);
   L(eglCreateContext); L(eglMakeCurrent); L(eglGetProcAddress); L(eglCreatePbufferSurface); L(eglGetConfigAttrib);
   EGLDisplay d = p_eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
   if (!p_eglInitialize(d, NULL, NULL)) { printf("eglInitialize failed\n"); return 1; }
   p_eglBindAPI(EGL_OPENGL_ES_API);
   const EGLint ca[] = {EGL_RENDERABLE_TYPE, 0x40 /* ES3 */, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_NONE};
   EGLConfig cfg; EGLint n = 0;
   p_eglChooseConfig(d, ca, &cfg, 1, &n);
   const EGLint cx[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
   EGLContext c = p_eglCreateContext(d, n ? cfg : NULL, EGL_NO_CONTEXT, cx);
   EGLSurface sf = EGL_NO_SURFACE;
   if (use_pbuffer) {
      const EGLint pa[] = {EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE};
      sf = p_eglCreatePbufferSurface(d, cfg, pa);
      EGLint vid = 0; p_eglGetConfigAttrib(d, cfg, EGL_NATIVE_VISUAL_ID, &vid);
      printf("pbuffer %p configs %d native visual %#x\n", sf, n, vid);
   }
   if (!c || !p_eglMakeCurrent(d, sf, sf, c)) { printf("context failed\n"); return 1; }
#define P(n) n##_ = p_eglGetProcAddress(#n)
   P(glGenTextures); P(glBindTexture); P(glTexImage2D); P(glTexParameteri); P(glGenFramebuffers);
   P(glBindFramebuffer); P(glFramebufferTexture2D); P(glCheckFramebufferStatus); P(glReadPixels);
   P(glClearColor); P(glClear); P(glViewport); P(glCreateShader); P(glShaderSource); P(glCompileShader);
   P(glCreateProgram); P(glAttachShader); P(glLinkProgram); P(glUseProgram); P(glBindAttribLocation);
   P(glVertexAttribPointer); P(glEnableVertexAttribArray); P(glDrawArrays); P(glFinish); P(glGetError);
   P(glGetShaderInfoLog); P(glGetUniformLocation); P(glUniform4f);

   const unsigned W = 64, H = 64;
   static uint8_t img[64 * 64 * 4];
   for (unsigned i = 0; i < W * H; i++) { img[i*4] = 10; img[i*4+1] = 20; img[i*4+2] = 30; img[i*4+3] = 40; }
   GLuint t, fb;
   glGenTextures_(1, &t); glBindTexture_(GL_TEXTURE_2D, t);
   glTexParameteri_(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexImage2D_(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
   glGenFramebuffers_(1, &fb); glBindFramebuffer_(GL_FRAMEBUFFER, use_pbuffer ? 0 : fb);
   if (!use_pbuffer)
      glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t, 0);
   printf("fbo status %#x (complete %#x)\n", glCheckFramebufferStatus_(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
   glViewport_(0, 0, W, H);
   dump("1 upload 10,20,30,40");

   glClearColor_(50 / 255.0f, 60 / 255.0f, 70 / 255.0f, 80 / 255.0f);
   glClear_(GL_COLOR_BUFFER_BIT);
   dump("2 clear 50,60,70,80");

   const char *vs = "attribute vec2 p; void main(){ gl_Position = vec4(p, 0.0, 1.0); }";
   const char *fs = "precision mediump float; uniform vec4 col; void main(){ gl_FragColor = col; }";
   GLuint v = glCreateShader_(GL_VERTEX_SHADER), f = glCreateShader_(GL_FRAGMENT_SHADER);
   glShaderSource_(v, 1, &vs, NULL); glCompileShader_(v);
   glShaderSource_(f, 1, &fs, NULL); glCompileShader_(f);
   GLuint pr = glCreateProgram_(); glAttachShader_(pr, v); glAttachShader_(pr, f);
   glBindAttribLocation_(pr, 0, "p"); glLinkProgram_(pr); glUseProgram_(pr);
   glUniform4f_(glGetUniformLocation_(pr, "col"), 90 / 255.0f, 100 / 255.0f, 110 / 255.0f, 120 / 255.0f);
   static const float q[] = {-1, -1, 1, -1, -1, 1, 1, 1};
   glVertexAttribPointer_(0, 2, GL_FLOAT, GL_FALSE, 0, q); glEnableVertexAttribArray_(0);
   glDrawArrays_(GL_TRIANGLE_STRIP, 0, 4);
   dump("3 draw uniform 90,100,110,120");
   /* 4) R11F_G11F_B10F render target */
   GLuint t2, fb2;
   glGenTextures_(1, &t2); glBindTexture_(GL_TEXTURE_2D, t2);
   glTexImage2D_(GL_TEXTURE_2D, 0, 0x8C3A /* GL_R11F_G11F_B10F */, W, H, 0, GL_RGB, GL_FLOAT, NULL);
   glGenFramebuffers_(1, &fb2); glBindFramebuffer_(GL_FRAMEBUFFER, fb2);
   glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t2, 0);
   printf("r11f fbo status %#x\n", glCheckFramebufferStatus_(GL_FRAMEBUFFER));
   glClearColor_(0.5f, 2.0f, 8.0f, 1.0f);
   glClear_(GL_COLOR_BUFFER_BIT);
   float fpx[4] = {0};
   glReadPixels_(0, 0, 1, 1, GL_RGBA, GL_FLOAT, fpx);
   printf("4 r11f clear .5,2,8 read FLOAT  err=%#x  %g %g %g %g\n", glGetError_(), fpx[0], fpx[1], fpx[2], fpx[3]);
   uint32_t packed = 0;
   glReadPixels_(0, 0, 1, 1, GL_RGB, 0x8C3B /* UNSIGNED_INT_10F_11F_11F_REV */, &packed);
   printf("4 r11f read packed err=%#x  %#010x (expect 0x%08x)\n", glGetError_(), packed, (0x88u << 22) | (0x400u << 11) | 0x380u);
   glUseProgram_(pr);
   glUniform4f_(glGetUniformLocation_(pr, "col"), 0.5f, 2.0f, 8.0f, 1.0f);
   glDrawArrays_(GL_TRIANGLE_STRIP, 0, 4);
   glReadPixels_(0, 0, 1, 1, GL_RGBA, GL_FLOAT, fpx);
   printf("5 r11f draw .5,2,8 read FLOAT  err=%#x  %g %g %g %g\n", glGetError_(), fpx[0], fpx[1], fpx[2], fpx[3]);
   return 0;
}
