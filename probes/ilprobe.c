/* ilprobe FILE.spv: clCreateProgramWithIL + clBuildProgram, print the log. */
#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv)
{
   FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
   char *il = malloc(n); fread(il, 1, n, f); fclose(f);
   cl_platform_id p; cl_device_id d; cl_int e; clGetPlatformIDs(1, &p, NULL); clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 1, &d, NULL);
   cl_context c = clCreateContext(NULL, 1, &d, NULL, NULL, &e);
   cl_program pr = clCreateProgramWithIL(c, il, n, &e); printf("create %d\n", e);
   e = clBuildProgram(pr, 1, &d, "", NULL, NULL);
   static char log[16384]; clGetProgramBuildInfo(pr, d, CL_PROGRAM_BUILD_LOG, sizeof log, log, NULL);
   printf("build %d\n%s\n", e, log); return e != 0;
}
