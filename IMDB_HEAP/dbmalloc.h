#include "Db/dballoc.h"
#include "Db/dbquery.h"
#include "Db/dbdata.h"
#include "Db/dbhash.h"
#include "Db/dblog.h"
#include "Db/dbindex.h"
#include "Db/dbcompare.h"
#include "Db/dblock.h"


void* db_heap_create(char* db_name, int size);
void* db_heap_attach(int size);
void db_heap_delete(char* db_name);

//main malloc and free process
void* dbmalloc(void* db, int size);
void  dbfree(void* db, void* free_ptr);

// private protocal for malloc process: encode and allocate space
gint  wg_encode_blob_heap(void* db, wg_int len);
gint  wg_encode_uniblob_heap(void* db, gint type, gint len);
static gint find_create_longstr_heap(void* db, gint type, gint length);
char* wg_decode_unistr_heap(void* db, gint data);


// private protocal for delloc process: recycle space
static gint wg_delete_record_heap(void* db, void *rec);
static gint free_field_encoffset_heap(void* db,gint encoffset);
static gint wg_remove_from_strhash_heap(void* db, gint longstr);

