# pg_logqueryid
PostgreSQL extension to display pg_stat_statements queryid with auto_explain


# Installation
## Compiling

This module can be built using the standard PGXS infrastructure. For this to work, the pg_config program must be available in your $PATH:
  
`git clone https://github.com/pierreforstmann/pg_logqueryid.git` <br>
`cd pg_logqueryid` <br>
`make` <br>
`make install` <br>

## PostgreSQL setup

Extension can be loaded:

In local session with `LOAD 'pg_logqueryid'`; <br>
Using `session_preload_libraries` parameter in a specific connection <br>
At server level with `shared_preload_libraries` <br>

# Usage
pg_logqueryid has no specific GUC.
To use it pg_stat_statements and auto_explain extensions must be loaded and configured. If this is not the case pg_logqueryid can be loaded but is not enabled.
