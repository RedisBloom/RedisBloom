/*
 * Copyright (c) 2006-Present, Redis Ltd.
 * All rights reserved.
 *
 * Licensed under your choice of (a) the Redis Source Available License 2.0
 * (RSALv2); or (b) the Server Side Public License v1 (SSPLv1); or (c) the
 * GNU Affero General Public License v3 (AGPLv3).
 */

#ifndef REBLOOM_VERSION_H_
// This is where the modules build/version is declared.
// If declared with -D in compile time, this file is ignored

#ifndef REBLOOM_VERSION_MAJOR
#define REBLOOM_VERSION_MAJOR 99
#endif

#ifndef REBLOOM_VERSION_MINOR
#define REBLOOM_VERSION_MINOR 99
#endif

#ifndef REBLOOM_VERSION_PATCH
#define REBLOOM_VERSION_PATCH 99

#endif

#define REBLOOM_MODULE_VERSION                                                                     \
    (REBLOOM_VERSION_MAJOR * 10000 + REBLOOM_VERSION_MINOR * 100 + REBLOOM_VERSION_PATCH)

#endif
