//
// Common definitions file for LPrint, a Label Printer Utility
//
// Copyright © 2019 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _COMMON_H_
#  define _COMMON_H_

//
// Include necessary headers...
//

#  include "config.h"
#  include <cups/cups.h>
#  include <pthread.h>


//
// Function annotations...
//

#  if defined(__GNUC__) || defined(__has_extension)
#    define LPRINT_FORMAT(a,b)	__attribute__ ((__format__(__printf__, a,b)))
#    define LPRINT_NONNULL(...)	__attribute__ ((nonnull(__VA_ARGS__)))
#    define LPRINT_NORETURN	__attribute__ ((noreturn))
#  else
#    define LPRINT_FORMAT(a,b)
#    define LPRINT_NONNULL(...)
#    define LPRINT_NORETURN
#  endif // __GNUC__ || __has_extension


#endif // !_COMMON_H_
