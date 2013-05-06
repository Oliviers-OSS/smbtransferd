#!/bin/sh

if [ "x$1" = "x-x" ] ; then
  set -x
  shift
fi

CLEARTOOL=/usr/atria/bin/cleartool

function checkInIfDifferent {
  modifications=`$CLEARTOOL diff -diff_format -predecessor $1`
  if [ -n "$modifications" ] ; then
     $CLEARTOOL ci -nwarn -ptime $1
  else
     $CLEARTOOL unco -rm $1
  fi
}

export PATH=/applis/automake_1.10/bin:/applis/autoconf_2.61/bin:$PATH

export CPPFLAGS="-I$PWD/../dbgflags/include -I$TAO_ROOT/include -I$ACE_TAO_HOME/include -I$TAO_ROOT/orbsvcs -I$ACE_TAO_HOME/include/TAO"

export LDFLAGS="-L$PWD/../dbgflags/libdbgflags/.libs -L$ACE_TAO_HOME/lib"
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/../dbgflags/libdbgflags/.libs

$CLEARTOOL co -nc -unreserved aclocal.m4
$CLEARTOOL co -nc configure
$CLEARTOOL co -nc config.h.in
$CLEARTOOL co -nc Makefile.in

./configure  --enable-corba --enable-debug --enable-CORBA-kind=no && make && make dist && make dist-bin-rpm

checkInIfDifferent configure
checkInIfDifferent config.h.in
checkInIfDifferent Makefile.in
$CLEARTOOL unco -rm aclocal.m4
