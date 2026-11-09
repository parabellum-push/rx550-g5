#!/usr/bin/env python3
"""Stratified list: up to PER cases, evenly spaced, from every subgroup at DEPTH name components."""
import collections,os,sys
base=os.path.expanduser('~/vk-cts-20261102/mustpass/vk-default')
depth,per,skip=int(sys.argv[1]),int(sys.argv[2]),sys.argv[3].split(',')
off=int(sys.argv[4]) if len(sys.argv)>4 else 3
g=collections.defaultdict(list)
for r,_,fs in os.walk(base):
    for f in fs:
        if f.endswith('.txt'):
            for l in open(os.path.join(r,f)):
                c=l.strip()
                if c.startswith('dEQP-VK.') and c.split('.')[1] not in skip:
                    g['.'.join(c.split('.')[:depth])].append(c)
out=[]
for k in sorted(g):
    v=sorted(set(g[k]));step=max(1,len(v)//per);out+=v[off%step::step][:per]
print(len(g),'strata',sum(len(v) for v in g.values()),'cases',len(out),'selected',file=sys.stderr)
print('\n'.join(out))
