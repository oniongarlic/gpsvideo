#!/bin/sh
set -x
libtoolize --automake --copy --force
aclocal-1.11 || aclocal
mkdir m4
autoconf --force
autoheader --force
automake-1.11 --add-missing --copy --force-missing --foreign || automake --add-missing --copy --force-missing --foreign
