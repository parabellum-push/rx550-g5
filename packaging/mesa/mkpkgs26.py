#!/usr/bin/env python3
"""Lay a Mesa 26.2.6 build out as replacements for the Debian 26.2.6-1 ppc64 binary packages.

Every package gets exactly the stock package's file list (<pkg>.list, from dpkg-deb -c on the
verified debian-ports debs). Binaries come from our build (STAGE). Files the build does not
produce (Debian docs, lintian overrides, bug hooks) come from the stock package (stock/).
Control fields and maintainer scripts are the stock ones with the new version.

The one name that changes is libgallium-<version>.so: Debian names it after the package
version (debian/rules writes it to VERSION), and every driver records that exact name as
NEEDED, so the stage's own libgallium name is used.

usage: mkpkgs26.py STAGE OUTDIR VERSION DIFF
"""
import glob, gzip, hashlib, os, shutil, stat, subprocess, sys, time

STAGE, OUT, VER, DIFF = sys.argv[1:5]
HERE = os.path.dirname(os.path.abspath(__file__))
STOCKVER = "26.2.6-1"
L = "usr/lib/powerpc64-linux-gnu"
PKGS = ["mesa-libgallium", "libgbm1", "libegl-mesa0", "libglx-mesa0", "libgl1-mesa-dri",
        "mesa-va-drivers", "mesa-vdpau-drivers", "mesa-vulkan-drivers", "mesa-opencl-icd"]
# Debian builds rusticl only for RUSTICL_ARCHS (not ppc64); this package follows the ppc64el one
# (pkg/stockctl/mesa-opencl-icd/SOURCE). Its file list and docs come from that package.
PPC64EL_REF = {"mesa-opencl-icd"}
STRIP = "powerpc64-linux-gnu-strip"
# Old diversion package from the early port; the 24.1.6 rx550g5.1 release already replaced it.
OLDPKG = "rx550-g5-be-mesa"

gallium = [os.path.basename(p) for p in glob.glob(os.path.join(STAGE, L, "libgallium-*.so"))]
if len(gallium) != 1:
    sys.exit(f"expected one libgallium-*.so in the stage, found {gallium}")
GALLIUM = f"{L}/{gallium[0]}"
if gallium[0] != f"libgallium-{VER}.so":
    print(f"warning: stage has {gallium[0]}, not libgallium-{VER}.so (not a release build)")
RENAME = {f"{L}/libgallium-{STOCKVER}.so": GALLIUM}

ENTRY = f"""mesa ({VER}) unstable; urgency=medium

  * Unofficial rebuild for the Radeon RX 550 (Polaris, GFX8) on big-endian
    PowerPC (Power Mac G5). Same drivers and options as the Debian ppc64
    build of {STOCKVER}, from Mesa 26.2.6 with the Debian patches plus the
    big-endian GFX8 fixes listed in /usr/share/doc/libgl1-mesa-dri/README.rx550-g5.

 -- RX550 G5 port <root@localhost>  {time.strftime('%a, %d %b %Y %H:%M:%S +0000', time.gmtime())}

"""


def scrub_dynstr(path):
    """Blank build-host paths left in .dynstr by the build-time rpath that meson removed at
    install. Refuses if any RPATH/RUNPATH entry is still present."""
    dyn = subprocess.check_output(["powerpc64-linux-gnu-readelf", "-d", path], text=True)
    if "RPATH" in dyn or "RUNPATH" in dyn:
        sys.exit(f"{path}: still has an rpath")
    sec = subprocess.check_output(["powerpc64-linux-gnu-readelf", "-W", "-S", path], text=True)
    for line in sec.splitlines():
        f = line.replace("[ ", "[").split()
        if len(f) > 6 and f[1] == ".dynstr":
            off, size = int(f[4], 16), int(f[5], 16)
            break
    else:
        return 0
    data = bytearray(open(path, "rb").read())
    tab = data[off:off + size]
    n, i = 0, 0
    while i < len(tab):
        j = tab.index(0, i)
        if b"/home/" in tab[i:j]:
            tab[i:j] = bytes(j - i)
            n += 1
        i = j + 1
    data[off:off + size] = tab
    open(path, "wb").write(data)
    return n


def is_elf(p):
    with open(p, "rb") as f:
        return f.read(4) == b"\x7fELF"


def stock_text(pkg, kind):
    p = os.path.join(HERE, "stockctl", pkg, kind)
    return open(p).read() if os.path.exists(p) else None


