#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "../p3.h"

#include <math.h>

struct Simple {
  P3_METHOD_DECLARE(takeANumber) {
    RETURN_LONG(++counter);
  }

  static zend_class_entry *class_entry;
  static zend_object_handlers handlers;

  bool toBool() const { return counter; }
  zend_long toLong() const { return counter; }
  double toDouble() const { return counter; }
  zend_string* toString() const {
    constexpr int maxlen = 2 + ceil(log(ZEND_LONG_MAX) / log(10));
    zend_string *ret = zend_string_alloc(maxlen, 0);
    ZSTR_LEN(ret) = snprintf(ZSTR_VAL(ret), maxlen, "%ld", (long)counter);
    return ret;
  }

  int compare(zend_long that) const {
    return counter == that ? 0 : ((counter < that) ? -1 : 1);
  }
  int compare(const Simple& that) const { return compare(that.counter); }

 private:
  zend_long counter{0};
};
zend_class_entry *Simple::class_entry;
zend_object_handlers Simple::handlers;

static zend_function_entry php_simple_methods[] = {
  P3_ME(Simple, takeANumber, nullptr, ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static PHP_MINIT_FUNCTION(simple) {
  p3::initClassEntry<Simple>("Simple", php_simple_methods);
  return SUCCESS;
}

static zend_module_entry simple_module_entry = {
  STANDARD_MODULE_HEADER,
  "simple",
  nullptr, /* functions */
  PHP_MINIT(simple),
  nullptr, /* MSHUTDOWN */
  nullptr, /* RINIT */
  nullptr, /* RSHUTDOWN */
  nullptr, /* MINFO */
  "7.2.0-dev",
  STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SIMPLE
ZEND_GET_MODULE(simple)
#endif

