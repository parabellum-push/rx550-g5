/* coordprobe: nearest + CLAMP (border) + unnormalized coords at x = -2^-k: border (0) or texel 0? */
#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define CK(x) do { cl_int e_ = (x); if (e_) { printf("CL error %d at line %d\n", e_, __LINE__); exit(100); } } while (0)
static const char *src =
"constant sampler_t s = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;\n"
"kernel void k(read_only image2d_t im, global const float *x, global uint4 *o) {\n"
"  int i = get_global_id(0); o[i] = read_imageui(im, s, (float2)(x[i], 0.5f)); o[i].w = as_uint(x[i]); }\n";
int main(void)
{
   cl_platform_id p; cl_device_id d; cl_int err;
   CK(clGetPlatformIDs(1, &p, NULL)); CK(clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 1, &d, NULL));
   cl_context c = clCreateContext(NULL, 1, &d, NULL, NULL, &err); CK(err);
   cl_command_queue q = clCreateCommandQueueWithProperties(c, d, NULL, &err); CK(err);
   cl_program pr = clCreateProgramWithSource(c, 1, &src, NULL, &err); CK(err);
   CK(clBuildProgram(pr, 1, &d, "", NULL, NULL));
   cl_image_format fmt = {CL_RGBA, CL_UNSIGNED_INT8};
   cl_image_desc desc = {CL_MEM_OBJECT_IMAGE2D, 4, 1};
   uint8_t px[16] = {7, 7, 7, 7, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3};
   enum { N = 32 };
   float x[N]; uint32_t o[N][4];
   for (int i = 0; i < N; i++) x[i] = -ldexpf(1.0f, -(i + 1));   /* -0.5 ... -2^-32 */
   cl_mem im = clCreateImage(c, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &fmt, &desc, px, &err); CK(err);
   cl_mem bx = clCreateBuffer(c, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof x, x, &err); CK(err);
   cl_mem bo = clCreateBuffer(c, CL_MEM_WRITE_ONLY, sizeof o, NULL, &err); CK(err);
   cl_kernel k = clCreateKernel(pr, "k", &err); CK(err);
   CK(clSetKernelArg(k, 0, sizeof im, &im)); CK(clSetKernelArg(k, 1, sizeof bx, &bx)); CK(clSetKernelArg(k, 2, sizeof bo, &bo));
   size_t gs = N;
   CK(clEnqueueNDRangeKernel(q, k, 1, NULL, &gs, NULL, 0, NULL, NULL));
   CK(clEnqueueReadBuffer(q, bo, CL_TRUE, 0, sizeof o, o, 0, NULL, NULL));
   for (int i = 0; i < N; i++) {
      union { float f; uint32_t u; } v = {x[i]};
      printf("x = -2^-%-2d (%08x, kernel saw %08x): %s\n", i + 1, v.u, o[i][3], o[i][0] == 7 ? "texel 0" : o[i][0] == 0 ? "border" : "other");
   }
   return 0;
}
