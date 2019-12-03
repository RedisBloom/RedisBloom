#ifndef REBLOOM_VERSION_H_
// This is where the modules build/version is declared.
// If declared with -D in compile time, this file is ignored


#ifndef REBLOOM_VERSION_MAJOR
#define REBLOOM_VERSION_MAJOR 2
#endif

#ifndef REBLOOM_VERSION_MINOR
#define REBLOOM_VERSION_MINOR 2
#endif

#ifndef REBLOOM_VERSION_PATCH
#define REBLOOM_VERSION_PATCH 0

#endif

#define REBLOOM_MODULE_VERSION \
  (REBLOOM_VERSION_MAJOR * 10000 + REBLOOM_VERSION_MINOR * 100 + REBLOOM_VERSION_PATCH)

#endif
