/* llprobe FILE.cl: run ll(global Node *n, global uint *out) on a plain buffer and on an SVM allocation. */
#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
int main(int argc, char **argv)
{
   FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
   char *src = calloc(1, n + 1); fread(src, 1, n, f); fclose(f);
   cl_platform_id p; cl_device_id d; cl_int e; clGetPlatformIDs(1, &p, NULL); clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 1, &d, NULL);
   cl_context c = clCreateContext(NULL, 1, &d, NULL, NULL, &e); cl_command_queue q = clCreateCommandQueueWithProperties(c, d, NULL, &e);
   const char *s = src; cl_program pr = clCreateProgramWithSource(c, 1, &s, NULL, &e);
   if (clBuildProgram(pr, 1, &d, "", NULL, NULL)) return 4;
   cl_kernel k = clCreateKernel(pr, "ll", &e);
   for (int svm = 0; svm < 2; svm++) {
      uint32_t out[4] = {0};
      cl_mem bo = clCreateBuffer(c, CL_MEM_READ_WRITE, sizeof out, NULL, &e);
      void *sv = NULL; cl_mem bn = NULL;
      if (svm) { sv = clSVMAlloc(c, CL_MEM_READ_WRITE, 4096, 0); clSetKernelArgSVMPointer(k, 0, sv); }
      else { bn = clCreateBuffer(c, CL_MEM_READ_WRITE, 4096, NULL, &e); clSetKernelArg(k, 0, sizeof bn, &bn); }
      clSetKernelArg(k, 1, sizeof bo, &bo); size_t gs = 1;
      e = clEnqueueNDRangeKernel(q, k, 1, NULL, &gs, NULL, 0, NULL, NULL);
      clEnqueueReadBuffer(q, bo, CL_TRUE, 0, sizeof out, out, 0, NULL, NULL);
      printf("%s: e=%d  pNext-n=%u (exp 16)  pNext->pos=%u (exp 1)  pNext hi %08x lo %08x  svm ptr %p\n", svm ? "SVM " : "buffer", e, out[0], out[1], out[2], out[3], sv);
      if (svm) { uint64_t host; clEnqueueSVMMap(q, CL_TRUE, CL_MAP_READ, sv, 64, 0, NULL, NULL); memcpy(&host, (char *)sv + 8, 8); printf("   host sees n[0].pNext = %016llx\n", (unsigned long long)host); clEnqueueSVMUnmap(q, sv, 0, NULL, NULL); clFinish(q); }
   }
   return 0;
}
