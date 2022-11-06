# pg_logqueryid
PostgreSQL extension to display pg_stat_statements queryid with auto_explain


# Installation
## Compiling

This module can be built using the standard PGXS infrastructure. For this to work, the `pg_config` program must be available in your $PATH:
  
`git clone https://github.com/pierreforstmann/pg_logqueryid.git` <br>
`cd pg_logqueryid` <br>
`make` <br>
`make install` <br>

This extension has been validated with PostgreSQL 9.5, 9.6, 10, 11, 12, 13, 14 and 15.

## PostgreSQL setup

Extension can be loaded:

1. in local session with `LOAD 'pg_logqueryid'`; <br>
2. using `session_preload_libraries` parameter in a specific connection <br>
3. at server level with `shared_preload_libraries` parameter. <br> 

## Usage
`pg_logqueryid` has no specific GUC.
To use it `pg_stat_statements` and `auto_explain` extensions must be loaded and configured. If this is not the case `pg_logqueryid` can be loaded but is not enabled.

## Example

In postgresql.conf:

`shared_preload_libraries = 'pg_stat_statements,auto_explain'` <br>
`pg_stat_statements.max = 10000` <br>
`pg_stat_statements.track = all` <br>
`auto_explain.log_min_duration=0` <br>

In the current database connection:

`pierre=# load 'pg_logqueryid';`<br>
`LOAD` <br>


In this setup all SQL statements are auto explained and server log displays for current database session:

`2020-03-28 14:47:08.633 CET [19735] LOG:  pg_logqueryid: queryId=5917340101676597114` <br>
`2020-03-28 14:47:08.633 CET [19735] STATEMENT:  SELECT e.extname AS "Name", e.extversion AS "Version", n.nspname AS "Schema", c.description AS "Description"` <br>
	`FROM pg_catalog.pg_extension e LEFT JOIN pg_catalog.pg_namespace n ON n.oid = e.extnamespace LEFT JOIN` <br> `pg_catalog.pg_description c ON c.objoid = e.oid AND c.classoid = 'pg_catalog.pg_extension'::pg_catalog.regclass
	ORDER BY 1;` <br>
`2020-03-28 14:47:08.633 CET [19735] LOG:  duration: 0.066 ms  plan:` <br>
	`Query Text: SELECT e.extname AS "Name", e.extversion AS "Version", n.nspname AS "Schema", c.description AS "Description"` <br>
	`FROM pg_catalog.pg_extension e LEFT JOIN pg_catalog.pg_namespace n ON n.oid = e.extnamespace LEFT JOIN` <br> `pg_catalog.pg_description c ON c.objoid = e.oid AND c.classoid = 'pg_catalog.pg_extension'::pg_catalog.regclass` <br>
	`ORDER BY 1;` <br>
	`Sort  (cost=10.46..10.47 rows=1 width=158)` <br>
	  `Sort Key: e.extname` <br>
	  `->  Nested Loop Left Join  (cost=0.28..10.45 rows=1 width=158)` <br>
	        `Join Filter: (n.oid = e.extnamespace)` <br>
	        `->  Nested Loop Left Join  (cost=0.28..9.32 rows=1 width=98)` <br>
	              `->  Seq Scan on pg_extension e  (cost=0.00..1.01 rows=1 width=76)` <br>
	              `->  Index Scan using pg_description_o_c_o_index on pg_description c  (cost=0.28..8.30 rows=1 width=30)` <br>
	                    `Index Cond: ((objoid = e.oid) AND (classoid = '3079'::oid))` <br>
	        `->  Seq Scan on pg_namespace n  (cost=0.00..1.06 rows=6 width=68)` <br>
`2020-03-28 14:47:08.633 CET [19735] LOG:  duration: 0.739 ms  statement: SELECT e.extname AS "Name", e.extversion AS "Version", n.nspname AS "Schema", c.description AS "Description"` <br>
	`FROM pg_catalog.pg_extension e LEFT JOIN pg_catalog.pg_namespace n ON n.oid = e.extnamespace LEFT JOIN pg_catalog.pg_description c ON c.objoid = e.oid AND c.classoid = 'pg_catalog.pg_extension'::pg_catalog.regclass` <br>
	`ORDER BY 1;` <br>


For this example, queryId can be checked in pg_stat_statements view:


`pierre=# select queryid, query from pg_stat_statements where queryId=5917340101676597114;` <br>
       `queryid       |                                                                               ` <br>
               `query                                                                                 ` <br>
`--------------------+-------------------------------------------------------------------------------` <br>
`----------------------------------------------------------------------------------------------------` <br>
` -----------` <br>
 `917340101676597114 | SELECT e.extname AS "Name", e.extversion AS "Version", n.nspname AS "Schema", 
c.description AS "Description" ` <br>                                                      
           `+` <br>
                   ` | FROM pg_catalog.pg_extension e LEFT JOIN pg_catalog.pg_namespace n ON n.oid = 
e.extnamespace LEFT JOIN pg_catalog.pg_description c ON c.objoid = e.oid AND c.classoid = $1::pg_cata
log.regclass+` <br>
                   ` | ORDER BY 1` <br>
`1 row)`<br>
`

