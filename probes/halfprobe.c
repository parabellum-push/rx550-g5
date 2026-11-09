/* halfprobe: CL_R/CL_HALF_FLOAT image with NaN payloads: create (COPY_HOST_PTR) / write / read / copy-to-buffer paths. */
#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CK(x) do { cl_int e_ = (x); if (e_) { printf("CL error %d at line %d\n", e_, __LINE__); exit(100); } } while (0)
enum { W = 512, H = 512 };
static int cmp(const char *what, const uint16_t *a, const uint16_t *b)
{
   int bad = 0;
   for (int i = 0; i < W * H; i++)
      if (a[i] != b[i] && bad++ < 4) printf("  %s [%d] exp %04x got %04x\n", what, i, a[i], b[i]);
   printf("%s %s: %d bad\n", bad ? "FAIL" : "PASS", what, bad);
   return !!bad;
}
int main(int argc, char **argv)
{
   cl_platform_id p; cl_device_id d; cl_int err;
   CK(clGetPlatformIDs(1, &p, NULL)); CK(clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 1, &d, NULL));
   cl_context c = clCreateContext(NULL, 1, &d, NULL, NULL, &err); CK(err);
   cl_command_queue q = clCreateCommandQueueWithProperties(c, d, NULL, &err); CK(err);
   cl_image_format fmt = {CL_R, argc > 1 ? CL_UNSIGNED_INT16 : CL_HALF_FLOAT};
   cl_image_desc desc = {CL_MEM_OBJECT_IMAGE2D, W, H};
   static uint16_t src[W * H], out[W * H], src2[W * H];
   for (int i = 0; i < W * H; i++) src[i] = (uint16_t)(i * 0x9e37u + 0x7c01u);   /* many NaNs / Infs / normals */
   for (int i = 0; i < 16; i++) src[i] = (uint16_t[]){0x7fca, 0x7e22, 0xff2b, 0xfffe, 0x7c01, 0xfc01, 0x7e00, 0xffff,
                                                       0x7c00, 0xfc00, 0x3c00, 0x0001, 0x8001, 0x0000, 0x8000, 0x7bff}[i];
   int f = 0; size_t org[3] = {0}, reg[3] = {W, H, 1};
   cl_mem img = clCreateImage(c, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &fmt, &desc, src, &err); CK(err);
   memset(out, 0, sizeof out);
   CK(clEnqueueReadImage(q, img, CL_TRUE, org, reg, 0, 0, out, 0, NULL, NULL));
   f += cmp("create(COPY_HOST_PTR)+ReadImage", src, out);
   cl_mem img2 = clCreateImage(c, CL_MEM_READ_WRITE, &fmt, &desc, NULL, &err); CK(err);
   CK(clEnqueueWriteImage(q, img2, CL_TRUE, org, reg, 0, 0, src, 0, NULL, NULL));
   memset(out, 0, sizeof out);
   CK(clEnqueueReadImage(q, img2, CL_TRUE, org, reg, 0, 0, out, 0, NULL, NULL));
   f += cmp("WriteImage+ReadImage", src, out);
   cl_mem buf = clCreateBuffer(c, CL_MEM_READ_WRITE, sizeof out, NULL, &err); CK(err);
   CK(clEnqueueCopyImageToBuffer(q, img2, buf, org, reg, 0, 0, NULL, NULL));
   memset(out, 0, sizeof out);
   CK(clEnqueueReadBuffer(q, buf, CL_TRUE, 0, sizeof out, out, 0, NULL, NULL));
   f += cmp("CopyImageToBuffer", src, out);
   cl_mem img3 = clCreateImage(c, CL_MEM_READ_WRITE, &fmt, &desc, NULL, &err); CK(err);
   CK(clEnqueueWriteBuffer(q, buf, CL_TRUE, 0, sizeof src, src, 0, NULL, NULL));
   CK(clEnqueueCopyBufferToImage(q, buf, img3, 0, org, reg, 0, NULL, NULL));
   memset(out, 0, sizeof out);
   CK(clEnqueueReadImage(q, img3, CL_TRUE, org, reg, 0, 0, out, 0, NULL, NULL));
   f += cmp("CopyBufferToImage+ReadImage", src, out);
   cl_mem img4 = clCreateImage(c, CL_MEM_READ_WRITE, &fmt, &desc, NULL, &err); CK(err);
   CK(clEnqueueCopyImage(q, img2, img4, org, org, reg, 0, NULL, NULL));
   memset(out, 0, sizeof out);
   CK(clEnqueueReadImage(q, img4, CL_TRUE, org, reg, 0, 0, out, 0, NULL, NULL));
   f += cmp("CopyImage+ReadImage", src, out);
   memcpy(src2, src, sizeof src);
   cl_mem img5 = clCreateImage(c, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, &fmt, &desc, src2, &err); CK(err);
   CK(clEnqueueCopyImageToBuffer(q, img5, buf, org, reg, 0, 0, NULL, NULL));
   memset(out, 0, sizeof out);
   CK(clEnqueueReadBuffer(q, buf, CL_TRUE, 0, sizeof out, out, 0, NULL, NULL));
   f += cmp("USE_HOST_PTR+CopyImageToBuffer", src, out);
   printf("failures: %d\n", f);
   return f;
}
