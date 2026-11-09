#!/bin/bash
# Build the rx550-g5 support package from pkg-support/tree into the release dir given as $1.
set -e
cd ~/g5-work/pkg-support; T=${T:-tree}
find $T -type d -exec chmod 755 {} +
find $T/usr/share $T/etc $T/usr/lib/systemd $T/usr/lib/udev $T/usr/lib/modprobe.d $T/usr/lib/modules-load.d -type f -exec chmod 644 {} +
(cd $T && find . -type f ! -path "./DEBIAN/*" -printf '%P\n' | sort | xargs md5sum) > $T/DEBIAN/md5sums
sed -i "s/^Installed-Size: .*/Installed-Size: $(du -sk --exclude=DEBIAN $T | cut -f1)/" $T/DEBIAN/control
V=$(sed -n 's/^Version: //p' $T/DEBIAN/control)
dpkg-deb --root-owner-group --build $T "$1/rx550-g5_${V}_all.deb"
