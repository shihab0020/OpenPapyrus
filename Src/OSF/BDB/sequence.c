/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2004, 2011 Oracle and/or its affiliates.  All rights reserved.
 *
 * $Id$
 */
#include "db_config.h"
#include "db_int.h"
#pragma hdrstop
#include "dbinc_auto/sequence_ext.h"

#ifdef HAVE_64BIT_TYPES
/*
 * Sequences must be architecture independent but they are stored as user
 * data in databases so the code here must handle the byte ordering.  We
 * store them in little-endian byte ordering.  If we are on a big-endian
 * machine we swap in and out when we read from the database. seq->seq_rp
 * always points to the record in native ordering.
 *
 * Version 1 always stored things in native format so if we detect this we
 * upgrade on the fly and write the record back at open time.
 */
 #define SEQ_SWAP(rp)                                                    \
        do {                                                            \
		M_32_SWAP((rp)->seq_version);                           \
		M_32_SWAP((rp)->flags);                                 \
		M_64_SWAP((rp)->seq_value);                             \
		M_64_SWAP((rp)->seq_max);                               \
		M_64_SWAP((rp)->seq_min);                               \
	} while(0)

 #define SEQ_SWAP_IN(env, seq) \
        do {                                                            \
		if(!F_ISSET((env), ENV_LITTLEENDIAN)) {                \
			memcpy(&seq->seq_record, seq->seq_data.data, sizeof(seq->seq_record)); \
			SEQ_SWAP(&seq->seq_record);                     \
		}                                                       \
	} while(0)

 #define SEQ_SWAP_OUT(env, seq) \
        do {                                                            \
		if(!F_ISSET((env), ENV_LITTLEENDIAN)) {                \
			memcpy(seq->seq_data.data, &seq->seq_record, sizeof(seq->seq_record)); \
			SEQ_SWAP((DB_SEQ_RECORD *)seq->seq_data.data);   \
		}                                                       \
	} while(0)

static int __seq_chk_cachesize __P((ENV*, int32, db_seq_t, db_seq_t));
static int __seq_close __P((DB_SEQUENCE*, uint32));
static int __seq_close_pp __P((DB_SEQUENCE*, uint32));
static int __seq_get __P((DB_SEQUENCE*, DB_TXN*, int32,  db_seq_t*, uint32));
static int __seq_get_cachesize __P((DB_SEQUENCE*, int32 *));
static int __seq_get_db __P((DB_SEQUENCE*, DB**));
static int __seq_get_flags __P((DB_SEQUENCE*, uint32 *));
static int __seq_get_key __P((DB_SEQUENCE*, DBT *));
static int __seq_get_range __P((DB_SEQUENCE*, db_seq_t*, db_seq_t *));
static int __seq_initial_value __P((DB_SEQUENCE*, db_seq_t));
static int __seq_open_pp __P((DB_SEQUENCE*, DB_TXN*, DBT*, uint32));
static int __seq_remove __P((DB_SEQUENCE*, DB_TXN*, uint32));
static int __seq_set_cachesize __P((DB_SEQUENCE*, int32));
static int __seq_set_flags __P((DB_SEQUENCE*, uint32));
static int __seq_set_range __P((DB_SEQUENCE*, db_seq_t, db_seq_t));
static int __seq_update __P((DB_SEQUENCE*, DB_THREAD_INFO*, DB_TXN*, int32, uint32));

/*
 * db_sequence_create --
 *	DB_SEQUENCE constructor.
 *
 * EXTERN: int db_sequence_create __P((DB_SEQUENCE **, DB *, uint32));
 */
