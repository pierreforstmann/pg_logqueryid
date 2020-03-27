/*-------------------------------------------------------------------------
 *  
 * pg_logqueryid is a PostgreSQL extension which allows to print 
 * pg_stat_statements queryid when auto_explain is enabled to log. 
 * 
 * Portions of code inspired by pg_sampletolog and auto_explain extensions.
 *
 * This program is open source, licensed under the PostgreSQL license.
 * For license terms, see the LICENSE file.
 *          
 * Copyright (c) 2020, Pierre Forstmann.
 *            
 *-------------------------------------------------------------------------
*/
#include "postgres.h"
#include "executor/executor.h"
#include "storage/proc.h"
#include "access/xact.h"

#include "tcop/tcopprot.h"
#include "tcop/utility.h"
#include "utils/guc.h"
#include "utils/snapmgr.h"
#if PG_VERSION_NUM >= 90600
#include "access/parallel.h"
#endif

PG_MODULE_MAGIC;

static bool pglqi_enabled = true;
static int auto_explain_log_min_duration = -1;
static int auto_explain_log_nested_statements = 0;

/* Current nesting depth of ExecutorRun calls */
static int	pglqi_nesting_level = 0;


/* Saved hook values in case of unload */
static ExecutorStart_hook_type prev_ExecutorStart = NULL;
static ExecutorRun_hook_type prev_ExecutorRun = NULL;
static ExecutorFinish_hook_type prev_ExecutorFinish = NULL;

/*---- Function declarations ----*/

void		_PG_init(void);
void		_PG_fini(void);

static void pglqi_ExecutorStart(QueryDesc *queryDesc, int eflags);

static void pglqi_ExecutorRun(QueryDesc *queryDesc,
							 ScanDirection direction,
#if PG_VERSION_NUM >= 90600
							 uint64 count
#else
							 long count
#endif
#if PG_VERSION_NUM >= 100000
							 ,bool execute_once
#endif
);
static void pglqi_ExecutorFinish(QueryDesc *queryDesc);



/*
 * Module load callback
 */
void
_PG_init(void)
{
	const char *shared_preload_libraries_config;
	const char *auto_explain_log_min_duration_config;
	const char *auto_explain_log_nested_statements_config;
	char *auto_explain;
	char *pg_stat_statements;

	elog(DEBUG5, "pg_logqueryid:_PG_init():entry");

	shared_preload_libraries_config = GetConfigOption("shared_preload_libraries", true, false);
	pg_stat_statements = strstr(shared_preload_libraries_config, "pg_stat_statements");
	if (pg_stat_statements == NULL)
	{
		ereport(WARNING, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), 
                                          errmsg("pg_logqueryid: pg_stat_statements not loaded")));
		pglqi_enabled = false;
	}
	auto_explain = strstr(shared_preload_libraries_config, "auto_explain");
	if (auto_explain == NULL)
	{
		ereport(WARNING, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), 
                                          errmsg("pg_logqueryid: auto_explain not loaded")));
		pglqi_enabled = false;
	}

	auto_explain_log_min_duration_config = GetConfigOption("auto_explain.log_min_duration", true, false);
	if (auto_explain_log_min_duration_config != NULL)
	{
		parse_int(auto_explain_log_min_duration_config, &auto_explain_log_min_duration, -1, NULL); 
		ereport(LOG, (errmsg("pg_logqueryid:_PG_init(): auto_explain.log_min_duration=%d", 
                              auto_explain_log_min_duration)));
	}

	auto_explain_log_nested_statements_config = GetConfigOption("auto_explain.log_nested_statements", true, false);
	if (auto_explain_log_nested_statements_config != NULL)
	{
		ereport(LOG, (errmsg("pg_logqueryid:_PG_init(): auto_explain.log_nested_statements=%s", 
                              auto_explain_log_nested_statements_config)));
		if (strcmp(auto_explain_log_nested_statements_config,"on") == 1)
			auto_explain_log_nested_statements = 1;
	}

	if (pglqi_enabled)
		ereport(LOG, (errmsg("pg_logqueryid:_PG_init(): pg_logqueryid is enabled")));
	else
		ereport(LOG, (errmsg("pg_logqueryid:_PG_init(): pg_logqueryid is not enabled")));
			

