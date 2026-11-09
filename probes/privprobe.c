/* privprobe: private-memory vloadN/vstoreN at odd element offsets (OpenCL CTS basic vload_private/vstore_private).
 * Usage: privprobe [T V]  (default runs char/short/int x 2/3/4). Exit = failures. */
#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *src =
"#define VT(t,n) t##n\n#define VTY(t,n) VT(t,n)\n#define PV VTY(T,V)\n#define VL(n) vload##n\n#define VS(n) vstore##n\n"
"#define VLOAD(n) VL(n)\n#define VSTORE(n) VS(n)\n"
"kernel void ld(global const T *src, global T *out) {\n"
"  private PV p[N];\n"
"  for (int i = 0; i < N; i++) p[i] = VLOAD(V)(i, src);\n"
"  int tid = get_global_id(0); int off = tid >> 2, al = tid & 3;\n"
"  PV t = VLOAD(V)(off, (private T *)p + al);\n"
"  VSTORE(V)(t, tid, out);\n"
"}\n"
"kernel void st(global const T *vals, global T *out) {\n"
"  private PV p[N];\n"
"  for (int i = 0; i < N; i++) p[i] = (PV)(T)0;\n"
"  int tid = get_global_id(0); int off = tid >> 2, al = tid & 3;\n"
"  VSTORE(V)(VLOAD(V)(tid, vals), off, (private T *)p + al);\n"
"  for (int i = 0; i < V * N; i++) out[tid * V * N + i] = ((private T *)p)[i];\n"
"}\n";

#define CK(x) do { cl_int e_ = (x); if (e_) { printf("CL error %d at line %d\n", e_, __LINE__); exit(100); } } while (0)
enum { N = 16, NT = (N - 2) * 4 };
static cl_context ctx; static cl_command_queue q; static cl_device_id dev;

