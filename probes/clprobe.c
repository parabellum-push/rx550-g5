/* clprobe: OpenCL big-endian probe for the RX550 G5 port.
 * Each test prints PASS/FAIL with expected and actual values. Exit code = number of failures. */
#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *src =
"kernel void add(global uint *u, global ushort *s, global uchar *c, global ulong *l, global float *f) {\n"
"  size_t i = get_global_id(0);\n"
"  u[i] += 0x01010101u; s[i] += (ushort)0x0101; c[i] += (uchar)1; l[i] += 0x0101010101010101ul; f[i] *= 2.0f;\n"
"}\n"
"kernel void args(global ulong *o, uchar a, ushort b, uint c, ulong d, float e, ulong2 g) {\n"
"  o[0] = a; o[1] = b; o[2] = c; o[3] = d; o[4] = as_uint(e); o[5] = g.y;\n"
"}\n"
"kernel void pun(global const uchar *b, global uint *o, local uchar *lb) {\n"
"  o[0] = *(global const uint *)b;          /* bytes 01 02 03 04 -> BE device: 0x01020304 */\n"
"  for (int i = 0; i < 8; i++) lb[i] = b[i];\n"
"  barrier(CLK_LOCAL_MEM_FENCE);\n"
"  o[1] = ((local uint *)lb)[1];             /* bytes 05 06 07 08 -> 0x05060708 */\n"
"  private uchar p[4] = {b[0], b[1], b[2], b[3]};\n"
"  o[2] = *(private uint *)p;\n"
"  o[3] = as_uint((uchar4)(b[0], b[1], b[2], b[3]));\n"
"}\n"
"kernel void atom(global uint *c, global ulong *m) {\n"
"  atomic_add(c, 1u); atomic_max(c + 1, (uint)get_global_id(0)); atomic_sub(c + 2, 2u);\n"
"  if (get_global_id(0) == 0) m[0] = 0x0102030405060708ul;\n"
"}\n";

static int fails;
static void check(const char *what, uint64_t exp, uint64_t got)
{
   int ok = exp == got;
   fails += !ok;
   printf("%s %-28s exp 0x%016llx got 0x%016llx\n", ok ? "PASS" : "FAIL", what,
          (unsigned long long)exp, (unsigned long long)got);
}

#define CK(x) do { cl_int e_ = (x); if (e_) { printf("CL error %d at %s:%d\n", e_, __FILE__, __LINE__); return 100; } } while (0)

