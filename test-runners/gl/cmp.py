#!/usr/bin/env python3
"""cmp.py BASE NEW: per-case comparison of two run_gl.py / run_cts_list.py result files."""
import json, sys, collections


def load(p):
    d = {}
    for l in open(p):
        if l.strip():
            x = json.loads(l)
            d[x["case"]] = x["status"]
    return d


OK = ("Pass", "NotSupported", "QualityWarning", "CompatibilityWarning")
a, b = load(sys.argv[1]), load(sys.argv[2])
good = lambda s: s.startswith(OK)
reg = [c for c in b if c in a and good(a[c]) and not good(b[c])]
fix = [c for c in b if c in a and not good(a[c]) and good(b[c])]
counts = collections.Counter(s.split(':')[0] for s in b.values())
print(f"{sys.argv[2]}: {len(b)}/{len(a)} run; {dict(counts)}")
print(f"  regressions {len(reg)}, fixed {len(fix)}, missing {len([c for c in a if c not in b])}")
for c in reg:
    print("  REG", c, b[c][:70])
for c in fix:
    print("  FIX", c, b[c][:40])