int db_sequence_create(DB_SEQUENCE ** seqp, DB * dbp, uint32 flags)
{
	DB_SEQUENCE * seq;
	int ret;
	ENV * env = dbp->env;
	DB_ILLEGAL_BEFORE_OPEN(dbp, "db_sequence_create");
	// Check for invalid function flags
	switch(flags) {
	    case 0: break;
	    default: return __db_ferr(env, "db_sequence_create", 0);
	}
	if(dbp->type == DB_HEAP) {
		__db_errx(env, DB_STR("4016", "Heap databases may not be used with sequences."));
		return EINVAL;
	}
	// Allocate the sequence
	if((ret = __os_calloc(env, 1, sizeof(*seq), &seq)) != 0)
		return ret;
	seq->seq_dbp = dbp;
	seq->close = __seq_close_pp;
	seq->get = __seq_get;
	seq->get_cachesize = __seq_get_cachesize;
	seq->set_cachesize = __seq_set_cachesize;
	seq->get_db = __seq_get_db;
	seq->get_flags = __seq_get_flags;
	seq->get_key = __seq_get_key;
	seq->get_range = __seq_get_range;
	seq->initial_value = __seq_initial_value;
	seq->open = __seq_open_pp;
	seq->remove = __seq_remove;
	seq->set_flags = __seq_set_flags;
	seq->set_range = __seq_set_range;
	seq->stat = __seq_stat;
	seq->stat_print = __seq_stat_print;
	seq->seq_rp = &seq->seq_record;
	*seqp = seq;
	return 0;
}
/*
 * __seq_open --
 *	DB_SEQUENCE->open method.
 *
 */
