// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "StaConfig.hh"

#if !PTHREADS

#include "Pthread.hh"

namespace sta {

int
pthread_create(pthread_t *,
	       const pthread_attr_t *,
	       void *(*) (void *),
	       void *)
{
  return 0;
}

int
pthread_join(pthread_t,
	     void **)
{
  return 0;
}

int
pthread_attr_init(pthread_attr_t *)
{
  return 0;
}

int
pthread_attr_setscope(pthread_attr_t *,
		      int)
{
  return 0;
}

int
pthread_cond_init(pthread_cond_t *,
		  const pthread_condattr_t *)
{
  return 0;
}

int
pthread_cond_destroy(pthread_cond_t *)
{
  return 0;
}

int
pthread_cond_signal(pthread_cond_t *)
{
  return 0;
}

int
pthread_cond_broadcast(pthread_cond_t *)
{
  return 0;
}

int
pthread_cond_wait(pthread_cond_t *,
		  pthread_mutex_t *)
{
  return 0;
}

int
pthread_mutex_init(pthread_mutex_t *,
		   const pthread_mutexattr_t *)
{
  return 0;
}

int
pthread_mutex_destroy(pthread_mutex_t *)
{
  return 0;
}

int
pthread_mutex_lock(pthread_mutex_t *)
{
  return 0;
}

int
pthread_mutex_trylock(pthread_mutex_t *)
{
  return 0;
}

int
pthread_mutex_unlock(pthread_mutex_t *)
{
  return 0;
}

int
pthread_rwlock_init(pthread_rwlock_t *,
		    const pthread_rwlockattr_t *)
{
  return 0;
}

int
pthread_rwlock_destroy(pthread_rwlock_t *)
{
  return 0;
}

int
pthread_rwlock_rdlock(pthread_rwlock_t *)
{
  return 0;
}

int
pthread_rwlock_wrlock(pthread_rwlock_t *)
{
  return 0;
}

int
pthread_rwlock_unlock(pthread_rwlock_t *)
{
  return 0;
}

} // namespace
#endif
