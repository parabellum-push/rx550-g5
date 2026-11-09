/* vhprobe FILE.cl KERNEL: run kernel(global ushort *g) with g = half 1,2,3,...,8 (printf output). */
#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
int main(int argc, char **argv)
{
   FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
   char *src = calloc(1, n + 1); fread(src, 1, n, f); fclose(f);
   cl_platform_id p; cl_device_id d; cl_int e; clGetPlatformIDs(1, &p, NULL); clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 1, &d, NULL);
   cl_context c = clCreateContext(NULL, 1, &d, NULL, NULL, &e); cl_command_queue q = clCreateCommandQueueWithProperties(c, d, NULL, &e);
   const char *s = src; cl_program pr = clCreateProgramWithSource(c, 1, &s, NULL, &e);
   if (clBuildProgram(pr, 1, &d, "", NULL, NULL)) { char l[8192]; clGetProgramBuildInfo(pr, d, CL_PROGRAM_BUILD_LOG, sizeof l, l, NULL); printf("%s\n", l); return 4; }
   cl_kernel k = clCreateKernel(pr, argv[2], &e);
   uint16_t h[8] = {0x3c00, 0x4000, 0x4200, 0x4400, 0x4500, 0x4600, 0x4700, 0x4800};   /* 1..8 */
   cl_mem b = clCreateBuffer(c, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof h, h, &e);
   clSetKernelArg(k, 0, sizeof b, &b); size_t gs = 1;
   e = clEnqueueNDRangeKernel(q, k, 1, NULL, &gs, NULL, 0, NULL, NULL); clFinish(q); fflush(stdout); return e;
}
