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
 *  by invoking p3::initClassEntry() from MINIT
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
 *  PHP comparisons will be mapped to the polymorphic compare() method(s)
 *  which should return -1, 0, or 1 consistent with the spaceship operator.
 *  First, attempt to call a type-specific compare function:
 *    $foo <=> true - int compare(bool) const;
 *    $foo <=> 1 - int compare(zend_long) const;
 *    $foo <=> 1.2 - int compare(double) const;
 *    $foo <=> "hello" - int compare(const zend_string*) const;
 *    $foo <=> [1,2,3] - int compare(const zend_array*) const;
 *    $foo <=> $foo2 - int compare(const Foo&) const;
 *    $foo <=> $bar - int compare(const zend_object*) const;
 *    $foo <=> $resource - int compare(const zend_resource*) const;
 *  If a specific comparator is not found, a generic fallback will be attempted:
 *    $foo <=> $whatever - int compare(const zval*) const;
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

namespace {
template<zend_uchar> struct phpType {};
template<typename> struct cppType {};
template<> struct phpType<IS_UNDEF> {
  typedef void type;
  typedef void const_type;
  static type get(zval *pzv) { ZEND_ASSERT(Z_TYPE_P(pzv) == IS_UNDEF); }
  static void make(zval *pzv) { ZVAL_UNDEF(pzv); }
};
template<> struct phpType<IS_NULL> {
  typedef void type;
  typedef void const_type;
  static type get(zval *pzv) { ZEND_ASSERT(Z_TYPE_P(pzv) == IS_NULL); }
  static void make(zval *pzv) { ZVAL_NULL(pzv); }
};
template<> struct cppType<void> { const zend_uchar type = IS_NULL; };
template<> struct phpType<_IS_BOOL> {
  typedef zend_bool type;
  typedef zend_bool const_type;
  static type get(zval *pzv) {
    ZEND_ASSERT((Z_TYPE_P(pzv) == IS_TRUE) || (Z_TYPE_P(pzv) == IS_FALSE));
    return Z_TYPE_P(pzv) == IS_TRUE;
  }
  static void make(zval *pzv, type bval) { ZVAL_BOOL(pzv, bval); }
};
template<> struct cppType<zend_bool> { const zend_uchar type = _IS_BOOL; };
template<> struct phpType<IS_TRUE> {
  typedef zend_bool type;
  typedef zend_bool const_type;
  static type get(zval *pzv) {
    ZEND_ASSERT(Z_TYPE_P(pzv) == IS_TRUE);
    return true;
  }
  static void make(zval *pzv) { ZVAL_TRUE(pzv); }
};
template<> struct phpType<IS_FALSE> {
  typedef zend_bool type;
  typedef zend_bool const_type;
  static type get(zval *pzv) {
    ZEND_ASSERT(Z_TYPE_P(pzv) == IS_FALSE);
    return false;
  }
  static void make(zval *pzv) { ZVAL_FALSE(pzv); }
};
template<> struct phpType<IS_LONG> {
  typedef zend_long type;
  typedef zend_long const_type;
  static type get(zval *pzv) {
    ZEND_ASSERT(Z_TYPE_P(pzv) == IS_LONG);
    return Z_LVAL_P(pzv);
  }
  static void make(zval *pzv, type val) { ZVAL_LONG(pzv, val); }
};
template<> struct cppType<zend_long> { const zend_uchar type = IS_LONG; };
template<> struct phpType<IS_DOUBLE> {
  typedef double type;
  typedef double const_type;
  static type get(zval *pzv) {
    ZEND_ASSERT(Z_TYPE_P(pzv) == IS_DOUBLE);
    return Z_DVAL_P(pzv);
  }
  static void make(zval *pzv, type val) { ZVAL_DOUBLE(pzv, val); }
};
template<> struct cppType<double> { const zend_uchar type = IS_DOUBLE; };
#define P3_DECLARE_GCTYPE_DETAIL(dt, ctype, wrap, unwrap) \
template<> struct phpType<dt> { \
  typedef ctype type; \
  typedef const ctype const_type; \
  static type get(zval *pzv) { \
    ZEND_ASSERT(Z_TYPE_P(pzv) == dt); \
    return unwrap(pzv); \
  } \
  static void make(zval *pzv, type val, bool cpy = true) { \
    wrap(pzv, val); \
    if (cpy) zval_addref_p(pzv); \
  } \
}; \
template<> struct cppType<ctype> { const zend_uchar type = dt; }; \
template<> struct cppType<const ctype> { const zend_uchar type = dt; };
P3_DECLARE_GCTYPE_DETAIL(IS_STRING,   zend_string*,   ZVAL_STR, Z_STR_P)
P3_DECLARE_GCTYPE_DETAIL(IS_ARRAY,    zend_array*,    ZVAL_ARR, Z_ARR_P)
P3_DECLARE_GCTYPE_DETAIL(IS_OBJECT,   zend_object*,   ZVAL_OBJ, Z_OBJ_P)
P3_DECLARE_GCTYPE_DETAIL(IS_RESOURCE, zend_resource*, ZVAL_RES, Z_RES_P)
#undef P3_DECLARE_GCTYPE_DETAIL



// Human Readable names to PHP Data Type map
enum NamedType {
  Undef = IS_UNDEF, Null = IS_NULL,
  Bool = _IS_BOOL, True = IS_TRUE, False = IS_FALSE,
  Long = IS_LONG, Double = IS_DOUBLE,
  String = IS_STRING, Array = IS_ARRAY,
  Object = IS_OBJECT, Resource = IS_RESOURCE
};

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
P3_CREATE_HAS_MEMBER_FN_TRAITS(hasCompare, compare);

#define P3_CREATE_CAST_WRAPPER(ptype) \
template<class T> typename \
  std::enable_if<hasTo##ptype<T, phpType<ptype>::type() const>::value, int>::type \
castObjectTo##ptype(zval *src, zval *dest) { \
  phpType<ptype>::make(dest, toObject<T>(src)->to##ptype()); \
  return SUCCESS; \
} \
template<class T> typename \
  std::enable_if<!hasTo##ptype<T, phpType<ptype>::type() const>::value, int>::type \
castObjectTo##ptype(zval *src, zval *dest) { return FAILURE; }

#define P3_CASTABLE_TYPES(X) X(Bool) X(Long) X(Double) X(String) X(Array)
P3_CASTABLE_TYPES(P3_CREATE_CAST_WRAPPER)

template<class T> typename
  std::enable_if<hasCompare<T, int() const>::value, int>::type \
compareObjectToNull(zval *ret, T *a) {
  ZVAL_LONG(ret, a->compare());
  return SUCCESS;
}
template<class T> typename
  std::enable_if<!hasCompare<T, int() const>::value, int>::type \
compareObjectToNull(zval *ret, T *a) {
  return FAILURE;
}

template<class T> typename
  std::enable_if<hasCompare<T, int(const T&) const>::value,
int>::type compareObjectToSimilar(zval *ret, const T *a, const T &b) {
  ZVAL_LONG(ret, a->compare(b));
  return SUCCESS;
}

template<class T> typename
  std::enable_if<!hasCompare<T, int(const T&) const>::value,
int>::type compareObjectToSimilar(zval *ret, const T *a, const T &b) {
  return FAILURE;
}

template<class T> typename
  std::enable_if<hasCompare<T, int(const zval*) const>::value,
int>::type compareObjectToZval(zval *ret, const T *a, const zval *b) {
  ZVAL_LONG(ret, a->compare(b));
  return SUCCESS;
}

template<class T> typename
  std::enable_if<!hasCompare<T, int(const zval*) const>::value,
int>::type compareObjectToZval(zval *ret, const T *a, const zval *b) {
  return FAILURE;
}

#define P3_CREATE_COMPARE_WRAPPER(ptype) \
template<class T> typename \
  std::enable_if<hasCompare<T, int(phpType<ptype>::const_type) const>::value, \
int>::type compareObjectTo##ptype(zval *ret, const T *a, \
                                  phpType<ptype>::const_type b) { \
  ZVAL_LONG(ret, a->compare(b)); \
  return SUCCESS; \
} \
template<class T> typename \
  std::enable_if<!hasCompare<T, int(phpType<ptype>::const_type) const>::value,\
