#!/bin/bash

## before this works, setup the following table in tmp.db:

echo "CREATE TABLE IF NOT EXISTS types (
     Name TEXT UNIQUE,
     Def TEXT NOT NULL,
     OutputFormat TEXT NOT NULL);" | ./sqlite_shell tmp.db



./import_type tmp.db 'long' '' 'tracerlogAddInt(%1%, %2%);'
./import_type tmp.db 'short' '' 'tracerlogAddShort(%1%, %2%);'
./import_type tmp.db '_Bool' '' 'tracerlogAddFmtString(%1%, %2% ? "true" : "false");'
./import_type tmp.db 'size_t' '' 'tracerlogAddInt(%1%, %2%);'
./import_type tmp.db 'long long' '' 'tracerlogAddLongLong(%1%, %2%);'
./import_type tmp.db 'string' '' 'tracerlogAddFmtString(%1%, %2%);'
./import_type tmp.db 'void *' '' 'tracerlogAddInt(%1%, (long)%2%);'
./import_type tmp.db 'token_t' 'typedef struct {char *value; size_t length;} token_t;' 'tracerlogAddFmtString(%1%, "{\"value\":\"%%s\", \"length\":%%d}", %2%.value, %2%.length);'
./import_type tmp.db 'item' 'typedef struct _stritem{struct _stritem *next; struct _stritem *prev; struct stritem *h_next; unsigned int time; unsigned int exptime; int nbytes; unsigned short refcount; uint8_t nsuffix; uint8_t it_flags; uint8_t slabs_clsid; uint8_t nkey; void *end[];} item;' 'tracerlogAddFmtString(%1%, "{\"next\":\"%%p\", \"prev\":\"%%p\", \"h_next\":\"%%p\", \"time\":%%d, \"exptime\":%%d, \"nbytes\":%%d, \"refcount\":%%u, \"nsuffix\":%%u, \"it_flags\":%%u, \"slabs_clsid\":%%u, \"nkey\":%%u, \"end\":\"%%p\"}", %2%.next, %2%.prev, %2%.h_next, %2%.time, %2%.exptime, %2%.nbytes, %2%.refcount, %2%.nsuffix, %2%.it_flags, %2%.slabs_clsid, %2%.nkey, %2%.end);'
./import_type tmp.db 'fd_queue_info_t' 'typedef struct {uint32_t idlers; int* idlers_mutex; int* wait_for_idler; int terminated; int max_idlers; int max_recycled_pools; uint32_t recycled_pools_count; int* recycled_pools;} fd_queue_info_t;' 'tracerlogAddFmtString(%1%, "{\"idlers\":\"%%d\", \"idlers_mutex\":\"%%p\", \"wait_for_idlers\":\"%%p\", \"termianted\":%%d, \"max_idlers\":%%d, \"max_recycled_pools\":%%d, \"recycled_pools_count\":%%u, \"recycled_pools\":\"%%p\"}", %2%.idlers, %2%.idlers_mutex, %2%.terminated, %2%.max_idlers, %2%.max_recycled_pools, %2%.recycled_pools_count, %2%.recycled_pools);'
