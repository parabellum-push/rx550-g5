/* naprobe: nextafter(global in1, global in2) like the CTS math kernel. */
#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define CK(x) do { cl_int e_ = (x); if (e_) { printf("CL error %d at line %d\n", e_, __LINE__); exit(100); } } while (0)
static const char *src =
"kernel void k(global float *out, global float *a, global float *b) { size_t i = get_global_id(0); out[i] = as_float((uint)isnan(a[i]) | ((uint)isnan(b[i]) << 1) | ((uint)(a[i] != a[i]) << 2) | ((uint)(as_uint(fabs(a[i])) == 0) << 3) | ((uint)isinf(a[i]) << 4) | ((uint)isfinite(a[i]) << 5)); }\n";
int main(int argc, char **argv)
{
   cl_platform_id p; cl_device_id d; cl_int err;
   CK(clGetPlatformIDs(1, &p, NULL)); CK(clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 1, &d, NULL));
   cl_context c = clCreateContext(NULL, 1, &d, NULL, NULL, &err); CK(err);
   cl_command_queue q = clCreateCommandQueueWithProperties(c, d, NULL, &err); CK(err);
   cl_program pr = clCreateProgramWithSource(c, 1, &src, NULL, &err); CK(err);
   CK(clBuildProgram(pr, 1, &d, argc > 1 ? argv[1] : "", NULL, NULL));
   cl_kernel k = clCreateKernel(pr, "k", &err); CK(err);
   uint32_t a[4] = {0x00292531u, 0x802b73bau, 0x00000001u, 0x3f800000u}, b[4] = {0x802d6c0au, 0x00384b06u, 0x00000002u, 0}, o[4];
   cl_mem bo = clCreateBuffer(c, CL_MEM_READ_WRITE, sizeof o, NULL, &err); CK(err);
   cl_mem ba = clCreateBuffer(c, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof a, a, &err); CK(err);
   cl_mem bb = clCreateBuffer(c, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof b, b, &err); CK(err);
   CK(clSetKernelArg(k, 0, sizeof bo, &bo)); CK(clSetKernelArg(k, 1, sizeof ba, &ba)); CK(clSetKernelArg(k, 2, sizeof bb, &bb));
   size_t gs = 4; CK(clEnqueueNDRangeKernel(q, k, 1, NULL, &gs, NULL, 0, NULL, NULL));
   CK(clEnqueueReadBuffer(q, bo, CL_TRUE, 0, sizeof o, o, 0, NULL, NULL));
   for (int i = 0; i < 4; i++) printf("isnan(a)|isnan(b)<<1|(a!=a)<<2|(fabs(a)bits==0)<<3|isinf<<4|isfinite<<5 for %08x,%08x: %x\n", a[i], b[i], o[i]);
   return 0;
}