int>::type compareObjectTo##ptype(zval *ret, const T *a, \
                                  phpType<ptype>::const_type b) { \
  return FAILURE; \
}

#define P3_COMPARABLE_TYPES(X) P3_CASTABLE_TYPES(X) X(Object) X(Resource)
P3_COMPARABLE_TYPES(P3_CREATE_COMPARE_WRAPPER)

#undef P3_CREATE_COMPARE_WRAPPER
#undef P3_CREATE_CAST_WRAPPER
#undef P3_CREATE_HAS_MEMBER_FN_TRAITS
#undef P3_CREATE_HAS_MEMBER_FN_TRAITS_IMPL
} // null namespace

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
    case _IS_BOOL:  return castObjectToBool<T>(src, dest);
    case IS_LONG:   return castObjectToLong<T>(src, dest);
    case IS_DOUBLE: return castObjectToDouble<T>(src, dest);
    case IS_STRING: return castObjectToString<T>(src, dest);
    case IS_ARRAY:  return castObjectToArray<T>(src, dest);
    case IS_OBJECT: ZVAL_ZVAL(dest, src, 1, 0); return SUCCESS;
    case IS_RESOURCE:
    default:
      return FAILURE;
  }
}

// In theory, handlers are as unique as class entry
// Let's hope that assumption holds.
// Otherwise we might have to store T::class_entry
template<class T>
int compareObject(zval *rv, zval *a, zval *b) {
  if ((Z_TYPE_P(a) != IS_OBJECT) || (Z_OBJ_P(a)->handlers != &T::handlers)) {
    // Invert if needed so that 'a' is always a T
    ZEND_ASSERT(Z_TYPE_P(b) == IS_OBJECT);
    ZEND_ASSERT(Z_OBJ_P(b)->handlers == &T::handlers);
    auto ret = compareObject<T>(rv, b, a);
    if ((ret == SUCCESS) && (Z_TYPE_P(rv) == IS_LONG)) {
      ZVAL_LONG(rv, -Z_LVAL_P(rv));
    }
    return ret;
  }

  auto obj = toObject<T>(a);
  if ((Z_TYPE_P(b) == IS_OBJECT) &&
      (Z_OBJ_P(b)->handlers == &T::handlers) &&
      (compareObjectToSimilar<T>(rv, obj, *toObject<T>(b)) == SUCCESS)) {
    // Special case for comparing similar objects
    return SUCCESS;
  }

  int ret = FAILURE;
  switch (Z_TYPE_P(b)) {
    case IS_UNDEF:
    case IS_NULL:
      ret = compareObjectToNull<T>(rv, obj);
      break;
#define P3_COMPARE_T(ptype) \
    case ptype: \
      ret = compareObjectTo##ptype<T>(rv, obj, phpType<ptype>::get(b)); break;
P3_COMPARABLE_TYPES(P3_COMPARE_T)
#undef P3_COMPARE_T
  }
  if (ret == FAILURE) {
    ret = compareObjectToZval<T>(rv, obj, b);
  }
  if (ret == FAILURE) {
    // ZE will crash if rv isn't initialized, even in FAILURE mode
    ZVAL_LONG(rv, 0);
  }
  return ret;
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
  T::handlers.compare = compareObject<T>;

  return pce;
}

#undef P3_CASTABLE_TYPES
#undef P3_COMPARABLE_TYPES
} // namespace p3
#endif
