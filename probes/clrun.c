/* clrun FILE.cl KERNEL [GLOBAL]: build and run a no-argument kernel (printf tests). */
#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv)
{
   FILE *f = fopen(argv[1], "rb"); if (!f) return 2;
   fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
   char *src = calloc(1, n + 1); fread(src, 1, n, f); fclose(f);
   cl_platform_id p; cl_device_id d; cl_int e;
   clGetPlatformIDs(1, &p, NULL);
   if (clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 1, &d, NULL)) return 3;
   cl_context c = clCreateContext(NULL, 1, &d, NULL, NULL, &e);
   cl_command_queue q = clCreateCommandQueueWithProperties(c, d, NULL, &e);
   const char *s = src;
   cl_program pr = clCreateProgramWithSource(c, 1, &s, NULL, &e);
   if (clBuildProgram(pr, 1, &d, "", NULL, NULL)) { fprintf(stderr, "build failed\n"); return 4; }
   cl_kernel k = clCreateKernel(pr, argv[2], &e); if (e) return 5;
   size_t gs = argc > 3 ? atoi(argv[3]) : 1;
   e = clEnqueueNDRangeKernel(q, k, 1, NULL, &gs, NULL, 0, NULL, NULL);
   clFinish(q); fflush(stdout);
   return e;
}
