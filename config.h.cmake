// Compatible with autoheader/configure generated config.h

#define VERSION "${STA_VERSION}"

#define HAVE_PTHREAD_H ${HAVE_PTHREADS}

#if ${ZLIB_FOUND}==TRUE
  #define ZLIB
#endif

#if ${CUDD_FOUND}==TRUE
  #define CUDD
#endif
