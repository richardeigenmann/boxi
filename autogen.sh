#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
srcdir=`cd $srcdir && pwd`
PKG_NAME="the package."

DIE=0

mkdir -p boxbackup
touch boxbackup/Makefile.in

ACLOCAL_FLAGS="$ACLOCAL_FLAGS -I $srcdir/boxbackup/infrastructure/m4"

HOSTDIR=$HOME/`hostname`/share/aclocal
if [ -d "$HOSTDIR" ]; then
	ACLOCAL_FLAGS="$ACLOCAL_FLAGS -I $HOSTDIR"
fi

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**Error**: You must have \`autoconf' installed to."
  echo "Download the appropriate package for your distribution,"
  echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
  DIE=1
}

LIBTOOLIZE=libtoolize
LIBTOOL=libtool

if libtoolize --help >/dev/null 2>&1
then
  :
elif glibtoolize --help >/dev/null 2>&1
then
  LIBTOOLIZE=glibtoolize
  LIBTOOL=glibtool
fi

(grep "^AM_PROG_LIBTOOL" $srcdir/configure.in >/dev/null) && {
  ($LIBTOOL --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have \`libtool' installed."
    echo "Get ftp://ftp.gnu.org/pub/gnu/"
    echo "(or a newer version if it is available)"
    DIE=1
  }
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**Error**: You must have \`automake' installed."
  echo "Get ftp://ftp.gnu.org/pub/gnu/"
  echo "(or a newer version if it is available)"
  DIE=1
  NO_AUTOMAKE=yes
}

# if no automake, don't bother testing for aclocal
test -n "$NO_AUTOMAKE" || (aclocal --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**Error**: Missing \`aclocal'.  The version of \`automake'"
  echo "installed doesn't appear recent enough."
  echo "Get ftp://ftp.gnu.org/pub/gnu/"
  echo "(or a newer version if it is available)"
  DIE=1
}

if test "$DIE" -eq 1; then
  exit 1
fi

if test -z "$*"; then
  echo "**Warning**: I am going to run \`configure' with no arguments."
  echo "If you wish to pass any to it, please specify them on the"
  echo \`$0\'" command line."
  echo
fi

case $CC in
xlc )
  am_opt=--include-deps;;
esac

for coin in `find $srcdir -name configure.in -print`
do 
  dr=`dirname $coin`
  if test -f $dr/NO-AUTO-GEN; then
    echo skipping $dr -- flagged as no auto-gen
  else
    echo processing $dr
    macrodirs=`sed -n -e 's,AM_ACLOCAL_INCLUDE(\(.*\)),\1,gp' < $coin`
    ( cd $dr
      aclocalinclude="$ACLOCAL_FLAGS"
      macrodirs="$macrodirs /usr/local/share/aclocal"
      for k in $macrodirs; do
  	    if test -d $k; then
          aclocalinclude="$aclocalinclude -I $k"
  	  ##else 
	    ##  echo "**Warning**: No such directory \`$k'.  Ignored."
        fi
      done
      aclocalinclude="$aclocalinclude"
      if grep "^AM_GNU_GETTEXT" configure.in >/dev/null; then
	if grep "sed.*POTFILES" configure.in >/dev/null; then
	  : do nothing -- we still have an old unmodified configure.in
	else
	  echo "Creating $dr/aclocal.m4 ..."
	  test -r $dr/aclocal.m4 || touch $dr/aclocal.m4
	  echo "Running gettextize...  Ignore non-fatal messages."
	  ./setup-gettext
	  echo "Making $dr/aclocal.m4 writable ..."
	  test -r $dr/aclocal.m4 && chmod u+w $dr/aclocal.m4
        fi
      fi
      if grep "^(AC)|(IT)_PROG_INTLTOOL" configure.in >/dev/null; then
        echo "Running intltoolize..."
        intltoolize --copy --force --automake
      fi
      if grep "^AM_GNOME_GETTEXT" configure.in >/dev/null; then
	echo "Creating $dr/aclocal.m4 ..."
	test -r $dr/aclocal.m4 || touch $dr/aclocal.m4
	echo "Running gettextize...  Ignore non-fatal messages."
	./setup-gettext
	echo "Making $dr/aclocal.m4 writable ..."
	test -r $dr/aclocal.m4 && chmod u+w $dr/aclocal.m4
      fi
      if grep "^AM_PROG_LIBTOOL" configure.in >/dev/null; then
	echo "Running $LIBTOOLIZE..."
	$LIBTOOLIZE --force --copy
      fi
      test -d m4 && aclocalinclude="$aclocalinclude -I m4"
      echo "Running aclocal $aclocalinclude ..."
      if ! aclocal $aclocalinclude; then
        echo "Error: aclocal failed, aborting." >&2
        exit 2
      fi
      if grep "^AM_CONFIG_HEADER" configure.in >/dev/null; then
	echo "Running autoheader..."
	autoheader
      fi
      echo "Running automake --gnu $am_opt ..."
      automake --add-missing --gnu $am_opt
      echo "Running autoconf ..."
      autoconf
    ) || exit $?
  fi
done

./make-image-headers.pl

#conf_flags="--enable-maintainer-mode --enable-compile-warnings" #--enable-iso-c

./update-main-h.sh

if test x$NOCONFIGURE = x; then
  echo Running $srcdir/configure $conf_flags "$@" ...
  CXXFLAGS="-g -O0" $srcdir/configure $conf_flags "$@" \
  && echo Now type \`make\' to compile $PKG_NAME
else
  echo Skipping configure process.
fi
