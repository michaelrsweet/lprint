//
// ??? for LPrint, a Label Printer Utility
//
// Copyright © 2019 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "lprint.h"


//
// Local functions...
//

static ipp_t		*create_media_col(const char *media, const char *source, const char *type, int width, int length, int bottom, int left, int right, int top);
static ipp_t		*create_media_size(int width, int length);
