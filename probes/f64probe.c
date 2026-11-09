/* f64probe: cl_khr_fp64 on the BE device (RUSTICL_FEATURES=fp64). Exit = failures. */
#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CK(x) do { cl_int e_ = (x); if (e_) { printf("CL error %d at line %d\n", e_, __LINE__); exit(100); } } while (0)
static const char *src =
"#pragma OPENCL EXTENSION cl_khr_fp64 : enable\n"
"kernel void k(global double *d, global ulong *u, double a, double2 v, global const double *in, local double *l) {\n"
"  size_t i = get_global_id(0);\n"
"  l[get_local_id(0)] = in[i] * 2.0;\n"
"  barrier(CLK_LOCAL_MEM_FENCE);\n"
"  private double p[4] = {in[i], in[i] + 1.0, a, v.y};\n"
"  int j = (int)(i & 3);\n"
"  d[i * 8 + 0] = in[i] + a;\n"
"  d[i * 8 + 1] = sqrt(in[i]);\n"
"  d[i * 8 + 2] = fma(in[i], a, v.x);\n"
"  d[i * 8 + 3] = l[get_local_id(0)];\n"
"  d[i * 8 + 4] = p[j];\n"
"  d[i * 8 + 5] = (double)(float)in[i];\n"
"  d[i * 8 + 6] = exp(in[i] / 100.0);\n"
"  d[i * 8 + 7] = vload2(0, in + i).y;\n"
"  u[i] = as_ulong(in[i]);\n"
"}\n";
int main(void)
{
   cl_platform_id p; cl_device_id dev; cl_int err;
   CK(clGetPlatformIDs(1, &p, NULL)); CK(clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 1, &dev, NULL));
   cl_device_fp_config cfg = 0; clGetDeviceInfo(dev, CL_DEVICE_DOUBLE_FP_CONFIG, sizeof cfg, &cfg, NULL);
   printf("double fp config 0x%llx\n", (unsigned long long)cfg);
   cl_context c = clCreateContext(NULL, 1, &dev, NULL, NULL, &err); CK(err);
   cl_command_queue q = clCreateCommandQueueWithProperties(c, dev, NULL, &err); CK(err);
   cl_program pr = clCreateProgramWithSource(c, 1, &src, NULL, &err); CK(err);
   if (clBuildProgram(pr, 1, &dev, "", NULL, NULL)) {
      static char log[16384]; clGetProgramBuildInfo(pr, dev, CL_PROGRAM_BUILD_LOG, sizeof log, log, NULL);
      printf("build failed\n%s\n", log); return 100;
   }
   enum { N = 64 };
   double in[N + 1], d[N * 8]; uint64_t u[N];
   for (int i = 0; i <= N; i++) in[i] = 1.0 + i * 3.25 + i * i * 1e-7;
   double a = 0.3125; cl_double2 v = {{-7.5, 1234.0625}};
   cl_mem bd = clCreateBuffer(c, CL_MEM_WRITE_ONLY, sizeof d, NULL, &err); CK(err);
   cl_mem bu = clCreateBuffer(c, CL_MEM_WRITE_ONLY, sizeof u, NULL, &err); CK(err);
   cl_mem bi = clCreateBuffer(c, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof in, in, &err); CK(err);
   cl_kernel k = clCreateKernel(pr, "k", &err); CK(err);
   CK(clSetKernelArg(k, 0, sizeof bd, &bd)); CK(clSetKernelArg(k, 1, sizeof bu, &bu));
   CK(clSetKernelArg(k, 2, sizeof a, &a)); CK(clSetKernelArg(k, 3, sizeof v, &v));
   CK(clSetKernelArg(k, 4, sizeof bi, &bi)); CK(clSetKernelArg(k, 5, 16 * sizeof(double), NULL));
   size_t gs = N, ls = 16;
   CK(clEnqueueNDRangeKernel(q, k, 1, NULL, &gs, &ls, 0, NULL, NULL));
   CK(clEnqueueReadBuffer(q, bd, CL_TRUE, 0, sizeof d, d, 0, NULL, NULL));
   CK(clEnqueueReadBuffer(q, bu, CL_TRUE, 0, sizeof u, u, 0, NULL, NULL));
   const char *nm[8] = {"add arg", "sqrt", "fma arg double2.x", "local", "private", "to float and back", "exp", "vload2.y"};
   int bad[9] = {0};
   for (int i = 0; i < N; i++) {
      double pv[4] = {in[i], in[i] + 1.0, a, v.s[1]};
      double e[8] = {in[i] + a, sqrt(in[i]), fma(in[i], a, v.s[0]), in[i] * 2.0, pv[i & 3], (double)(float)in[i],
                     exp(in[i] / 100.0), in[i + 1]};
      for (int t = 0; t < 8; t++) {
         double tol = t == 6 ? 4e-15 * e[t] : 0;   /* exp: a few ulp */
         if (fabs(d[i * 8 + t] - e[t]) > tol && bad[t]++ < 2)
            printf("  %s [%d] exp %.17g got %.17g\n", nm[t], i, e[t], d[i * 8 + t]);
      }
      uint64_t eu; memcpy(&eu, &in[i], 8);
      if (u[i] != eu && bad[8]++ < 2) printf("  as_ulong [%d] exp %016llx got %016llx\n", i, (unsigned long long)eu, (unsigned long long)u[i]);
   }
   int f = 0;
   for (int t = 0; t < 9; t++) { printf("%s %s\n", bad[t] ? "FAIL" : "PASS", t < 8 ? nm[t] : "as_ulong"); f += !!bad[t]; }
   return f;
}
