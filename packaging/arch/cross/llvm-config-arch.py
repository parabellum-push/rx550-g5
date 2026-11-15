#!/usr/bin/env python3
"""llvm-config stand-in for Arch POWER ppc64 LLVM 22 in the arch sysroot (no ppc64 exec on the PC).
Answers from lib/cmake/llvm/LLVMConfig.cmake; shared libLLVM only."""
import re,sys
from pathlib import Path
import os
P=Path(os.environ['ARCH_SYSROOT'])/'usr'
cm=(P/'lib/cmake/llvm/LLVMConfig.cmake').read_text()
def cv(n): return re.search(r'set\('+n+r' ([^)]*)\)',cm).group(1).strip('"')
ver=cv('LLVM_PACKAGE_VERSION'); major=ver.split('.')[0]
targets=cv('LLVM_TARGETS_TO_BUILD').split(';')
libs=[l for l in cv('LLVM_AVAILABLE_LIBS').split(';') if l.startswith('LLVM')]
comps={l[4:].lower() for l in libs}
for t in targets:
    t=t.lower(); comps|={t}
comps|={'all','all-targets','native','nativecodegen','engine','mcjit','orcjit','interpreter','ipo','instcombine','scalaropts','transformutils','coroutines','lto','libdriver','windowsdriver','frontendhlsl','frontenddriver','abi','powerpccodegen'}
defs=cv('LLVM_DEFINITIONS')
inc=f'{P}/include'
ans={'--version':ver,'--prefix':str(P),'--bindir':f'{P}/bin','--libdir':f'{P}/lib','--includedir':inc,
 '--cppflags':f'-I{inc} {defs}','--cflags':f'-I{inc} {defs}',
 '--cxxflags':f'-I{inc} -std=c++17 -fno-exceptions -funwind-tables {defs}',
 '--ldflags':f'-L{P}/lib','--system-libs':'','--libs':f'-lLLVM-{major}','--libnames':f'libLLVM-{major}.so',
 '--libfiles':f'{P}/lib/libLLVM-{major}.so','--shared-mode':'shared','--has-rtti':'YES','--build-mode':'RelWithDebInfo',
 '--targets-built':' '.join(targets),'--components':' '.join(sorted(comps)),'--host-target':'powerpc64-unknown-linux-gnu',
 '--assertion-mode':'OFF','--link-shared':None}
out=[]
for a in sys.argv[1:]:
    if a.startswith('--'):
        if a=='--link-static': sys.exit('static LLVM not available')
        if a not in ans: sys.exit('unsupported query '+a)
        if ans[a] is not None: out.append(ans[a])
    elif a not in comps: sys.exit('Unknown component: '+a)
print('\n'.join(out))
