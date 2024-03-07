IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # 1. Turn on warnings and errors.
  # SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")

  # 2. Turn off errors and fix it latter.
  # Turn off the errors, raise as warnings instead.
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=sign-compare")
  # CLang not support this option.
  IF(CMAKE_COMPILER_IS_GNUCXX)
    # Turn off the errors, raise as warnings instead.
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=unused-but-set-variable")
  ENDIF()  
  # Not too many warnings, should fix it.
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-overloaded-virtual")
  # Not too many warnings, should fix it.
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-suggest-override")
  # There are more than 10, 000 warnings for this option!
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
  # Features like std::unique_ptr, throw in function declaration, hash_(set|map).
  # std::unique_ptr is replaced with unique_ptr, other smart pointers are harder to fix.
  # throw in funciton declaration is replaced with noexcept.
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated")
  # Turn off -fstrict-aliasing, which is turned on by -O2 or above. See also
  # https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html#Optimize-Options
  #
  # Culprits in bintools.h:
  # bool IsDoubleNull(const double d) {return *(int64*)&d == NULL_VALUE_64;}
  # Most of the culprits are in RCDataTypes.h. If turned on, code like this
  # may be optimized out, and lead to errors.
  # void Negate() { *(int64*)&dt = *(int64*)&dt * -1; }
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing")
  # Turn off warnings about implicit casting away const.
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-cast-qual")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=undef")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=suggest-attribute=format")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=dangling-else")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=implicit-fallthrough=")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=unused-value")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=class-memaccess")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=format-truncation=")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=extra")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=nonnull-compare")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=parentheses")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=literal-suffix")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=misleading-indentation")
  # [-Werror=extra-semi]
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=extra-semi")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=unused-variable")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-narrowing")
  # [-Wundef]
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-undef")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-extra-semi")
ENDIF()

