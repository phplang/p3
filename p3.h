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
 *  PHP casts will be mapped to conversion methods on the object, if they exist.
 *  Implement as many (or as few) as make sense for your class.
 *    (null)$foo - No proto required, just returns NULL
 *    (bool)$foo - bool toBool() const;
 *    (int)$foo - zend_long toLong() const;
 *    (float)$foo - double toDouble() const;
 *    (string)$foo - zend_string* toString() const;
 *    (array)$foo - zend_array* toArray() const;
 *    (object)$foo - No proto required, jsut returns $foo unmodified
 *    (resource)$foo - Throws exception
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

namespace detail {
// Borrowed from FOLly https://github.com/facebook/folly
#define P3_CREATE_HAS_MEMBER_FN_TRAITS_IMPL(classname, func_name, cv_qual) \
  template <typename TTheClass_, typename RTheReturn_, typename... TTheArgs_> \
  class classname<TTheClass_, RTheReturn_(TTheArgs_...) cv_qual> { \
    template < \
      typename UTheClass_, RTheReturn_ (UTheClass_::*)(TTheArgs_...) cv_qual \
    > struct sfinae {}; \
    template <typename UTheClass_> \
    constexpr static bool test(sfinae<UTheClass_, &UTheClass_::func_name>*) \
    { return true; } \
    template <typename> \
    constexpr static bool test(...) { return false; } \
  public: \
    constexpr static bool value = test<TTheClass_>(nullptr); \
  }
#define P3_CREATE_HAS_MEMBER_FN_TRAITS(classname, func_name) \
  template <typename, typename> class classname; \
  P3_CREATE_HAS_MEMBER_FN_TRAITS_IMPL(classname, func_name, ); \
  P3_CREATE_HAS_MEMBER_FN_TRAITS_IMPL(classname, func_name, const); \
  P3_CREATE_HAS_MEMBER_FN_TRAITS_IMPL( \
      classname, func_name, /* nolint */ volatile); \
  P3_CREATE_HAS_MEMBER_FN_TRAITS_IMPL( \
      classname, func_name, /* nolint */ volatile const)

P3_CREATE_HAS_MEMBER_FN_TRAITS(hasToBool, toBool);
P3_CREATE_HAS_MEMBER_FN_TRAITS(hasToLong, toLong);
P3_CREATE_HAS_MEMBER_FN_TRAITS(hasToDouble, toDouble);
P3_CREATE_HAS_MEMBER_FN_TRAITS(hasToString, toString);
P3_CREATE_HAS_MEMBER_FN_TRAITS(hasToArray, toArray);

#define P3_CREATE_CAST_WRAPPER(ptype, ctype, zwrap) \
template<class T> typename \
  std::enable_if<hasTo##ptype<T, ctype() const>::value, int>::type \
castObjectTo##ptype(zval *src, zval *dest) { \
  zwrap(dest, toObject<T>(src)->to##ptype()); \
  return SUCCESS; \
} \
template<class T> typename \
  std::enable_if<!hasTo##ptype<T, ctype() const>::value, int>::type \
castObjectTo##ptype(zval *src, zval *dest) { return FAILURE; }

P3_CREATE_CAST_WRAPPER(Bool, bool, ZVAL_BOOL)
P3_CREATE_CAST_WRAPPER(Long, zend_long, ZVAL_LONG)
P3_CREATE_CAST_WRAPPER(Double, double, ZVAL_DOUBLE)
P3_CREATE_CAST_WRAPPER(String, zend_string*, ZVAL_STR)
P3_CREATE_CAST_WRAPPER(Array, zend_array*, ZVAL_ARR)

#undef P3_CREATE_CAST_WRAPPER
#undef P3_CREATE_HAS_MEMBER_FN_TRAITS
#undef P3_CREATE_HAS_MEMBER_FN_TRAITS_IMPL
} // namespace detail

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

template<class T>
int castObject(zval *src, zval *dest, int type) {
  switch (type) {
    case IS_UNDEF:  ZVAL_UNDEF(dest); return SUCCESS;
    case IS_NULL:   ZVAL_NULL(dest);  return SUCCESS;
    case _IS_BOOL:  return detail::castObjectToBool<T>(src, dest);
    case IS_LONG:   return detail::castObjectToLong<T>(src, dest);
    case IS_DOUBLE: return detail::castObjectToDouble<T>(src, dest);
    case IS_STRING: return detail::castObjectToString<T>(src, dest);
    case IS_ARRAY:  return detail::castObjectToArray<T>(src, dest);
    case IS_OBJECT: ZVAL_ZVAL(dest, src, 1, 0); return SUCCESS;
    case IS_RESOURCE:
    default:
      return FAILURE;
  }
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
  T::handlers.cast_object = castObject<T>;

  return pce;
}

} // namespace p3
#endif
