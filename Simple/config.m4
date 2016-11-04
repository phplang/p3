dnl $Id$
dnl config.m4 for extension simple

PHP_ARG_ENABLE(simple, whether to enable simple support,
[  --disable-simple    Disable simple support], yes)

if test "$PHP_SIMPLE" != "no"; then
  PHP_REQUIRE_CXX()
  PHP_NEW_EXTENSION(simple, simple.cpp, $ext_shared,, -std=c++11 )
fi