static int __seq_open_pp(DB_SEQUENCE * seq, DB_TXN * txn, DBT * keyp, uint32 flags)
{
	DB * dbp;
	DB_SEQ_RECORD * rp;
	DB_THREAD_INFO * ip;
	ENV * env;
	uint32 tflags;
	int handle_check, txn_local, ret, t_ret;
 #define SEQ_OPEN_FLAGS  (DB_CREATE|DB_EXCL|DB_THREAD)
	dbp = seq->seq_dbp;
	env = dbp->env;
	txn_local = 0;
	STRIP_AUTO_COMMIT(flags);
	SEQ_ILLEGAL_AFTER_OPEN(seq, "DB_SEQUENCE->open");
	ENV_ENTER(env, ip);
	/* Check for replication block. */
	handle_check = IS_ENV_REPLICATED(env);
	if(handle_check && (ret = __db_rep_enter(dbp, 1, 0, IS_REAL_TXN(txn))) != 0) {
		handle_check = 0;
		goto err;
	}
	if((ret = __db_fchk(env, "DB_SEQUENCE->open", flags, SEQ_OPEN_FLAGS)) != 0)
		goto err;
	if(keyp->size == 0) {
		__db_errx(env, DB_STR("4001", "Zero length sequence key specified"));
		ret = EINVAL;
		goto err;
	}
	if((ret = __db_get_flags(dbp, &tflags)) != 0)
		goto err;
	/*
	 * We can let replication clients open sequences, but must
	 * check later that they do not update them.
	 */
	if(F_ISSET(dbp, DB_AM_RDONLY)) {
		ret = __db_rdonly(dbp->env, "DB_SEQUENCE->open");
		goto err;
	}
	if(FLD_ISSET(tflags, DB_DUP)) {
		__db_errx(env, DB_STR("4002", "Sequences not supported in databases configured for duplicate data"));
		ret = EINVAL;
		goto err;
	}
	if(LF_ISSET(DB_THREAD)) {
		if((ret = __mutex_alloc(env, MTX_SEQUENCE, DB_MUTEX_PROCESS_ONLY, &seq->mtx_seq)) != 0)
			goto err;
	}
	memzero(&seq->seq_data, sizeof(DBT));
	if(F_ISSET(env, ENV_LITTLEENDIAN)) {
		seq->seq_data.data = &seq->seq_record;
		seq->seq_data.flags = DB_DBT_USERMEM;
	}
	else {
		if((ret = __os_umalloc(env, sizeof(seq->seq_record), &seq->seq_data.data)) != 0)
			goto err;
		seq->seq_data.flags = DB_DBT_REALLOC;
	}
	seq->seq_data.ulen = seq->seq_data.size = sizeof(seq->seq_record);
	seq->seq_rp = &seq->seq_record;
	if((ret = __dbt_usercopy(env, keyp)) != 0)
		goto err;
	memzero(&seq->seq_key, sizeof(DBT));
	if((ret = __os_malloc(env, keyp->size, &seq->seq_key.data)) != 0)
		goto err;
	memcpy(seq->seq_key.data, keyp->data, keyp->size);
	seq->seq_key.size = seq->seq_key.ulen = keyp->size;
	seq->seq_key.flags = DB_DBT_USERMEM;
retry:
	if((ret = __db_get(dbp, ip, txn, &seq->seq_key, &seq->seq_data, 0)) != 0) {
		if(ret == DB_BUFFER_SMALL && seq->seq_data.size > sizeof(seq->seq_record)) {
			seq->seq_data.flags = DB_DBT_REALLOC;
			seq->seq_data.data = NULL;
			goto retry;
		}
		if((ret != DB_NOTFOUND && ret != DB_KEYEMPTY) || !LF_ISSET(DB_CREATE))
			goto err;
		if(IS_REP_CLIENT(env) && !F_ISSET(dbp, DB_AM_NOT_DURABLE)) {
			ret = __db_rdonly(env, "DB_SEQUENCE->open");
			goto err;
		}
		ret = 0;
		rp = &seq->seq_record;
		if(!F_ISSET(rp, DB_SEQ_RANGE_SET)) {
			rp->seq_max = INT64_MAX;
			rp->seq_min = INT64_MIN;
		}
		/* INC is the default. */
		if(!F_ISSET(rp, DB_SEQ_DEC))
			F_SET(rp, DB_SEQ_INC);
		rp->seq_version = DB_SEQUENCE_VERSION;
		if(rp->seq_value > rp->seq_max || rp->seq_value < rp->seq_min) {
			__db_errx(env, DB_STR("4003", "Sequence value out of range"));
			ret = EINVAL;
			goto err;
		}
		else {
			SEQ_SWAP_OUT(env, seq);
			/* Create local transaction as necessary. */
			if(IS_DB_AUTO_COMMIT(dbp, txn)) {
				if((ret = __txn_begin(env, ip, NULL, &txn, 0)) != 0)
					goto err;
				txn_local = 1;
			}
			if((ret = __db_put(dbp, ip, txn, &seq->seq_key, &seq->seq_data, DB_NOOVERWRITE)) != 0) {
				__db_errx(env, DB_STR("4004", "Sequence create failed"));
				goto err;
			}
		}
	}
	else if(LF_ISSET(DB_CREATE) && LF_ISSET(DB_EXCL)) {
		ret = EEXIST;
		goto err;
	}
	else if(seq->seq_data.size < sizeof(seq->seq_record)) {
		__db_errx(env, DB_STR("4005", "Bad sequence record format"));
		ret = EINVAL;
		goto err;
	}
	if(F_ISSET(env, ENV_LITTLEENDIAN))
		seq->seq_rp = (DB_SEQ_RECORD *)seq->seq_data.data;
	/*
	 * The first release was stored in native mode.
	 * Check the version number before swapping.
	 */
	rp = (DB_SEQ_RECORD *)seq->seq_data.data;
	if(rp->seq_version == DB_SEQUENCE_OLDVER) {
oldver:
		if(IS_REP_CLIENT(env) && !F_ISSET(dbp, DB_AM_NOT_DURABLE)) {
			ret = __db_rdonly(env, "DB_SEQUENCE->open");
			goto err;
		}
		rp->seq_version = DB_SEQUENCE_VERSION;
		if(!F_ISSET(env, ENV_LITTLEENDIAN)) {
			if(IS_DB_AUTO_COMMIT(dbp, txn)) {
				if((ret = __txn_begin(env, ip, NULL, &txn, 0)) != 0)
					goto err;
				txn_local = 1;
				goto retry;
			}
			memcpy(&seq->seq_record, rp, sizeof(seq->seq_record));
			SEQ_SWAP_OUT(env, seq);
		}
		if((ret = __db_put(dbp, ip, txn, &seq->seq_key, &seq->seq_data, 0)) != 0)
			goto err;
	}
	rp = seq->seq_rp;
	SEQ_SWAP_IN(env, seq);
	if(rp->seq_version != DB_SEQUENCE_VERSION) {
		/*
		 * The database may have moved from one type
		 * of machine to another, check here.
		 * If we moved from little-end to big-end then
		 * the swap above will make the version correct.
		 * If the move was from big to little
		 * then we need to swap to see if this
		 * is an old version.
		 */
		if(rp->seq_version == DB_SEQUENCE_OLDVER)
			goto oldver;
		M_32_SWAP(rp->seq_version);
		if(rp->seq_version == DB_SEQUENCE_OLDVER) {
			SEQ_SWAP(rp);
			goto oldver;
		}
		M_32_SWAP(rp->seq_version);
		__db_errx(env, DB_STR_A("4006", "Unsupported sequence version: %d", "%d"), rp->seq_version);
		goto err;
	}
	seq->seq_last_value = seq->seq_prev_value = rp->seq_value;
	if(F_ISSET(rp, DB_SEQ_INC))
		seq->seq_last_value--;
	else
		seq->seq_last_value++;
	/*
	 * It's an error to specify a cache larger than the range of sequences.
	 */
	if(seq->seq_cache_size != 0 && (ret = __seq_chk_cachesize(env, seq->seq_cache_size, rp->seq_max, rp->seq_min)) != 0)
		goto err;
err:
	if(txn_local && (t_ret = __db_txn_auto_resolve(env, txn, 0, ret)) && ret == 0)
		ret = t_ret;
	if(ret != 0) {
		__os_free(env, seq->seq_key.data);
		seq->seq_key.data = NULL;
	}
	/* Release replication block. */
	if(handle_check && (t_ret = __env_db_rep_exit(env)) != 0 && ret == 0)
		ret = t_ret;
	ENV_LEAVE(env, ip);
	__dbt_userfree(env, keyp, 0, 0);
	return ret;
}
/*
 * __seq_get_cachesize --
 *	Accessor for value passed into DB_SEQUENCE->set_cachesize call.
 *
 */