def build(pkg):
    root = os.path.join(OUT, "tree", pkg)
    shutil.rmtree(root, ignore_errors=True)
    os.makedirs(os.path.join(root, "DEBIAN"))
    inode_first = {}
    from_stock = []
    for line in open(os.path.join(HERE, pkg + ".list")):
        path = line.strip().lstrip("/")
        if not path or path == ".":
            continue
        path = RENAME.get(path, path)
        dst = os.path.join(root, path)
        stockp = os.path.join(HERE, "stock", path)
        stagep = os.path.join(STAGE, path)
        if (os.path.isdir(stagep) and not os.path.islink(stagep)) or \
           (os.path.isdir(stockp) and not os.path.islink(stockp)):
            os.makedirs(dst, exist_ok=True)
            continue
        src = stagep
        if not os.path.lexists(src):
            src = stockp
            if not os.path.islink(src) and \
               not path.startswith(("usr/share/doc/", "usr/share/lintian/", "usr/share/bug/")):
                from_stock.append(path)
        if not os.path.lexists(src):
            sys.exit(f"{pkg}: no source for {path}")
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        if os.path.islink(src):
            os.symlink(os.readlink(src), dst)
            continue
        st = os.stat(src)
        if st.st_nlink > 1 and (st.st_dev, st.st_ino) in inode_first:
            os.link(inode_first[(st.st_dev, st.st_ino)], dst)
            continue
        shutil.copy2(src, dst)
        os.chmod(dst, 0o644 if not (st.st_mode & stat.S_IXUSR) else 0o755)
        if is_elf(dst) and src == stagep:
            subprocess.check_call([STRIP, "--strip-unneeded", "--remove-section=.comment",
                                   "--remove-section=.note", dst])
            scrub_dynstr(dst)
            os.chmod(dst, 0o644)
        inode_first[(st.st_dev, st.st_ino)] = dst
    if from_stock:
        sys.exit(f"{pkg}: build lacks non-doc files {from_stock}")

    doc = os.path.join(root, "usr/share/doc", pkg)
    if os.path.isdir(doc):
        cl = os.path.join(doc, "changelog.Debian.gz")
        old = gzip.open(cl).read() if os.path.exists(cl) else b""
        if os.path.islink(cl):
            os.unlink(cl)
        with gzip.GzipFile(cl, "wb", mtime=0) as f:
            f.write(ENTRY.encode() + old)
        if pkg == "libgl1-mesa-dri":
            shutil.copy(os.path.join(HERE, "README.rx550-g5"), os.path.join(doc, "README.rx550-g5"))
            with open(DIFF, "rb") as i, \
                 gzip.GzipFile(os.path.join(doc, "rx550-g5-vs-mesa-26.2.6.diff.gz"), "wb", mtime=0) as o:
                o.write(i.read())

    for kind in ("conffiles", "shlibs", "symbols", "triggers", "preinst", "postinst", "prerm", "postrm"):
        text = stock_text(pkg, kind)
        if text is None:
            continue
        if kind == "shlibs":
            text = text.replace(f"libgallium-{STOCKVER}", os.path.basename(GALLIUM)[:-3])
        d = os.path.join(root, "DEBIAN", kind)
        open(d, "w").write(text)
        os.chmod(d, 0o755 if kind.endswith(("inst", "rm")) else 0o644)

    os.chmod(root, 0o755)
    size, sums, seen = 0, [], set()
    for dp, dn, fn in os.walk(root):
        if dp.endswith("/DEBIAN"):
            continue
        for d in dn:
            os.chmod(os.path.join(dp, d), 0o755)
        for f in fn:
            p = os.path.join(dp, f)
            if os.path.islink(p):
                size += 1
                continue
            st = os.lstat(p)
            if (st.st_dev, st.st_ino) not in seen:
                seen.add((st.st_dev, st.st_ino))
                size += st.st_size
            sums.append(f"{hashlib.md5(open(p, 'rb').read()).hexdigest()}  {os.path.relpath(p, root)}")
    with open(os.path.join(root, "DEBIAN/md5sums"), "w") as f:
        f.write("\n".join(sorted(sums)) + "\n")

    fields, desc_lines, key = {}, [], None
    for l in stock_text(pkg, "control").splitlines():
        if l.startswith(" ") and key == "Description":
            desc_lines.append(l)
            continue
        key, _, v = l.partition(": ")
        fields[key] = v.replace(f"(= {STOCKVER})", f"(= {VER})").replace(f"(>= {STOCKVER})", f"(>= {VER})")
    ctl = [f"Package: {pkg}", "Source: mesa", f"Version: {VER}", "Architecture: ppc64",
           "Maintainer: RX550 G5 port <root@localhost>",
           f"Original-Maintainer: {fields['Maintainer']}",
           f"Installed-Size: {(size + 1023) // 1024}"]
    for k in ("Multi-Arch", "Section", "Priority", "Pre-Depends", "Depends", "Recommends",
              "Suggests", "Provides"):
        if k in fields:
            ctl.append(f"{k}: {fields[k]}")
    for k, extra in (("Breaks", None), ("Conflicts", OLDPKG), ("Replaces", OLDPKG)):
        v = ", ".join(x for x in (fields.get(k), extra) if x)
        if v:
            ctl.append(f"{k}: {v}")
    ctl.append(f"Homepage: {fields['Homepage']}")
    ctl.append(f"Description: {fields['Description']} (RX550 big-endian build)")
    ctl.append(" Unofficial rebuild of Mesa 26.2.6 for the Radeon RX 550 on big-endian PowerPC")
    if pkg in PPC64EL_REF:
        ctl.append(" (Power Mac G5). Debian does not build this package for ppc64; it follows")
        ctl.append(f" the Debian ppc64el package of {STOCKVER}, with rusticl enabled on radeonsi")
        ctl.append(" by default; see /usr/share/doc/libgl1-mesa-dri/README.rx550-g5.")
    else:
        ctl.append(" (Power Mac G5). Drivers and options match the Debian ppc64 build of")
        ctl.append(f" {STOCKVER}; see /usr/share/doc/libgl1-mesa-dri/README.rx550-g5.")
    ctl.append(" .")
    ctl.extend(desc_lines)
    with open(os.path.join(root, "DEBIAN/control"), "w") as f:
        f.write("\n".join(ctl) + "\n")

    deb = os.path.join(OUT, f"{pkg}_{VER}_ppc64.deb")
    subprocess.check_call(["dpkg-deb", "--root-owner-group", "-Zxz", "--build", root, deb],
                          stdout=subprocess.DEVNULL)
    print(f"{deb}  {os.path.getsize(deb) // 1024} KiB")


os.makedirs(OUT, exist_ok=True)
for p in PKGS:
    build(p)