/* Install hooks only on leader. */
#if PG_VERSION_NUM >= 90600
	if (!IsParallelWorker())
	{
#endif
		prev_ExecutorStart = ExecutorStart_hook;
		ExecutorStart_hook = pglqi_ExecutorStart;
		prev_ExecutorRun = ExecutorRun_hook;
		ExecutorRun_hook = pglqi_ExecutorRun;
		prev_ExecutorFinish = ExecutorFinish_hook;
		ExecutorFinish_hook = pglqi_ExecutorFinish;
#if PG_VERSION_NUM >= 90600
	}
#endif
	elog(DEBUG5, "pg_logqueryid:_PG_init():exit");
}


/*
 *  Module unload callback
 */
void
_PG_fini(void)
{
	/* Uninstall hooks only on leader. */
#if PG_VERSION_NUM >= 90600
	if (!IsParallelWorker())
	{
#endif
		ExecutorStart_hook = prev_ExecutorStart;
		ExecutorRun_hook = prev_ExecutorRun;
		ExecutorFinish_hook = prev_ExecutorFinish;
#if PG_VERSION_NUM >= 90600
	}
#endif
}

static void
pglqi_ExecutorStart(QueryDesc *queryDesc, int eflags)
{

	elog(DEBUG5, "pg_logqueryid: pglqi_ExecutorStart: entry");
	if (pglqi_enabled)
	{
		uint64		queryId = queryDesc->plannedstmt->queryId;
		ereport(LOG, (errmsg("pg_logqueryid: queryId=%ld", queryId)));
	}

	if (prev_ExecutorStart)
		prev_ExecutorStart(queryDesc, eflags);
	else
		standard_ExecutorStart(queryDesc, eflags);
	elog(DEBUG5, "pglogqueryid: pglqi_ExecutorStart: exit");
}

static void
pglqi_ExecutorRun(QueryDesc *queryDesc,
				 ScanDirection direction,
#if PG_VERSION_NUM >= 90600
				 uint64 count
#else
				 long count
#endif
#if PG_VERSION_NUM >= 100000
				 ,bool execute_once
#endif
)
{
	elog(DEBUG5, "pg_logqueryid: pglqi_ExecutorRun: entry");
	pglqi_nesting_level++;
	ereport(DEBUG5, (errmsg("pg_logqueryid: pglqi_ExecutorRun: nesting_level=%d", pglqi_nesting_level)));
	PG_TRY();
	{
		if (prev_ExecutorRun)
#if PG_VERSION_NUM >= 100000
			prev_ExecutorRun(queryDesc, direction, count, execute_once);
#else
			prev_ExecutorRun(queryDesc, direction, count);
#endif
		else
#if PG_VERSION_NUM >= 100000
			standard_ExecutorRun(queryDesc, direction, count, execute_once);
#else
			standard_ExecutorRun(queryDesc, direction, count);
#endif
	}
	PG_CATCH();
	{
		pglqi_nesting_level--;
		PG_RE_THROW();
	}
	PG_END_TRY();
	ereport(DEBUG5, (errmsg("pg_logqueryid:pglqi_ExecutorRun: nesting_level=%d", pglqi_nesting_level)));
	elog(DEBUG5, "pglqi_ExecutorRun: exit");
}

static void
pglqi_ExecutorFinish(QueryDesc *queryDesc)
{
	elog(DEBUG5, "pg_logqueryid: pglsqi_ExecutorFinish: entry");
	pglqi_nesting_level++;
	ereport(DEBUG5, (errmsg("pg_logqeueryid: pglqi_ExecutorFinish: nesting_level=%d", pglqi_nesting_level)));

	PG_TRY();
	{
	if (prev_ExecutorFinish)
			prev_ExecutorFinish(queryDesc);
	else
			standard_ExecutorFinish(queryDesc);
	}
	PG_CATCH();
	{
		pglqi_nesting_level--;
		PG_RE_THROW();
	}
	PG_END_TRY();
	ereport(DEBUG5, (errmsg("pg_logqueryid: pglqi_ExecutorFinish: nesting_level=%d", pglqi_nesting_level)));
	elog(DEBUG5, "pglqi_ExecutorFinish: exit");
}