static int __seq_get_cachesize(DB_SEQUENCE * seq, int32 * cachesize)
{
	SEQ_ILLEGAL_BEFORE_OPEN(seq, "DB_SEQUENCE->get_cachesize");
	*cachesize = seq->seq_cache_size;
	return 0;
}
/*
 * __seq_set_cachesize --
 *	DB_SEQUENCE->set_cachesize.
 *
 */
static int __seq_set_cachesize(DB_SEQUENCE * seq, int32 cachesize)
{
	int ret;
	ENV * env = seq->seq_dbp->env;
	if(cachesize < 0) {
		__db_errx(env, DB_STR("4007", "Cache size must be >= 0"));
		return EINVAL;
	}
	/*
	 * It's an error to specify a cache larger than the range of sequences.
	 */
	if(SEQ_IS_OPEN(seq) && (ret = __seq_chk_cachesize(env, cachesize, seq->seq_rp->seq_max, seq->seq_rp->seq_min)) != 0)
		return ret;
	seq->seq_cache_size = cachesize;
	return 0;
}

#define SEQ_SET_FLAGS   (DB_SEQ_WRAP|DB_SEQ_INC|DB_SEQ_DEC)
//
// Accessor for flags passed into DB_SEQUENCE->open call
//
static int __seq_get_flags(DB_SEQUENCE * seq, uint32 * flagsp)
{
	SEQ_ILLEGAL_BEFORE_OPEN(seq, "DB_SEQUENCE->get_flags");
	*flagsp = F_ISSET(seq->seq_rp, SEQ_SET_FLAGS);
	return 0;
}
/*
 * __seq_set_flags --
 *	DB_SEQUENCE->set_flags.
 *
 */
static int __seq_set_flags(DB_SEQUENCE * seq, uint32 flags)
{
	int ret;
	ENV * env = seq->seq_dbp->env;
	DB_SEQ_RECORD * rp = seq->seq_rp;
	SEQ_ILLEGAL_AFTER_OPEN(seq, "DB_SEQUENCE->set_flags");
	if((ret = __db_fchk(env, "DB_SEQUENCE->set_flags", flags, SEQ_SET_FLAGS)) != 0)
		return ret;
	if((ret = __db_fcchk(env, "DB_SEQUENCE->set_flags", flags, DB_SEQ_DEC, DB_SEQ_INC)) != 0)
		return ret;
	if(LF_ISSET(DB_SEQ_DEC|DB_SEQ_INC))
		F_CLR(rp, DB_SEQ_DEC|DB_SEQ_INC);
	F_SET(rp, flags);
	return 0;
}
/*
 * __seq_initial_value --
 *	DB_SEQUENCE->initial_value.
 *
 */
