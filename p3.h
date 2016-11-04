/**
 * C++ class wrapper for PHP7
 *
 * Usage:
 *  Given a typical C++ class definition such as:
 *    class Foo {
 *     public:
 *      Foo() {} // construct
 *      Foo(const Foo&) {} // clone
 *      ~Foo() {} // destruct
 *
 *      static zend_object_handlers handlers;
 *    private:
 *     Bar m_data;
 *  };
 *
 *  The class may be wrapped with all basic handlers and hooks defined
 *  by invocing p3::initClassEntry() from MINIT
 *
 *  PHP_MINIT_FUNCTION(myext) {
 *    php_myext_ce = p3::initClassEntry<Foo>("Foo", foo_methods);
 *    return SUCCESS;
 *  }
 *
 *  Additionally, methods on the class may be invoked directly from PHP
 *  by usage of three macros:
 *
 *  P3_METHOD_DECLARE(name)
 *    Declares a PHP callable method on an object
 *      class Foo { public: P3_METHOD_DECLARE(bar); } // $foo->bar()
 *      class Foo { public: static P3_METHOD_DECLARE(bar); } // Foo::bar()
 *      class Foo { public: P3_METHOD_DECLARE(bar) { } } // Inline impl allowed
 *
 *  P3_METHOD(cls, name)
 *    Method implementation signatute, similar to PHP_METHOD(cls, name)
 *    for use outside of the class definition.
 *      P3_METHOD(Foo, bar) { }
 *      Potentially invoked from PHP as $foo->bar() or Foo::bar()
 *        depending on declaration signture.
 *
 *  P3_ME(cls, meth, arginfo, flags)
 *    Bind PHP instance method to class method.
 *      Decodes getThis() into object's `this', then passed through
 *
 *  P3_STATIC_ME(cls, meth, arginfo, flags)
 *    Bind PHP static method to class static method
 *      Basically a vanity implementation of PHP_STATIC_ME
 */

#ifndef incl_PHP_P3_H
#define incl_PHP_P3_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "zend_exceptions.h"

#include <new>
#include <type_traits>

namespace p3 {

template<class T>
zend_object* toZendObject(T *obj) {
  return reinterpret_cast<zend_object*>(obj + 1);
}

template<class T>
T* toObject(zend_object *obj) {
  return reinterpret_cast<T*>(obj) - 1;
}

template<class T>
T* toObject(zval *obj) {
  return reinterpret_cast<T*>(Z_OBJ_P(obj)) - 1;
}

/////////////////////////////////////////////////////////////////////////////

#define P3_METHOD_DECLARE(name) \
  void zim_##name(INTERNAL_FUNCTION_PARAMETERS)

#define P3_METHOD(cls, name) \
  void cls::zim_##name(INTERNAL_FUNCTION_PARAMETERS)

#define P3_ME(cls, meth, arginfo, flags) \
  ZEND_FENTRY(meth, \
    [](INTERNAL_FUNCTION_PARAMETERS) { \
      ::p3::toObject<cls>(getThis()) \
        ->zim_##meth(INTERNAL_FUNCTION_PARAM_PASSTHRU); \
    }, arginfo, flags)

#define P3_STATIC_ME(cls, meth, arginfo, flags) \
  ZEND_FENTRY(meth, &cls::zim_##meth, arginfo, flags | ZEND_ACC_STATIC)

/////////////////////////////////////////////////////////////////////////////

template<class T, typename InitFunc>
zend_object* allocObject(zend_class_entry *ce, InitFunc init) {
  T *ptr = reinterpret_cast<T*>(
    ecalloc(1, sizeof(T) + sizeof(zend_object) +
               zend_object_properties_size(ce))
  );
  init(ptr);
  auto zobj = toZendObject(ptr);
  zend_object_std_init(zobj, ce);
  zobj->handlers = &T::handlers;
  return zobj;
}

template<class T> typename
  std::enable_if<std::is_constructible<T>::value, zend_object*>::type
createObject(zend_class_entry *ce) {
  return allocObject<T>(ce, [](T*ptr) { new(ptr) T(); });
}

template<class T> typename
  std::enable_if<std::is_constructible<T,const T&>::value, zend_object*>::type
cloneObject(zval *oldzval) {
  const T *old = toObject<T>(oldzval);
  return allocObject<T>(
    Z_OBJCE_P(oldzval),
    [old](T*ptr) { new(ptr) T(*old); }
  );
}

template<class T>
void dtorObject(zend_object *obj) {
  zend_object_std_dtor(obj);
  toObject<T>(obj)->~T();
}

// These templates never actually get called,
// they just exist to satisfy expansion from initClassEntry
template<class T> typename
  std::enable_if<!std::is_constructible<T>::value, zend_object*>::type
createObject(zend_class_entry *ce) {
  assert(false);
  return nullptr;
}

template<class T> typename
  std::enable_if<!std::is_constructible<T,const T&>::value, zend_object*>::type
cloneObject(zval *oldzval) {
  assert(false);
  return nullptr;
}

// Throw an exception when trying to directly instantiate
// a class when no zero-argument constructor is available
template<class T>
zend_object* createThrownObject(zend_class_entry *ce) {
  zend_throw_exception_ex(zend_ce_error, 0,
    "%s may not be directly instantiated", ZSTR_VAL(ce->name));
  return zend_objects_new(ce);
}

template<class T>
zend_class_entry* initClassEntry(
  const char *name,
  const zend_function_entry *methods) {

  zend_class_entry ce;
  INIT_CLASS_ENTRY_EX(ce, name, strlen(name), methods);
  auto pce = zend_register_internal_class(&ce);
  pce->create_object = std::is_constructible<T>::value
    ? createObject<T> : createThrownObject<T>;

  memcpy(&T::handlers, zend_get_std_object_handlers(),
         sizeof(zend_object_handlers));
  T::handlers.offset = sizeof(T);
  T::handlers.free_obj = dtorObject<T>;
  T::handlers.clone_obj = std::is_constructible<T,const T&>::value
    ? cloneObject<T> : nullptr;

  return pce;
}

} // namespace p3
#endif
