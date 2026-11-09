/* fmaprobe: FP32/INT throughput per instruction form (RX550 G5). Prints GOP/s per kernel. */
#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#define BODY(T, init, op1, op2) "kernel void k_" #T "(global float *d) { " init \
  " for (uint i = 0u; i < 512u; i++) { " op1 " " op2 " } d[get_global_id(0)] = (float)y; }\n"
static const char *src =
  BODY(fma,  "float x = (float)get_global_id(0); float y = (float)get_local_id(0);", "x = fma(y, x, y);", "y = fma(x, y, x);")
  BODY(mad,  "float x = (float)get_global_id(0); float y = (float)get_local_id(0);", "x = mad(y, x, y);", "y = mad(x, y, x);")
  BODY(expr, "float x = (float)get_global_id(0); float y = (float)get_local_id(0);", "x = y * x + y;", "y = x * y + x;")
  BODY(mul,  "float x = (float)get_global_id(0); float y = (float)get_local_id(0);", "x = y * x;", "y = x * y;")
  BODY(add,  "float x = (float)get_global_id(0); float y = (float)get_local_id(0);", "x = y + x;", "y = x + y;")
  BODY(imul, "int x = get_global_id(0); int y = get_local_id(0);", "x = y * x + y;", "y = x * y + x;")
  BODY(mad24,"int x = get_global_id(0); int y = get_local_id(0);", "x = mad24(y, x, y);", "y = mad24(x, y, x);");
int main(int argc, char **argv) {
  const char *opts = argc > 1 ? argv[1] : "";
  cl_platform_id p[4]; cl_uint np; clGetPlatformIDs(4, p, &np);
  cl_device_id dev; cl_int e = -1;
  for (cl_uint i = 0; i < np && e; i++) e = clGetDeviceIDs(p[i], CL_DEVICE_TYPE_GPU, 1, &dev, NULL);
  if (e) { printf("no GPU\n"); return 1; }
  cl_uint cu, mhz; clGetDeviceInfo(dev, CL_DEVICE_MAX_COMPUTE_UNITS, 4, &cu, 0);
  clGetDeviceInfo(dev, CL_DEVICE_MAX_CLOCK_FREQUENCY, 4, &mhz, 0);
  printf("CUs %u clock %u MHz peak FP32 %.1f GFLOP/s (opts '%s')\n", cu, mhz, cu * 64 * 2.0 * mhz / 1000, opts);
  cl_context c = clCreateContext(0, 1, &dev, 0, 0, &e);
  cl_command_queue q = clCreateCommandQueueWithProperties(c, dev, (cl_queue_properties[]){CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0}, &e);
  cl_program pr = clCreateProgramWithSource(c, 1, &src, 0, &e);
  if (clBuildProgram(pr, 1, &dev, opts, 0, 0)) { char log[8192]; clGetProgramBuildInfo(pr, dev, CL_PROGRAM_BUILD_LOG, sizeof log, log, 0); printf("%s\n", log); return 1; }
  size_t n = (size_t)cu * 64 * 2048, l = 64;
  cl_mem b = clCreateBuffer(c, CL_MEM_READ_WRITE, n * 4, 0, &e);
  const char *names[] = {"k_fma", "k_mad", "k_expr", "k_mul", "k_add", "k_imul", "k_mad24"};
  const double opsper[] = {2, 2, 2, 1, 1, 2, 2};
  for (int k = 0; k < 7; k++) {
    cl_kernel kr = clCreateKernel(pr, names[k], &e); clSetKernelArg(kr, 0, sizeof b, &b);
    double best = 1e30;
    for (int r = 0; r < 4; r++) {
      cl_event ev; clEnqueueNDRangeKernel(q, kr, 1, 0, &n, &l, 0, 0, &ev); clWaitForEvents(1, &ev);
      cl_ulong t0, t1; clGetEventProfilingInfo(ev, CL_PROFILING_COMMAND_START, 8, &t0, 0);
      clGetEventProfilingInfo(ev, CL_PROFILING_COMMAND_END, 8, &t1, 0);
      if (r && (t1 - t0) < best) best = t1 - t0; clReleaseEvent(ev);
    }
    printf("%-8s %8.1f GOP/s  (%.3f ms)\n", names[k], n * 1024.0 * opsper[k] / best, best / 1e6);
    clReleaseKernel(kr);
  }
  return 0;
}