static int __seq_initial_value(DB_SEQUENCE * seq, db_seq_t value)
{
	DB_SEQ_RECORD * rp;
	ENV * env = seq->seq_dbp->env;
	SEQ_ILLEGAL_AFTER_OPEN(seq, "DB_SEQUENCE->initial_value");
	rp = seq->seq_rp;
	if(F_ISSET(rp, DB_SEQ_RANGE_SET) && (value > rp->seq_max || value < rp->seq_min)) {
		__db_errx(env, DB_STR("4008", "Sequence value out of range"));
		return EINVAL;
	}
	rp->seq_value = value;
	return 0;
}
/*
 * __seq_get_range --
 *	Accessor for range passed into DB_SEQUENCE->set_range call
 *
 */
static int __seq_get_range(DB_SEQUENCE * seq, db_seq_t * minp, db_seq_t * maxp)
{
	SEQ_ILLEGAL_BEFORE_OPEN(seq, "DB_SEQUENCE->get_range");
	*minp = seq->seq_rp->seq_min;
	*maxp = seq->seq_rp->seq_max;
	return 0;
}
/*
 * __seq_set_range --
 *	SEQUENCE->set_range.
 *
 */
static int __seq_set_range(DB_SEQUENCE * seq, db_seq_t min, db_seq_t max)
{
	DB_SEQ_RECORD * rp;
	ENV * env = seq->seq_dbp->env;
	SEQ_ILLEGAL_AFTER_OPEN(seq, "DB_SEQUENCE->set_range");
	rp = seq->seq_rp;
	if(min >= max) {
		__db_errx(env, DB_STR("4009", "Minimum sequence value must be less than maximum sequence value"));
		return EINVAL;
	}
	rp->seq_min = min;
	rp->seq_max = max;
	F_SET(rp, DB_SEQ_RANGE_SET);
	return 0;
}

