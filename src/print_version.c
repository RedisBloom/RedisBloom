 /*
 * Copyright (c) 2006-Present, Redis Ltd.
 * All rights reserved.
 *
 * Licensed under your choice of (a) the Redis Source Available License 2.0
 * (RSALv2); or (b) the Server Side Public License v1 (SSPLv1); or (c) the
 * GNU Affero General Public License v3 (AGPLv3).
 */

#ifdef PRINT_VERSION_TARGET
#include <stdio.h>
#include "version.h"

/* This is a utility that prints the current semantic version string, to be used in make files */

int main(int argc, char **argv) {
    printf("%d.%d.%d\n", REBLOOM_VERSION_MAJOR, REBLOOM_VERSION_MINOR, REBLOOM_VERSION_PATCH);
    return 0;
}
#endif
