# P3: PHP Plus Plus

This repository contains a single useful item: [p3.h](https://github.com/phplang/p3/tree/master/p3.h).
This file may be freely (BSD License) included in any PHP extension project to quickly and easily
wrap a C++ class into a PHP class.  The C++11 templates take care of all the boilerplate methods
like allocators, constructors, cloning, and reserving extra space for the base `zend_object`.

Two example extensions have been included in this repo:

  * [MyFile](https://github.com/phplang/p3/tree/master/MyFile/) is a very basic File implementation with read/write methods.
  * [Simple](https://github.com/phplang/p3/tree/master/Simple/) is an even simpler object which does nothing but track a counter var.

The inline documentation in `p3.h` itself, as well as `MyFile` should provide enough information.
`Simple` is deliberately undocumented as it's meant to show how small the code for a custom object can be.

Otherwise, the key items to note are `PHP_REQUIRE_CXX()` and `-std=c++11` in `config.m4`.

## Future Plans

  * dim/obj read/write/isset/unset accessors
  * Wrapped property access (short-hand for above)
  * Comparator support (e.g. operator< operator> operator==)