static int __seq_update(DB_SEQUENCE * seq, DB_THREAD_INFO * ip, DB_TXN * txn, int32 delta, uint32 flags)
{
	DBT    ldata;
	DB_SEQ_RECORD * rp;
	int32 adjust;
	int ret, txn_local;
	DB * dbp = seq->seq_dbp;
	ENV * env = dbp->env;
	int need_mutex = 0;
	DBT * data = &seq->seq_data;
	/*
	 * Create a local transaction as necessary, check for consistent
	 * transaction usage, and, if we have no transaction but do have
	 * locking on, acquire a locker id for the handle lock acquisition.
	 */
	if(IS_DB_AUTO_COMMIT(dbp, txn)) {
		if((ret = __txn_begin(env, ip, NULL, &txn, flags)) != 0)
			return ret;
		txn_local = 1;
	}
	else
		txn_local = 0;
	// Check for consistent transaction usage
	if((ret = __db_check_txn(dbp, txn, DB_LOCK_INVALIDID, 0)) != 0)
		goto err;
	//
	// If we are in a global transaction avoid deadlocking on the mutex.
	// The write lock on the data will prevent two updaters getting in
	// at once.  Fetch the data then see if things are what we thought they were.
	//
	if(txn_local == 0 && txn != NULL) {
		MUTEX_UNLOCK(env, seq->mtx_seq);
		need_mutex = 1;
		data = &ldata;
		data->data = NULL;
		data->flags = DB_DBT_REALLOC;
	}
retry:
	if((ret = __db_get(dbp, ip, txn, &seq->seq_key, data, DB_RMW)) != 0) {
		if(ret == DB_BUFFER_SMALL && seq->seq_data.size > sizeof(seq->seq_record)) {
			data->flags = DB_DBT_REALLOC;
			data->data = NULL;
			goto retry;
		}
		goto err;
	}
	if(data->size < sizeof(seq->seq_record)) {
		__db_errx(env, DB_STR("4010", "Bad sequence record format"));
		ret = EINVAL;
		goto err;
	}
	/* We have an exclusive lock on the data, see if we raced. */
	if(need_mutex) {
		MUTEX_LOCK(env, seq->mtx_seq);
		need_mutex = 0;
		rp = seq->seq_rp;
		/*
		 * Note that caching must be off if we have global
		 * transaction so the value we fetch from the database
		 * is the correct current value.
		 */
		if(data->size <= seq->seq_data.size) {
			memcpy(seq->seq_data.data, data->data, data->size);
			__os_ufree(env, data->data);
		}
		else {
			seq->seq_data.data = data->data;
			seq->seq_data.size = data->size;
		}
	}
	if(F_ISSET(env, ENV_LITTLEENDIAN))
		seq->seq_rp = (DB_SEQ_RECORD *)seq->seq_data.data;
	SEQ_SWAP_IN(env, seq);
	rp = seq->seq_rp;
	if(F_ISSET(rp, DB_SEQ_WRAPPED))
		goto overflow;
	adjust = delta > seq->seq_cache_size ? delta : seq->seq_cache_size;
	/*
	 * Check whether this operation will cause the sequence to wrap.
	 *
	 * The sequence minimum and maximum values can be INT64_MIN and
	 * INT64_MAX, so we need to do the test carefully to cope with
	 * arithmetic overflow.  The first part of the test below checks
	 * whether we will hit the end of the 64-bit range.  The second part
	 * checks whether we hit the end of the sequence.
	 */
again:
	if(F_ISSET(rp, DB_SEQ_INC)) {
		if(rp->seq_value+adjust-1 < rp->seq_value || rp->seq_value+adjust-1 > rp->seq_max) {
			/* Don't wrap just to fill the cache. */
			if(adjust > delta) {
				adjust = delta;
				goto again;
			}
			if(F_ISSET(rp, DB_SEQ_WRAP))
				rp->seq_value = rp->seq_min;
			else {
overflow:
				__db_errx(env, DB_STR("4011", "Sequence overflow"));
				ret = EINVAL;
				goto err;
			}
		}
		/* See if we are at the end of the 64 bit range. */
		if(!F_ISSET(rp, DB_SEQ_WRAP) && rp->seq_value+adjust < rp->seq_value)
			F_SET(rp, DB_SEQ_WRAPPED);
	}
	else {
		if((rp->seq_value-adjust)+1 > rp->seq_value || (rp->seq_value-adjust)+1 < rp->seq_min) {
			/* Don't wrap just to fill the cache. */
			if(adjust > delta) {
				adjust = delta;
				goto again;
			}
			if(F_ISSET(rp, DB_SEQ_WRAP))
				rp->seq_value = rp->seq_max;
			else
				goto overflow;
		}
		/* See if we are at the end of the 64 bit range. */
		if(!F_ISSET(rp, DB_SEQ_WRAP) && rp->seq_value-adjust > rp->seq_value)
			F_SET(rp, DB_SEQ_WRAPPED);
		adjust = -adjust;
	}
	rp->seq_value += adjust;
	SEQ_SWAP_OUT(env, seq);
	ret = __db_put(dbp, ip, txn, &seq->seq_key, &seq->seq_data, 0);
	rp->seq_value -= adjust;
	if(ret != 0) {
		__db_errx(env, DB_STR("4012", "Sequence update failed"));
		goto err;
	}
	seq->seq_last_value = rp->seq_value+adjust;
	if(F_ISSET(rp, DB_SEQ_INC))
		seq->seq_last_value--;
	else
		seq->seq_last_value++;
err:
	if(need_mutex) {
		__os_ufree(env, data->data);
		MUTEX_LOCK(env, seq->mtx_seq);
	}
	return txn_local ? __db_txn_auto_resolve(env, txn, LF_ISSET(DB_TXN_NOSYNC), ret) : ret;
}

