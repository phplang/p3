#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "../p3.h"

struct Simple {
  P3_METHOD_DECLARE(takeANumber) {
    RETURN_LONG(++counter);
  }

  static zend_object_handlers handlers;
 private:
  int counter{0};
};
zend_class_entry *php_simple_ce;
zend_object_handlers Simple::handlers;

static zend_function_entry php_simple_methods[] = {
  P3_ME(Simple, takeANumber, nullptr, ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static PHP_MINIT_FUNCTION(simple) {
  php_simple_ce = p3::initClassEntry<Simple>("Simple", php_simple_methods);
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

