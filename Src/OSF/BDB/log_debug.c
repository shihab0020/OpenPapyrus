/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2011 Oracle and/or its affiliates.  All rights reserved.
 *
 * $Id$
 */
#include "db_config.h"
#include "db_int.h"
#pragma hdrstop

static int __log_printf_int __P((ENV*, DB_TXN*, const char *, va_list));
/*
 * __log_printf_capi --
 *	Write a printf-style format string into the DB log.
 */
int __log_printf_capi(DB_ENV * dbenv, DB_TXN * txnid, const char * fmt, ...)
{
	va_list ap;
	int ret;
	va_start(ap, fmt);
	ret = __log_printf_pp(dbenv, txnid, fmt, ap);
	va_end(ap);
	return ret;
}
/*
 * __log_printf_pp --
 *	Handle the arguments and call an internal routine to do the work.
 *
 *	The reason this routine isn't just folded into __log_printf_capi
 *	is because the C++ API has to call a C API routine, and you can
 *	only pass variadic arguments to a single routine.
 */
int __log_printf_pp(DB_ENV * dbenv, DB_TXN * txnid, const char * fmt, va_list ap)
{
	DB_THREAD_INFO * ip;
	int ret;
	ENV * env = dbenv->env;
	ENV_REQUIRES_CONFIG(env, env->lg_handle, "DB_ENV->log_printf", DB_INIT_LOG);
	ENV_ENTER(env, ip);
	REPLICATION_WRAP(env, (__log_printf_int(env, txnid, fmt, ap)), 0, ret);
	va_end(ap);
	ENV_LEAVE(env, ip);
	return ret;
}
/*
 * __log_printf --
 *	Write a printf-style format string into the DB log.
 */
int __log_printf(ENV * env, DB_TXN * txnid, const char * fmt, ...)
{
	va_list ap;
	int ret;
	va_start(ap, fmt);
	ret = __log_printf_int(env, txnid, fmt, ap);
	va_end(ap);
	return ret;
}
/*
 * __log_printf_int --
 *	Write a printf-style format string into the DB log (internal).
 */
static int __log_printf_int(ENV * env, DB_TXN * txnid, const char * fmt, va_list ap)
{
	DBT opdbt, msgdbt;
	DB_LSN lsn;
	char __logbuf[2048]; /* !!!: END OF THE STACK DON'T TRUST SPRINTF. */
	if(!DBENV_LOGGING(env)) {
		__db_errx(env, DB_STR("2510", "Logging not currently permitted"));
		return EAGAIN;
	}
	memzero(&opdbt, sizeof(opdbt));
	opdbt.data = "DIAGNOSTIC";
	opdbt.size = sizeof("DIAGNOSTIC")-1;
	memzero(&msgdbt, sizeof(msgdbt));
	msgdbt.data = __logbuf;
	msgdbt.size = (uint32)vsnprintf(__logbuf, sizeof(__logbuf), fmt, ap);
	return __db_debug_log(env, txnid, &lsn, 0, &opdbt, -1, &msgdbt, NULL, 0);
}