static int __seq_get(DB_SEQUENCE * seq, DB_TXN * txn, int32 delta, db_seq_t * retp, uint32 flags)
{
	DB_THREAD_INFO * ip;
	int handle_check, t_ret;
	DB * dbp = seq->seq_dbp;
	ENV * env = dbp->env;
	DB_SEQ_RECORD * rp = seq->seq_rp;
	int ret = 0;
	STRIP_AUTO_COMMIT(flags);
	SEQ_ILLEGAL_BEFORE_OPEN(seq, "DB_SEQUENCE->get");
	if(delta < 0 || (delta == 0 && !LF_ISSET(DB_CURRENT))) {
		__db_errx(env, "Sequence delta must be greater than 0");
		return EINVAL;
	}
	if(seq->seq_cache_size != 0 && txn != NULL) {
		__db_errx(env, "Sequence with non-zero cache may not specify transaction handle");
		return EINVAL;
	}
	ENV_ENTER(env, ip);

	/* Check for replication block. */
	handle_check = IS_ENV_REPLICATED(env);
	if(handle_check && (ret = __db_rep_enter(dbp, 1, 0, IS_REAL_TXN(txn))) != 0)
		return ret;
	MUTEX_LOCK(env, seq->mtx_seq);
	if(handle_check && IS_REP_CLIENT(env) && !F_ISSET(dbp, DB_AM_NOT_DURABLE)) {
		ret = __db_rdonly(env, "DB_SEQUENCE->get");
		goto err;
	}
	if(rp->seq_min+delta > rp->seq_max) {
		__db_errx(env, DB_STR("4013", "Sequence overflow"));
		ret = EINVAL;
		goto err;
	}
	if(LF_ISSET(DB_CURRENT)) {
		*retp = seq->seq_prev_value;
	}
	else if(F_ISSET(rp, DB_SEQ_INC)) {
		if(seq->seq_last_value+1-rp->seq_value < delta && (ret = __seq_update(seq, ip, txn, delta, flags)) != 0)
			goto err;
		rp = seq->seq_rp;
		*retp = rp->seq_value;
		seq->seq_prev_value = rp->seq_value;
		rp->seq_value += delta;
	}
	else {
		if((rp->seq_value-seq->seq_last_value)+1 < delta && (ret = __seq_update(seq, ip, txn, delta, flags)) != 0)
			goto err;
		rp = seq->seq_rp;
		*retp = rp->seq_value;
		seq->seq_prev_value = rp->seq_value;
		rp->seq_value -= delta;
	}
err:
	MUTEX_UNLOCK(env, seq->mtx_seq);
	/* Release replication block. */
	if(handle_check && (t_ret = __env_db_rep_exit(env)) != 0 && ret == 0)
		ret = t_ret;
	ENV_LEAVE(env, ip);
	return ret;
}
/*
 * __seq_get_db --
 *	Accessor for dbp passed into db_sequence_create call
 */
static int __seq_get_db(DB_SEQUENCE * seq, DB ** dbpp)
{
	*dbpp = seq->seq_dbp;
	return 0;
}
/*
 * __seq_get_key --
 *	Accessor for key passed into DB_SEQUENCE->open call
 */
static int __seq_get_key(DB_SEQUENCE * seq, DBT * key)
{
	SEQ_ILLEGAL_BEFORE_OPEN(seq, "DB_SEQUENCE->get_key");
	if(F_ISSET(key, DB_DBT_USERCOPY))
		return __db_retcopy(seq->seq_dbp->env, key, seq->seq_key.data, seq->seq_key.size, NULL, 0);
	key->data = seq->seq_key.data;
	key->size = key->ulen = seq->seq_key.size;
	key->flags = seq->seq_key.flags;
	return 0;
}
/*
 * __seq_close_pp --
 *	Close a sequence pre/post processing
 */
