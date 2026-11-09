#!/usr/bin/env python3
"""run_gl.py MODULE LIST OUT.jsonl [HANG]: writes each result at once, resumes on restart. MODULE is gles2, gles3, gles31 or egl."""
import json,os,re,selectors,subprocess,sys,tempfile,time
mod,lst,out=sys.argv[1:4];hang=int(sys.argv[4]) if len(sys.argv)>4 else 150
base=os.path.expanduser('~/gl-cts-20261105/'+mod)
done=set()
if os.path.exists(out):
    done={json.loads(l)['case'] for l in open(out) if l.strip()}
todo=[c for c in open(lst).read().split() if c not in done]
log=open(out,'a')
def rec(c,s):
    log.write(json.dumps({'case':c,'status':s,'t':time.time()})+'\n');log.flush();done.add(c)
RES=re.compile(r"\s+(Pass|Fail|NotSupported|QualityWarning|CompatibilityWarning|InternalError|ResourceError|Crash|Timeout|Waiver)\b ?\(?(.*)\)?")
env=dict(os.environ,EGL_PLATFORM='surfaceless')
# Cases that legitimately run longer than the default limit (measured idle: copy_image.functional 1055 s).
SLOW={'KHR-GL46.copy_image.functional':2400,'KHR-GL46.texture_swizzle.smoke':1200}
while todo:
    with tempfile.NamedTemporaryFile('w',suffix='.txt',delete=False) as f:
        batch=todo[:200];f.write('\n'.join(batch)+'\n');name=f.name
    p=subprocess.Popen(['stdbuf','-oL','./deqp-'+mod,'--deqp-caselist-file='+name,'--deqp-log-filename=/dev/null','--deqp-log-images=disable','--deqp-log-shader-sources=disable','--deqp-watchdog=disable','--deqp-surface-type=pbuffer','--deqp-gl-config-name=rgba8888d24s8ms0','--deqp-surface-width=256','--deqp-surface-height=256'],cwd=base,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace')
    sel=selectors.DefaultSelector();sel.register(p.stdout,selectors.EVENT_READ)
    cur=None;last=time.time();n=0
    while True:
        if not sel.select(timeout=5):
            pend=cur or next((c for c in batch if c not in done),batch[0])
            if time.time()-last>max(hang,SLOW.get(pend,0)):
                p.kill();rec(pend,'HANG');n+=1;cur=None;break
            continue
        line=p.stdout.readline()
        if not line:break
        last=time.time()
        m=re.match(r"Test case '(.+)'\.\.",line)
        if m:cur=m.group(1);continue
        m=RES.match(line)
        if m and cur:
            rec(cur,m.group(1)+(':'+m.group(2)[:110] if m.group(1) not in('Pass','NotSupported') else ''));cur=None;n+=1
    rc=p.wait();os.unlink(name)
    if cur:rec(cur,'CRASH rc=%d'%rc);n+=1
    if not n:rec(batch[0],'NORUN')
    todo=[c for c in todo if c not in done]
print('DONE',len(done),flush=True)