static int run(const char *T, int ts, int V)
{
   char opt[128]; snprintf(opt, sizeof opt, "-DT=%s -DV=%d -DN=%d", T, V, N);
   cl_int err; cl_program pr = clCreateProgramWithSource(ctx, 1, &src, NULL, &err); CK(err);
   if (clBuildProgram(pr, 1, &dev, opt, NULL, NULL)) {
      static char log[16384]; clGetProgramBuildInfo(pr, dev, CL_PROGRAM_BUILD_LOG, sizeof log, log, NULL);
      printf("build failed %s:\n%s\n", opt, log); return 1;
   }
   size_t n = (size_t)V * N * ts, on = (size_t)NT * V * N * ts;
   unsigned char *in = malloc(n), *ref = malloc(on), *got = malloc(on), *vals = malloc((size_t)NT * V * ts);
   for (size_t i = 0; i < n; i++) in[i] = (unsigned char)(i * 37 + 5);
   for (size_t i = 0; i < (size_t)NT * V * ts; i++) vals[i] = (unsigned char)(i * 53 + 11);
   int fails = 0, nbad = 0;
   cl_mem bi = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, n, in, &err); CK(err);
   cl_mem bv = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, (size_t)NT * V * ts, vals, &err); CK(err);
   cl_mem bo = clCreateBuffer(ctx, CL_MEM_READ_WRITE, on, NULL, &err); CK(err);
   size_t gs = NT;
   /* load */
   cl_kernel k = clCreateKernel(pr, "ld", &err); CK(err);
   CK(clSetKernelArg(k, 0, sizeof bi, &bi)); CK(clSetKernelArg(k, 1, sizeof bo, &bo));
   CK(clEnqueueNDRangeKernel(q, k, 1, NULL, &gs, NULL, 0, NULL, NULL));
   CK(clEnqueueReadBuffer(q, bo, CL_TRUE, 0, (size_t)NT * V * ts, got, 0, NULL, NULL));
   for (int t = 0; t < NT; t++) {
      size_t e = (size_t)(t >> 2) * V + (t & 3);
      if (V == 3) {
         /* private T3 p[N] has a 4-element stride: element k of (T *)p is in[(k/4)*3 + k%4], k%4 == 3 is padding */
         int pad = 0; unsigned char exp3[3 * 8];
         for (int c = 0; c < 3; c++) {
            size_t k = e + c;
            if (k % 4 == 3) pad = 1;
            else memcpy(exp3 + c * ts, in + ((k / 4) * 3 + k % 4) * ts, ts);
         }
         if (!pad && memcmp(got + (size_t)t * V * ts, exp3, (size_t)V * ts) && nbad++ < 4)
            printf("  ld %s3 off %d al %d wrong\n", T, t >> 2, t & 3);
         continue;
      }
      if (memcmp(got + (size_t)t * V * ts, in + e * ts, (size_t)V * ts)) {
         if (nbad++ < 4) {
            printf("  ld %s%d off %d al %d: exp", T, V, t >> 2, t & 3);
            for (int b = 0; b < V * ts; b++) printf(" %02x", in[e * ts + b]);
            printf(" got"); for (int b = 0; b < V * ts; b++) printf(" %02x", got[(size_t)t * V * ts + b]); printf("\n");
         }
      }
   }
   printf("%s ld %s%d: %d/%d bad\n", nbad ? "FAIL" : "PASS", T, V, nbad, NT); fails += !!nbad; nbad = 0;
   clReleaseKernel(k);
   /* store */
   k = clCreateKernel(pr, "st", &err); CK(err);
   CK(clSetKernelArg(k, 0, sizeof bv, &bv)); CK(clSetKernelArg(k, 1, sizeof bo, &bo));
   CK(clEnqueueNDRangeKernel(q, k, 1, NULL, &gs, NULL, 0, NULL, NULL));
   CK(clEnqueueReadBuffer(q, bo, CL_TRUE, 0, on, got, 0, NULL, NULL));
   memset(ref, 0, on);
   for (int t = 0; t < NT; t++) {
      size_t e = (size_t)(t >> 2) * V + (t & 3);
      memcpy(ref + (size_t)t * n + e * ts, vals + (size_t)t * V * ts, (size_t)V * ts);
      if (memcmp(ref + (size_t)t * n, got + (size_t)t * n, n) && nbad++ < 4) {
         printf("  st %s%d off %d al %d bytes [e=%zu]:", T, V, t >> 2, t & 3, e);
         for (size_t b = 0; b < n; b++) if (ref[t * n + b] != got[t * n + b]) printf(" @%zu exp %02x got %02x", b, ref[t * n + b], got[t * n + b]);
         printf("\n");
      }
   }
   printf("%s st %s%d: %d/%d bad\n", nbad ? "FAIL" : "PASS", T, V, nbad, NT); fails += !!nbad;
   clReleaseKernel(k); clReleaseMemObject(bi); clReleaseMemObject(bv); clReleaseMemObject(bo); clReleaseProgram(pr);
   free(in); free(ref); free(got); free(vals);
   return fails;
}

int main(int argc, char **argv)
{
   cl_platform_id p; cl_int err;
   CK(clGetPlatformIDs(1, &p, NULL)); CK(clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 1, &dev, NULL));
   ctx = clCreateContext(NULL, 1, &dev, NULL, NULL, &err); CK(err);
   q = clCreateCommandQueueWithProperties(ctx, dev, NULL, &err); CK(err);
   int f = 0;
   if (argc > 2) {
      int ts = !strcmp(argv[1], "char") || !strcmp(argv[1], "uchar") ? 1 : !strcmp(argv[1], "short") ? 2 : !strcmp(argv[1], "long") ? 8 : 4;
      return run(argv[1], ts, atoi(argv[2]));
   }
   const char *tn[] = {"char", "short", "int", "long"}; int tsz[] = {1, 2, 4, 8}; int vs[] = {2, 3, 4, 8};
   for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) f += run(tn[i], tsz[i], vs[j]);
   printf("failures: %d\n", f);
   return f;
}