static int __seq_close_pp(DB_SEQUENCE * seq, uint32 flags)
{
	DB_THREAD_INFO * ip;
	int ret;
	ENV_ENTER(seq->seq_dbp->env, ip);
	ret = __seq_close(seq, flags);
	ENV_LEAVE(seq->seq_dbp->env, ip);
	return ret;
}
//
// Close a sequence
//
 static int __seq_close(DB_SEQUENCE * seq, uint32 flags)
{
	int    t_ret;
	int    ret = 0;
	ENV  * env = seq->seq_dbp->env;
	if(flags)
		ret = __db_ferr(env, "DB_SEQUENCE->close", 0);
	if((t_ret = __mutex_free(env, &seq->mtx_seq)) != 0 && ret == 0)
		ret = t_ret;
	__os_free(env, seq->seq_key.data);
	if(seq->seq_data.data != &seq->seq_record)
		__os_ufree(env, seq->seq_data.data);
	seq->seq_key.data = NULL;
	memset(seq, CLEAR_BYTE, sizeof(*seq));
	__os_free(env, seq);
	return ret;
}
//
// Remove a sequence from the database.
//
static int __seq_remove(DB_SEQUENCE * seq, DB_TXN * txn, uint32 flags)
{
	DB_THREAD_INFO * ip;
	int    handle_check, ret, t_ret;
	DB   * dbp = seq->seq_dbp;
	ENV  * env = dbp->env;
	int    txn_local = 0;
	SEQ_ILLEGAL_BEFORE_OPEN(seq, "DB_SEQUENCE->remove");
	// 
	// Flags can only be 0, unless the database has DB_AUTO_COMMIT enabled.
	// Then DB_TXN_NOSYNC is allowed.
	// 
	if(flags != 0 && (flags != DB_TXN_NOSYNC || !IS_DB_AUTO_COMMIT(dbp, txn)))
		return __db_ferr(env, "DB_SEQUENCE->remove illegal flag", 0);
	ENV_ENTER(env, ip);
	// Check for replication block.
	handle_check = IS_ENV_REPLICATED(env);
	if(handle_check && (ret = __db_rep_enter(dbp, 1, 0, IS_REAL_TXN(txn))) != 0) {
		handle_check = 0;
		goto err;
	}
	/*
	 * Create a local transaction as necessary, check for consistent
	 * transaction usage, and, if we have no transaction but do have
	 * locking on, acquire a locker id for the handle lock acquisition.
	 */
	if(IS_DB_AUTO_COMMIT(dbp, txn)) {
		if((ret = __txn_begin(env, ip, NULL, &txn, flags)) != 0)
			return ret;
		txn_local = 1;
	}
	// Check for consistent transaction usage.
	if((ret = __db_check_txn(dbp, txn, DB_LOCK_INVALIDID, 0)) != 0)
		goto err;
	ret = __db_del(dbp, ip, txn, &seq->seq_key, 0);
	if((t_ret = __seq_close(seq, 0)) != 0 && ret == 0)
		ret = t_ret;
	// Release replication block
	if(handle_check && (t_ret = __env_db_rep_exit(env)) != 0 && ret == 0)
		ret = t_ret;
err:
	if(txn_local && (t_ret = __db_txn_auto_resolve(env, txn, 0, ret)) != 0 && ret == 0)
		ret = t_ret;
	ENV_LEAVE(env, ip);
	return ret;
}
/*
 * __seq_chk_cachesize --
 *	Validate the cache size vs. the range.
 */
static int __seq_chk_cachesize(ENV * env, int32 cachesize, db_seq_t max, db_seq_t min)
{
	/*
	 * It's an error to specify caches larger than the sequence range.
	 *
	 * The min and max of the range can be either positive or negative,
	 * the difference will fit in an unsigned variable of the same type.
	 * Assume a 2's complement machine, and simply subtract.
	 */
	if((uint32)cachesize > (uint64)max-(uint64)min) {
		__db_errx(env, DB_STR("4014", "Number of items to be cached is larger than the sequence range"));
		return EINVAL;
	}
	else
		return 0;
}

#else /* !HAVE_64BIT_TYPES */

int db_sequence_create(DB_SEQUENCE ** seqp, DB * dbp, uint32 flags)
{
	COMPQUIET(seqp, 0);
	COMPQUIET(flags, 0);
	__db_errx(dbp->env, DB_STR("4015", "library build did not include support for sequences"));
	return DB_OPNOTSUP;
}
#endif /* HAVE_64BIT_TYPES */
