dnl $Id$
dnl config.m4 for extension myfile

PHP_ARG_ENABLE(myfile, whether to enable myfile support,
[  --disable-myfile    Disable myfile support], yes)

if test "$PHP_MYFILE" != "no"; then
  PHP_REQUIRE_CXX()
  PHP_NEW_EXTENSION(myfile, myfile.cpp, $ext_shared,, -std=c++11 )
fi
