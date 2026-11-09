#!/usr/bin/env python3
"""Run a case list, appending each result to a JSONL file as it finishes, so a kill loses nothing.
Usage: run_cts_list.py LIST OUT.jsonl [HANG_SECONDS]. Already recorded cases are skipped on restart."""
import json,os,re,selectors,subprocess,sys,tempfile,time
base=os.path.expanduser('~/vk-cts-20261102')
lst,out=sys.argv[1],sys.argv[2];hang=int(sys.argv[3]) if len(sys.argv)>3 else 150
done=set()
if os.path.exists(out):
    done={json.loads(l)['case'] for l in open(out) if l.strip()}
todo=[c for c in open(lst).read().split() if c not in done]
log=open(out,'a')
def rec(c,s):
    log.write(json.dumps({'case':c,'status':s,'t':int(time.time())})+'\n');log.flush();done.add(c)
RES=re.compile(r"\s+(Pass|Fail|NotSupported|QualityWarning|CompatibilityWarning|InternalError|ResourceError|Crash|Timeout|Waiver)\b ?\(?(.*)\)?")
while todo:
    batch=todo[:200]
    with tempfile.NamedTemporaryFile('w',suffix='.txt',delete=False) as f:
        f.write('\n'.join(batch)+'\n');name=f.name
    p=subprocess.Popen(["stdbuf","-oL",os.environ.get("DEQP","./deqp-vk"),'--deqp-caselist-file='+name,'--deqp-log-filename=/dev/null','--deqp-log-images=disable','--deqp-log-shader-sources=disable','--deqp-watchdog=disable'],cwd=base,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace')
    sel=selectors.DefaultSelector();sel.register(p.stdout,selectors.EVENT_READ)
    cur=None;last=time.time();n=0
    while True:
        if not sel.select(timeout=5):
            if time.time()-last>hang:
                p.kill();rec(cur or next((c for c in batch if c not in done),batch[0]),'HANG');n+=1;cur=None;break
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
