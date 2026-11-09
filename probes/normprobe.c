/* normprobe: image1d_buffer (BE shader decode) vs image1d (texture unit) read_imagef, every 8/16-bit value. */
#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CK(x) do { cl_int e_ = (x); if (e_) { printf("CL error %d at line %d\n", e_, __LINE__); exit(100); } } while (0)
static const char *src =
"constant sampler_t s = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;\n"
"kernel void k(read_only image1d_buffer_t a, read_only image1d_t b, global uint2 *o) {\n"
"  int x = get_global_id(0); o[x] = (uint2)(as_uint(read_imagef(a, x).x), as_uint(read_imagef(b, s, x).x)); }\n";
int main(void)
{
   cl_platform_id p; cl_device_id d; cl_int err;
   CK(clGetPlatformIDs(1, &p, NULL)); CK(clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 1, &d, NULL));
   cl_context c = clCreateContext(NULL, 1, &d, NULL, NULL, &err); CK(err);
   cl_command_queue q = clCreateCommandQueueWithProperties(c, d, NULL, &err); CK(err);
   cl_program pr = clCreateProgramWithSource(c, 1, &src, NULL, &err); CK(err);
   CK(clBuildProgram(pr, 1, &d, "", NULL, NULL));
   cl_kernel k = clCreateKernel(pr, "k", &err); CK(err);
   struct { cl_channel_type t; int n; const char *nm; } fm[] = {
      {CL_UNORM_INT8, 256, "UNORM8"}, {CL_SNORM_INT8, 256, "SNORM8"}, {CL_UNORM_INT16, 16384, "UNORM16"}, {CL_SNORM_INT16, 16384, "SNORM16"}};
   for (int f = 0; f < 4; f++) {
      int n = fm[f].n, sz = n == 256 ? 1 : 2;
      #define V16(i) ((uint16_t)((i) * 4 + ((i) & 3)))
      unsigned char *data = malloc((size_t)n * sz); uint32_t *o = malloc((size_t)n * 8);
      for (int i = 0; i < n; i++) { if (sz == 1) data[i] = i; else { uint16_t v = V16(i); memcpy(data + 2 * i, &v, 2); } }
      cl_image_format fmt = {CL_R, fm[f].t};
      cl_mem buf = clCreateBuffer(c, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, (size_t)n * sz, data, &err); CK(err);
      cl_image_desc da = {CL_MEM_OBJECT_IMAGE1D_BUFFER, n}; da.buffer = buf;
      cl_mem a = clCreateImage(c, CL_MEM_READ_ONLY, &fmt, &da, NULL, &err); CK(err);
      cl_image_desc db = {CL_MEM_OBJECT_IMAGE1D, n};
      cl_mem bimg = clCreateImage(c, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &fmt, &db, data, &err); CK(err);
      cl_mem bo = clCreateBuffer(c, CL_MEM_WRITE_ONLY, (size_t)n * 8, NULL, &err); CK(err);
      CK(clSetKernelArg(k, 0, sizeof a, &a)); CK(clSetKernelArg(k, 1, sizeof bimg, &bimg)); CK(clSetKernelArg(k, 2, sizeof bo, &bo));
      size_t gs = n; CK(clEnqueueNDRangeKernel(q, k, 1, NULL, &gs, NULL, 0, NULL, NULL));
      CK(clEnqueueReadBuffer(q, bo, CL_TRUE, 0, (size_t)n * 8, o, 0, NULL, NULL));
      int diff = 0, exact_div = 0, exact_mul = 0;
      for (int i = 0; i < n; i++) {
         float hw; memcpy(&hw, &o[2 * i + 1], 4);
         int raw = sz == 1 ? (fm[f].t == CL_SNORM_INT8 ? (int8_t)i : i) : (fm[f].t == CL_SNORM_INT16 ? (int16_t)V16(i) : V16(i));
         float mx = sz == 1 ? (fm[f].t == CL_SNORM_INT8 ? 127.f : 255.f) : (fm[f].t == CL_SNORM_INT16 ? 32767.f : 65535.f);
         float dv = fmaxf(raw / mx, -1.f), ml = fmaxf(raw * (1.0f / mx), -1.f);
         exact_div += !memcmp(&dv, &hw, 4); exact_mul += !memcmp(&ml, &hw, 4);
         if (o[2 * i] != o[2 * i + 1] && diff++ < 3) printf("  %s raw %d: buffer %08x image %08x\n", fm[f].nm, raw, o[2 * i], o[2 * i + 1]);
      }
      printf("%s: %d/%d differ; hardware == x/max for %d, == x*(1/max) for %d\n", fm[f].nm, diff, n, exact_div, exact_mul);
   }
   return 0;
}
