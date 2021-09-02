/*
 * File: condvar2_1.c
 *
 *
 * --------------------------------------------------------------------------
 *
 *      Pthreads4w - POSIX Threads for Windows
 *      Copyright 1998 John E. Bossom
 *      Copyright 1999-2018, Pthreads4w contributors
 *
 *      Homepage: https://sourceforge.net/projects/pthreads4w/
 *
 *      The current list of contributors is contained
 *      in the file CONTRIBUTORS included with the source
 *      code distribution. The list can also be seen at the
 *      following World Wide Web location:
 *
 *      https://sourceforge.net/p/pthreads4w/wiki/Contributors/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * --------------------------------------------------------------------------
 *
 * Test Synopsis:
 * - Test timeout of multiple waits on a CV with no signal/broadcast.
 *
 * Test Method (Validation or Falsification):
 * - Validation
 *
 * Requirements Tested:
 * -
 *
 * Features Tested:
 * -
 *
 * Cases Tested:
 * -
 *
 * Description:
 * - Because the CV is never signaled, we expect the waits to time out.
 *
 * Environment:
 * -
 *
 * Input:
 * - None.
 *
 * Output:
 * - File name, Line number, and failed expression on failure.
 * - No output on success.
 *
 * Assumptions:
 * -
 *
 * Pass Criteria:
 * - pthread_cond_timedwait returns ETIMEDOUT.
 * - Process returns zero exit status.
 *
 * Fail Criteria:
 * - pthread_cond_timedwait does not return ETIMEDOUT.
 * - Process returns non-zero exit status.
 */
// @sobolev #define _WIN32_WINNT 0x400
#include <sl_pthreads4w.h>
#pragma hdrstop
#include "test.h"

static pthread_cond_t cv;
static pthread_mutex_t mutex;
static struct timespec abstime = { 0, 0 }, reltime = { 5, 0 };

enum {
	NUMTHREADS = 30
};

static void * mythread(void * arg)
{
	assert(pthread_mutex_lock(&mutex) == 0);
	assert(pthread_cond_timedwait(&cv, &mutex, &abstime) == ETIMEDOUT);
	assert(pthread_mutex_unlock(&mutex) == 0);
	return arg;
}

/* Cheating here - sneaking a peek at library internals */
//#include "../config.h"
//#include <ptw32_config.h>
//#include <implement.h>

int PThr4wTest_CondVar21()
{
	int i;
	pthread_t t[NUMTHREADS + 1];
	void* result = (void*)0;
	assert(pthread_cond_init(&cv, NULL) == 0);
	assert(pthread_mutex_init(&mutex, NULL) == 0);
	pthread_win32_getabstime_np(&abstime, &reltime);
	assert(pthread_mutex_lock(&mutex) == 0);
	for(i = 1; i <= NUMTHREADS; i++) {
		assert(pthread_create(&t[i], NULL, mythread, (void*)(size_t)i) == 0);
	}
	assert(pthread_mutex_unlock(&mutex) == 0);
	for(i = 1; i <= NUMTHREADS; i++) {
		assert(pthread_join(t[i], &result) == 0);
		assert((int)(size_t)result == i);
	}
	{
		int result = pthread_cond_destroy(&cv);
		if(result != 0) {
			fprintf(stderr, "Result = %s\n", PThr4wErrorString[result]);
			fprintf(stderr, "\tWaitersBlocked = %ld\n", cv->nWaitersBlocked);
			fprintf(stderr, "\tWaitersGone = %ld\n", cv->nWaitersGone);
			fprintf(stderr, "\tWaitersToUnblock = %ld\n", cv->nWaitersToUnblock);
			fflush(stderr);
		}
		assert(result == 0);
	}

	return 0;
}