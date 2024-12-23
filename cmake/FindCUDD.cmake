################################################################
#
# Locate CUDD bdd package.
#
################################################################
find_library(CUDD_LIB
  NAME cudd
  PATHS ${CUDD_DIR}
  PATH_SUFFIXES lib lib/cudd cudd/.libs
  )
if (CUDD_LIB)
  message(STATUS "CUDD library: ${CUDD_LIB}")
  get_filename_component(CUDD_LIB_DIR "${CUDD_LIB}" PATH)
  get_filename_component(CUDD_LIB_PARENT1 "${CUDD_LIB_DIR}" PATH)
  find_file(CUDD_HEADER cudd.h
    PATHS ${CUDD_LIB_PARENT1} ${CUDD_LIB_PARENT1}/include ${CUDD_LIB_PARENT1}/include/cudd)
  if (CUDD_HEADER)
    get_filename_component(CUDD_INCLUDE "${CUDD_HEADER}" PATH)
    message(STATUS "CUDD header: ${CUDD_HEADER}")
  else()
    message(STATUS "CUDD header: not found")
  endif()
else()
  message(STATUS "CUDD library: not found")
endif()