int main(void)
{
   cl_platform_id plats[8]; cl_uint np = 0; cl_device_id dev; cl_int err;
   CK(clGetPlatformIDs(8, plats, &np));
   int found = 0;
   for (cl_uint p = 0; p < np && !found; p++)
      if (clGetDeviceIDs(plats[p], CL_DEVICE_TYPE_GPU, 1, &dev, NULL) == CL_SUCCESS) found = 1;
   if (!found) { printf("no GPU device\n"); return 100; }
   char name[256]; cl_bool le;
   CK(clGetDeviceInfo(dev, CL_DEVICE_NAME, sizeof name, name, NULL));
   CK(clGetDeviceInfo(dev, CL_DEVICE_ENDIAN_LITTLE, sizeof le, &le, NULL));
   printf("device: %s, CL_DEVICE_ENDIAN_LITTLE=%d\n", name, (int)le);
   check("device reports big endian", 0, le);

   cl_context ctx = clCreateContext(NULL, 1, &dev, NULL, NULL, &err); CK(err);
   cl_command_queue q = clCreateCommandQueueWithProperties(ctx, dev, NULL, &err); CK(err);
   cl_program prog = clCreateProgramWithSource(ctx, 1, &src, NULL, &err); CK(err);
   if (clBuildProgram(prog, 1, &dev, "", NULL, NULL)) {
      char log[8192]; clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, sizeof log, log, NULL);
      printf("build failed:\n%s\n", log); return 100;
   }

   /* 1: arithmetic on host-order buffers */
   enum { N = 4 };
   uint32_t u[N]; uint16_t s[N]; uint8_t c[N]; uint64_t l[N]; float f[N];
   for (int i = 0; i < N; i++) { u[i] = 0x10203040u + i; s[i] = 0x1020 + i; c[i] = 0x10 + i; l[i] = 0x1020304050607080ull + i; f[i] = 1.5f + i; }
   cl_mem bu = clCreateBuffer(ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof u, u, &err); CK(err);
   cl_mem bs = clCreateBuffer(ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof s, s, &err); CK(err);
   cl_mem bc = clCreateBuffer(ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof c, c, &err); CK(err);
   cl_mem bl = clCreateBuffer(ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof l, l, &err); CK(err);
   cl_mem bf = clCreateBuffer(ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof f, f, &err); CK(err);
   cl_kernel k = clCreateKernel(prog, "add", &err); CK(err);
   cl_mem m5[5] = {bu, bs, bc, bl, bf};
   for (int a = 0; a < 5; a++) CK(clSetKernelArg(k, a, sizeof(cl_mem), &m5[a]));
   size_t gs = N;
   CK(clEnqueueNDRangeKernel(q, k, 1, NULL, &gs, NULL, 0, NULL, NULL));
   CK(clEnqueueReadBuffer(q, bu, CL_TRUE, 0, sizeof u, u, 0, NULL, NULL));
   CK(clEnqueueReadBuffer(q, bs, CL_TRUE, 0, sizeof s, s, 0, NULL, NULL));
   CK(clEnqueueReadBuffer(q, bc, CL_TRUE, 0, sizeof c, c, 0, NULL, NULL));
   CK(clEnqueueReadBuffer(q, bl, CL_TRUE, 0, sizeof l, l, 0, NULL, NULL));
   CK(clEnqueueReadBuffer(q, bf, CL_TRUE, 0, sizeof f, f, 0, NULL, NULL));
   check("uint  += 0x01010101", 0x11213142u, u[1]);
   check("ushort+= 0x0101", 0x1122, s[1]);
   check("uchar += 1", 0x12, c[1]);
   check("ulong += 0x0101..", 0x1121314151617182ull, l[1]);
   { float e = 5.0f; uint32_t eb, gb; memcpy(&eb, &e, 4); memcpy(&gb, &f[1], 4); check("float *= 2 (2.5 -> 5)", eb, gb); }

   /* 2: scalar kernel arguments of every width */
   cl_mem bo = clCreateBuffer(ctx, CL_MEM_READ_WRITE, 8 * 8, NULL, &err); CK(err);
   k = clCreateKernel(prog, "args", &err); CK(err);
   cl_uchar a8 = 0x12; cl_ushort a16 = 0x1234; cl_uint a32 = 0x12345678; cl_ulong a64 = 0x123456789abcdef0ull;
   cl_float af = 3.25f; cl_ulong2 ag = {{0x1111222233334444ull, 0x5555666677778888ull}};
   CK(clSetKernelArg(k, 0, sizeof bo, &bo)); CK(clSetKernelArg(k, 1, 1, &a8)); CK(clSetKernelArg(k, 2, 2, &a16));
   CK(clSetKernelArg(k, 3, 4, &a32)); CK(clSetKernelArg(k, 4, 8, &a64)); CK(clSetKernelArg(k, 5, 4, &af));
   CK(clSetKernelArg(k, 6, sizeof ag, &ag));
   gs = 1;
   CK(clEnqueueNDRangeKernel(q, k, 1, NULL, &gs, NULL, 0, NULL, NULL));
   uint64_t o[8];
   CK(clEnqueueReadBuffer(q, bo, CL_TRUE, 0, sizeof o, o, 0, NULL, NULL));
   check("arg uchar", a8, o[0]); check("arg ushort", a16, o[1]); check("arg uint", a32, o[2]);
   check("arg ulong", a64, o[3]);
   { uint32_t fb; memcpy(&fb, &af, 4); check("arg float bits", fb, o[4]); }
   check("arg ulong2.y", 0x5555666677778888ull, o[5]);

   /* 3: byte <-> word reinterpretation (BE device semantics) */
   uint8_t bytes[8] = {1, 2, 3, 4, 5, 6, 7, 8};
   cl_mem bb = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 8, bytes, &err); CK(err);
   cl_mem bp = clCreateBuffer(ctx, CL_MEM_READ_WRITE, 16, NULL, &err); CK(err);
   k = clCreateKernel(prog, "pun", &err); CK(err);
   CK(clSetKernelArg(k, 0, sizeof bb, &bb)); CK(clSetKernelArg(k, 1, sizeof bp, &bp)); CK(clSetKernelArg(k, 2, 8, NULL));
   CK(clEnqueueNDRangeKernel(q, k, 1, NULL, &gs, NULL, 0, NULL, NULL));
   uint32_t po[4];
   CK(clEnqueueReadBuffer(q, bp, CL_TRUE, 0, sizeof po, po, 0, NULL, NULL));
   check("global bytes as uint", 0x01020304, po[0]);
   check("local bytes as uint", 0x05060708, po[1]);
   check("private bytes as uint", 0x01020304, po[2]);
   check("as_uint(uchar4)", 0x01020304, po[3]);

   /* 4: atomics */
   uint32_t cnt[3] = {1000, 0, 0x100000};
   cl_mem bk = clCreateBuffer(ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof cnt, cnt, &err); CK(err);
   cl_mem bm = clCreateBuffer(ctx, CL_MEM_READ_WRITE, 8, NULL, &err); CK(err);
   k = clCreateKernel(prog, "atom", &err); CK(err);
   CK(clSetKernelArg(k, 0, sizeof bk, &bk)); CK(clSetKernelArg(k, 1, sizeof bm, &bm));
   gs = 4096;
   CK(clEnqueueNDRangeKernel(q, k, 1, NULL, &gs, NULL, 0, NULL, NULL));
   uint64_t mv;
   CK(clEnqueueReadBuffer(q, bk, CL_TRUE, 0, sizeof cnt, cnt, 0, NULL, NULL));
   CK(clEnqueueReadBuffer(q, bm, CL_TRUE, 0, 8, &mv, 0, NULL, NULL));
   check("atomic_add x4096", 1000 + 4096, cnt[0]);
   check("atomic_max", 4095, cnt[1]);
   check("atomic_sub 2 x4096", 0x100000 - 2 * 4096, cnt[2]);
   check("ulong store", 0x0102030405060708ull, mv);

   printf("%d failures\n", fails);
   return fails;
}
