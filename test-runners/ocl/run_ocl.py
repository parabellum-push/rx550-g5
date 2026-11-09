#!/usr/bin/env python3
"""run_ocl.py LIST.csv OUT.jsonl [TIMEOUT]: run OpenCL CTS suites (name,cmd lines) with ~/m26/envocl.sh.
Resumes: suites already in OUT are skipped. Stops if dmesg gains a ring timeout."""
import json, os, re, shlex, subprocess, sys, time
lst, out = sys.argv[1], sys.argv[2]; tmo = int(sys.argv[3]) if len(sys.argv) > 3 else 3600
base = os.path.expanduser("~/ocl-cts")
env = dict(os.environ)
for l in subprocess.run(["bash", "-c", ". ~/m26/envocl.sh; env"], capture_output=True, text=True).stdout.splitlines():
    k, _, v = l.partition("="); env[k] = v
done = {json.loads(l)["suite"] for l in open(out)} if os.path.exists(out) else set()
def rings():
    return subprocess.run("dmesg | grep -ciE \"ring .*timeout\"", shell=True, capture_output=True, text=True).stdout.strip()
r0 = rings()
for line in open(os.path.join(base, lst)):
    line = line.strip()
    if not line or line.startswith("#") or line.startswith("CL_DEVICE_TYPE"): continue
    name, _, cmd = line.partition(",")
    if name in done: continue
    args = shlex.split(cmd); exe = os.path.join(base, args[0])
    if not os.path.exists(exe):
        rec = {"suite": name, "rc": "NOTBUILT", "secs": 0, "summary": [], "log": ""}
        open(out, "a").write(json.dumps(rec) + "\n"); print(json.dumps(rec), flush=True); continue
    logf = os.path.join(base, "logs", re.sub(r"[^A-Za-z0-9]+", "_", name) + ".log")
    t = time.time()
    try:
        p = subprocess.run([exe] + args[1:], cwd=os.path.dirname(exe), env=env, stdout=open(logf, "w"), stderr=subprocess.STDOUT, timeout=tmo)
        rc = p.returncode
    except subprocess.TimeoutExpired:
        rc = "TIMEOUT"
    txt = open(logf, errors="replace").read()
    summ = re.findall(r"(PASSED \d+ of \d+ (?:sub-)?tests?\.?|FAILED \d+ of \d+ (?:sub-)?tests?\.?|PASSED sub-test.*|FAILED (?:sub-)?test.*)", txt)[-3:]
    rec = {"suite": name, "rc": rc, "secs": round(time.time() - t), "summary": summ, "log": logf}
    open(out, "a").write(json.dumps(rec) + "\n")
    print(json.dumps(rec), flush=True)
    if rings() != r0:
        print("RING TIMEOUT - stopping", flush=True); open(out + ".hang", "w").write(name); break
print("DONE", flush=True); open(out + ".done", "w").write("DONE")
