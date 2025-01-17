/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000, 2011 Oracle and/or its affiliates.  All rights reserved.
 *
 * $Id$
 */
#include "db_config.h"
#include "db_int.h"
#pragma hdrstop

static int __cdsgroup_abort(DB_TXN * txn);
static int __cdsgroup_commit(DB_TXN * txn, uint32 flags);
static int __cdsgroup_discard(DB_TXN * txn, uint32 flags);
static uint32 __cdsgroup_id(DB_TXN * txn);
static int FASTCALL __cdsgroup_notsup(ENV * env, const char * meth);
static int __cdsgroup_prepare(DB_TXN * txn, uint8*gid);
static int __cdsgroup_get_name(DB_TXN * txn, const char ** namep);
static int __cdsgroup_set_name(DB_TXN * txn, const char * name);
static int __cdsgroup_set_timeout(DB_TXN * txn, db_timeout_t timeout, uint32 flags);
/*
 * __cdsgroup_notsup --
 *	Error when CDS groups don't support a method.
 */
static int FASTCALL __cdsgroup_notsup(ENV * env, const char * meth)
{
	__db_errx(env, DB_STR_A("0687", "CDS groups do not support %s", "%s"), meth);
	return DB_OPNOTSUP;
}

static int __cdsgroup_abort(DB_TXN * txn)
{
	return __cdsgroup_notsup(txn->mgrp->env, "abort");
}

static int __cdsgroup_commit(DB_TXN * txn, uint32 flags)
{
	DB_LOCKER * locker;
	DB_LOCKREQ lreq;
	ENV * env;
	int ret, t_ret;
	COMPQUIET(flags, 0);
	env = txn->mgrp->env;
	/* Check for live cursors. */
	if(txn->cursors != 0) {
		__db_errx(env, DB_STR("0688", "CDS group has active cursors"));
		return EINVAL;
	}
	/* We may be holding handle locks; release them. */
	lreq.op = DB_LOCK_PUT_ALL;
	lreq.obj = NULL;
	ret = __lock_vec(env, txn->locker, 0, &lreq, 1, 0);
	env = txn->mgrp->env;
	locker = txn->locker;
	__os_free(env, txn->mgrp);
	__os_free(env, txn);
	if((t_ret = __lock_id_free(env, locker)) != 0 && ret == 0)
		ret = t_ret;
	return ret;
}

static int __cdsgroup_discard(DB_TXN * txn, uint32 flags)
{
	COMPQUIET(flags, 0);
	return __cdsgroup_notsup(txn->mgrp->env, "discard");
}

static uint32 __cdsgroup_id(DB_TXN * txn)
{
	return txn->txnid;
}

static int __cdsgroup_prepare(DB_TXN * txn, uint8 * gid)
{
	COMPQUIET(gid, 0);
	return __cdsgroup_notsup(txn->mgrp->env, "prepare");
}

static int __cdsgroup_get_name(DB_TXN * txn, const char ** namep)
{
	COMPQUIET(namep, 0);
	return __cdsgroup_notsup(txn->mgrp->env, "get_name");
}

static int __cdsgroup_set_name(DB_TXN * txn, const char * name)
{
	COMPQUIET(name, 0);
	return __cdsgroup_notsup(txn->mgrp->env, "set_name");
}

static int __cdsgroup_set_timeout(DB_TXN * txn, db_timeout_t timeout, uint32 flags)
{
	COMPQUIET(timeout, 0);
	COMPQUIET(flags, 0);
	return __cdsgroup_notsup(txn->mgrp->env, "set_timeout");
}
/*
 * PUBLIC: int __cdsgroup_begin __P((ENV *, DB_TXN **));
 */
int __cdsgroup_begin(ENV * env, DB_TXN ** txnpp)
{
	DB_TXN * txn;
	int ret;
	*txnpp = txn = NULL;
	if((ret = __os_calloc(env, 1, sizeof(DB_TXN), &txn)) != 0)
		goto err;
	/*
	 * We need a dummy DB_TXNMGR -- it's the only way to get from a
	 * transaction handle to the environment handle.
	 */
	if((ret = __os_calloc(env, 1, sizeof(DB_TXNMGR), &txn->mgrp)) != 0)
		goto err;
	txn->mgrp->env = env;
	if((ret = __lock_id(env, &txn->txnid, &txn->locker)) != 0)
		goto err;
	txn->flags = TXN_FAMILY;
	txn->abort = __cdsgroup_abort;
	txn->commit = __cdsgroup_commit;
	txn->discard = __cdsgroup_discard;
	txn->id = __cdsgroup_id;
	txn->prepare = __cdsgroup_prepare;
	txn->get_name = __cdsgroup_get_name;
	txn->set_name = __cdsgroup_set_name;
	txn->set_timeout = __cdsgroup_set_timeout;
	*txnpp = txn;
	if(0) {
err:
		if(txn) {
			__os_free(env, txn->mgrp);
			__os_free(env, txn);
		}
	}
	return ret;
}
/*
 * __cds_txn_begin_pp --
 *	DB_ENV->cdsgroup_begin
 *
 * PUBLIC: int __cdsgroup_begin_pp __P((DB_ENV *, DB_TXN **));
 */
int __cdsgroup_begin_pp(DB_ENV * dbenv, DB_TXN ** txnpp)
{
	DB_THREAD_INFO * ip;
	int ret;
	ENV * env = dbenv->env;
	ENV_ILLEGAL_BEFORE_OPEN(env, "cdsgroup_begin");
	if(!CDB_LOCKING(env))
		return __env_not_config(env, "cdsgroup_begin", DB_INIT_CDB);
	ENV_ENTER(env, ip);
	ret = __cdsgroup_begin(env, txnpp);
	ENV_LEAVE(env, ip);
	return ret;
}
