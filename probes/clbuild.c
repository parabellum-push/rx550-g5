/* clbuild FILE.cl [options]: build an OpenCL C file on the first GPU and print the build log. */
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
   const char *s = src;
   cl_program pr = clCreateProgramWithSource(c, 1, &s, NULL, &e);
   e = clBuildProgram(pr, 1, &d, argc > 2 ? argv[2] : "", NULL, NULL);
   static char log[65536]; clGetProgramBuildInfo(pr, d, CL_PROGRAM_BUILD_LOG, sizeof log, log, NULL);
   printf("build %s (%d)\n%s\n", e ? "FAILED" : "OK", e, log);
   return e != 0;
}
