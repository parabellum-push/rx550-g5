#!/usr/bin/env python3
"""Fill sha256sums=() of each PKGBUILD here from its local source files (URLs keep their sum, git = SKIP)."""
import hashlib, os, re
here = os.path.dirname(os.path.abspath(__file__))
for d in ('linux-ppc64-rx550g5', 'mesa-rx550g5', 'rx550-g5'):
    p = os.path.join(here, d, 'PKGBUILD'); s = open(p).read()
    src = re.search(r'^source=\((.*?)\)', s, re.S | re.M).group(1)
    items = re.findall(r'"[^"]*"|\S+', src)
    old = re.search(r'^sha256sums=\((.*?)\)', s, re.S | re.M)
    olds = re.findall(r"'[^']*'|\S+", old.group(1))
    sums = []
    for i, it in enumerate(items):
        it = it.strip('"')
        if '::' in it or it.startswith(('git+', 'http')):
            sums.append(olds[i].strip("'") if 'git+' not in it and i < len(olds) else 'SKIP')
        else:
            sums.append(hashlib.sha256(open(os.path.join(here, d, it), 'rb').read()).hexdigest())
    pad = ' ' * len('sha256sums=(')
    new = 'sha256sums=(' + ('\n' + pad).join(f"'{x}'" for x in sums) + ')'
    s = s[:old.start()] + new + s[old.end():]
    open(p, 'w').write(s); print(d, len(sums))
