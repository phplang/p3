#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "../p3.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

class MyFile {
 public:
  MyFile() {
    // This method is called when $x = new MyFile; is invoked
    // During the ce->create_object call.
    // This happens before MyFile::__construct() is called.

    // To prevent this object from being directly created
    // from userspace, you may delete the default constructor
    // with: `MyFile() = delete;' and an exception will be thrown instead

    // This myfile initializer doesn't need to exist,
    // so it could have been omitted.
  }
  MyFile(const MyFile& that) {
    // This method is called when $x = clone $y; is invoked
    // During the handlers.clone_obj call.

    // To prevent this object from being cloned from userspace,
    // you may delete the default copy constructor with:
    // `MyFile(const MyFile&) = delete;' and an exception will be
    // thrown instead
    fd = dup(that.fd);
  }
  ~MyFile() {
    // This method is called when the object falls out of scope
    // e.g. During `unset($x);` during the handlers.free_obj call.
    close();
  }

  // Only strictly required elements on the class
  // These memebers must exist, and of course
  // the matching implementation in a source file.
  static zend_class_entry* class_entry;
  static zend_object_handlers handlers;

  // Class methods may be stand-alone PHP_METHOD()s
  // or a class may use the P3_METHOD_DECLARE() macro
  // to define a method directly on a class.

  P3_METHOD_DECLARE(__construct) {
    zend_string *name, *mode;

    if (zend_parse_parameters_throw(ZEND_NUM_ARGS(), "SS", &name, &mode)
        == FAILURE) {
      return;
    }

    bool rd = strchr(ZSTR_VAL(mode), 'r');
    bool ap = strchr(ZSTR_VAL(mode), 'a');
    bool wr = strchr(ZSTR_VAL(mode), 'w');
    int flags = 0;
    if (ap|wr) {
      flags |= rd ? O_RDWR : O_WRONLY;
      flags |= ap ? O_APPEND : O_CREAT;
    }

    if (!open(name, flags)) {
      zend_throw_exception_ex(zend_ce_error, 0,
        "Failed opening file %s with flags %d", ZSTR_VAL(name), flags);
    }
  }

  // Static methods:
  static P3_METHOD_DECLARE(getName) {
    RETURN_STRING("MyFile");
  }

  // Forward declare (See implementation below
  // using P3_METHOD()
  P3_METHOD_DECLARE(read);
  P3_METHOD_DECLARE(write);

  // The rest of the object definition is typical
  // stuff you'd find on an object.
  // This myfile demonstrates a MyFile object.

  bool isOpen() const { return fd > -1; }

  bool open(const zend_string* filename, int flags) {
    close();
    fd = ::open(ZSTR_VAL(filename), flags);
    return fd > -1;
  }

  ssize_t write(const zend_string* data) {
    if (!isOpen()) { return -1; }
    return ::write(fd, ZSTR_VAL(data), ZSTR_LEN(data));
  }

  zend_string* read(ssize_t len) {
    if (!isOpen()) { return nullptr; }
    zend_string *ret = zend_string_alloc(len, 0);
    ZSTR_LEN(ret) = ::read(fd, ZSTR_VAL(ret), len);
    if (ZSTR_LEN(ret) < 0) {
      zend_string_release(ret);
      return nullptr;
    }
    ZSTR_VAL(ret)[ZSTR_LEN(ret)] = 0;
    return ret;
  }

  void close() {
    if (!isOpen()) { return; }
    ::close(fd);
    fd = -1;
  }

 private:
  int fd{-1};
};
zend_class_entry *MyFile::class_entry;
zend_object_handlers MyFile::handlers;

/* {{{ proto string MyFile::read(int maxlen) */
ZEND_BEGIN_ARG_INFO_EX(read_arginfo, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, read)
ZEND_END_ARG_INFO()
P3_METHOD(MyFile, read) {
  zend_long len;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &len) == FAILURE) {
    return;
  }

  if (len < 1) {
    zend_throw_exception(zend_ce_error, "Invalid length", 0);
    return;
  }

  zend_string *ret = read(len);
  if (!ret) {
    zend_throw_exception(zend_ce_error, "Failure reading from file", 0);
    return;
  }
  RETURN_STR(ret);
}
/* }}} */

/* {{{ proto int MyFile::write(string maxlen) */
ZEND_BEGIN_ARG_INFO_EX(write_arginfo, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()
P3_METHOD(MyFile, write) {
  zend_string *data;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &data) == FAILURE) {
    return;
  }

  int len = write(data);
  if (len < 0) {
    zend_throw_exception(zend_ce_error, "Failure writing to file", 0);
    return;
  }
  RETURN_LONG(len);
}
/* }}} */

static zend_function_entry php_myfile_methods[] = {
  P3_ME(MyFile, __construct, nullptr, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
  P3_ME(MyFile, read, read_arginfo, ZEND_ACC_PUBLIC)
  P3_ME(MyFile, write, write_arginfo, ZEND_ACC_PUBLIC)
  P3_STATIC_ME(MyFile, getName, nullptr, ZEND_ACC_PUBLIC)
  PHP_FE_END
};


/* {{{ PHP_MINI_FUNCTION */
static PHP_MINIT_FUNCTION(myfile) {
  p3::initClassEntry<MyFile>(
    "MyFile", // PHP visible classname
    php_myfile_methods // User callable methods
  );

  return SUCCESS;
} /* }}} */

/* {{{ myfile_module_entry
 */
static zend_module_entry myfile_module_entry = {
  STANDARD_MODULE_HEADER,
  "myfile",
  nullptr, /* functions */
  PHP_MINIT(myfile),
  nullptr, /* MSHUTDOWN */
  nullptr, /* RINIT */
  nullptr, /* RSHUTDOWN */
  nullptr, /* MINFO */
  "7.2.0-dev",
  STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_MYFILE
ZEND_GET_MODULE(myfile)
#endif

