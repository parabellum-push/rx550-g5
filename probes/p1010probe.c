/* p1010probe: CL_RGBA / CL_UNORM_INT_101010_2 (R bits 31..22, G 21..12, B 11..2, A 1..0 of a host u32). */
#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define CK(x) do { cl_int e_ = (x); if (e_) { printf("CL error %d at line %d\n", e_, __LINE__); exit(100); } } while (0)
static const char *src =
"kernel void rd(read_only image2d_t im, global float4 *o) { int x = get_global_id(0); o[x] = read_imagef(im, (int2)(x, 0)); }\n"
"kernel void wr(write_only image2d_t im, global const float4 *v) { int x = get_global_id(0); write_imagef(im, (int2)(x, 0), v[x]); }\n";
enum { W = 8 };
int main(void)
{
   cl_platform_id p; cl_device_id d; cl_int err;
   CK(clGetPlatformIDs(1, &p, NULL)); CK(clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 1, &d, NULL));
   cl_context c = clCreateContext(NULL, 1, &d, NULL, NULL, &err); CK(err);
   cl_command_queue q = clCreateCommandQueueWithProperties(c, d, NULL, &err); CK(err);
   cl_program pr = clCreateProgramWithSource(c, 1, &src, NULL, &err); CK(err);
   CK(clBuildProgram(pr, 1, &d, "", NULL, NULL));
   cl_image_format fmt = {CL_RGBA, CL_UNORM_INT_101010_2};
   cl_image_desc desc = {CL_MEM_OBJECT_IMAGE2D, W, 1};
   uint32_t px[W]; float ref[W][4], out[W][4];
   for (int i = 0; i < W; i++) {
      uint32_t r = (100 + 97 * i) & 0x3ff, g = (900 - 61 * i) & 0x3ff, b = (17 + 131 * i) & 0x3ff, a = i & 3;
      px[i] = r << 22 | g << 12 | b << 2 | a;
      ref[i][0] = r / 1023.f; ref[i][1] = g / 1023.f; ref[i][2] = b / 1023.f; ref[i][3] = a / 3.f;
   }
   int f = 0;
   cl_mem im = clCreateImage(c, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &fmt, &desc, px, &err); CK(err);
   cl_mem bo = clCreateBuffer(c, CL_MEM_READ_WRITE, sizeof out, NULL, &err); CK(err);
   cl_kernel k = clCreateKernel(pr, "rd", &err); CK(err);
   CK(clSetKernelArg(k, 0, sizeof im, &im)); CK(clSetKernelArg(k, 1, sizeof bo, &bo));
   size_t gs = W;
   CK(clEnqueueNDRangeKernel(q, k, 1, NULL, &gs, NULL, 0, NULL, NULL));
   CK(clEnqueueReadBuffer(q, bo, CL_TRUE, 0, sizeof out, out, 0, NULL, NULL));
   for (int i = 0; i < W; i++)
      for (int ch = 0; ch < 4; ch++)
         if (fabsf(out[i][ch] - ref[i][ch]) > 1e-3f) { if (f < 4) printf("  rd px %d ch %d exp %f got %f\n", i, ch, ref[i][ch], out[i][ch]); f++; }
   printf("%s read_imagef: %d bad\n", f ? "FAIL" : "PASS", f);
   int g = 0;
   cl_mem im2 = clCreateImage(c, CL_MEM_READ_WRITE, &fmt, &desc, NULL, &err); CK(err);
   cl_mem bv = clCreateBuffer(c, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof ref, ref, &err); CK(err);
   cl_kernel k2 = clCreateKernel(pr, "wr", &err); CK(err);
   CK(clSetKernelArg(k2, 0, sizeof im2, &im2)); CK(clSetKernelArg(k2, 1, sizeof bv, &bv));
   CK(clEnqueueNDRangeKernel(q, k2, 1, NULL, &gs, NULL, 0, NULL, NULL));
   uint32_t got[W]; size_t org[3] = {0}, reg[3] = {W, 1, 1};
   CK(clEnqueueReadImage(q, im2, CL_TRUE, org, reg, 0, 0, got, 0, NULL, NULL));
   for (int i = 0; i < W; i++)
      if (got[i] != px[i]) { if (g < 4) printf("  wr px %d exp %08x got %08x\n", i, px[i], got[i]); g++; }
   printf("%s write_imagef: %d bad\n", g ? "FAIL" : "PASS", g);
   return f + g;
}
