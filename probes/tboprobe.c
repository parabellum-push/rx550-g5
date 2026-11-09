/* tboprobe: image1d_buffer_t read_imagef for UNORM/SNORM 8/16 formats vs reference. */
#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CK(x) do { cl_int e_ = (x); if (e_) { printf("CL error %d at line %d\n", e_, __LINE__); exit(100); } } while (0)
static const char *src =
"kernel void k(read_only image1d_buffer_t im, global float4 *o) { int x = get_global_id(0); o[x] = read_imagef(im, x); }\n";
enum { W = 16 };
int main(void)
{
   cl_platform_id p; cl_device_id d; cl_int err;
   CK(clGetPlatformIDs(1, &p, NULL)); CK(clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 1, &d, NULL));
   cl_context c = clCreateContext(NULL, 1, &d, NULL, NULL, &err); CK(err);
   cl_command_queue q = clCreateCommandQueueWithProperties(c, d, NULL, &err); CK(err);
   cl_program pr = clCreateProgramWithSource(c, 1, &src, NULL, &err); CK(err);
   CK(clBuildProgram(pr, 1, &d, "", NULL, NULL));
   cl_kernel k = clCreateKernel(pr, "k", &err); CK(err);
   struct { cl_channel_order o; int n; cl_channel_type t; int sz; int sgn; const char *nm; } fm[] = {
      {CL_R, 1, CL_UNORM_INT8, 1, 0, "R UNORM8"}, {CL_R, 1, CL_SNORM_INT8, 1, 1, "R SNORM8"},
      {CL_R, 1, CL_UNORM_INT16, 2, 0, "R UNORM16"}, {CL_R, 1, CL_SNORM_INT16, 2, 1, "R SNORM16"},
      {CL_RGBA, 4, CL_UNORM_INT8, 1, 0, "RGBA UNORM8"}, {CL_RGBA, 4, CL_UNORM_INT16, 2, 0, "RGBA UNORM16"},
      {CL_RGBA, 4, CL_SNORM_INT16, 2, 1, "RGBA SNORM16"}, {CL_R, 1, CL_UNSIGNED_INT16, 2, 0, "R UINT16 (int ref)"}};
   int fails = 0;
   for (unsigned f = 0; f < sizeof fm / sizeof fm[0]; f++) {
      unsigned char data[W * 4 * 2]; float o[W][4];
      for (unsigned i = 0; i < sizeof data; i++) data[i] = (unsigned char)(i * 29 + 3);
      cl_mem buf = clCreateBuffer(c, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof data, data, &err); CK(err);
      cl_image_format fmt = {fm[f].o, fm[f].t};
      cl_image_desc desc = {CL_MEM_OBJECT_IMAGE1D_BUFFER, W}; desc.buffer = buf;
      cl_mem im = clCreateImage(c, CL_MEM_READ_ONLY, &fmt, &desc, NULL, &err); CK(err);
      cl_mem bo = clCreateBuffer(c, CL_MEM_WRITE_ONLY, sizeof o, NULL, &err); CK(err);
      CK(clSetKernelArg(k, 0, sizeof im, &im)); CK(clSetKernelArg(k, 1, sizeof bo, &bo));
      size_t gs = W; CK(clEnqueueNDRangeKernel(q, k, 1, NULL, &gs, NULL, 0, NULL, NULL));
      CK(clEnqueueReadBuffer(q, bo, CL_TRUE, 0, sizeof o, o, 0, NULL, NULL));
      int bad = 0;
      for (int x = 0; x < W; x++) for (int ch = 0; ch < fm[f].n; ch++) {
         const unsigned char *e = data + (x * fm[f].n + ch) * fm[f].sz; float ref;
         if (fm[f].sz == 1) ref = fm[f].sgn ? fmaxf((int8_t)e[0] / 127.f, -1.f) : e[0] / 255.f;
         else { uint16_t v; memcpy(&v, e, 2); ref = fm[f].t == CL_UNSIGNED_INT16 ? 0 : fm[f].sgn ? fmaxf((int16_t)v / 32767.f, -1.f) : v / 65535.f; }
         if (fm[f].t == CL_UNSIGNED_INT16) continue;
         if (fabsf(o[x][ch] - ref) > 1e-3f) { if (bad < 2) printf("  %s x %d ch %d exp %f got %f\n", fm[f].nm, x, ch, ref, o[x][ch]); bad++; }
      }
      printf("%s %s: %d bad\n", bad ? "FAIL" : "PASS", fm[f].nm, bad); fails += !!bad;
      clReleaseMemObject(im); clReleaseMemObject(buf); clReleaseMemObject(bo);
   }
   return fails;
}
