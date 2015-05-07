#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <malloc.h>
#include "dbapi.h"

int main(int argc, char **argv){
	void *db, *rec;
	wg_int enc;
	const char* test_str = "TEST STRING HEAP";

  	db = wg_attach_database("1000", 2000000);

  	/* create some records for testing */
  	rec = wg_create_record(db, 2);
  	char* test = (char*)malloc(1024*sizeof(char));

  	memcpy((char*)test,(char*)test_str,strlen(test_str));
  	char tmp[sizeof(void*)+1] = {0x0};
  	*((long*)tmp) = (long)test;

  	printf("%x\n",test);
//  	char* type = "memory_ptr";
//	enc = wg_encode_blob(db, (char*)tmp, (char*)type, sizeof(void*));
	enc = wg_encode_str(db, (char*)tmp, NULL);
	wg_set_field(db, rec, 0, wg_encode_int(db, 1024));
	wg_set_field(db, rec, 1, enc);

	//rec = wg_find_record_int(db, 0, WG_COND_EQUAL, 1024, NULL);
	rec = wg_find_record_str(db, 1, WG_COND_EQUAL, tmp, NULL);
	char *tp = wg_decode_str(db, wg_get_field(db,rec,1));
	printf("%x\n",*(long*)tp);
	if(tp){
		printf("%s\n",(char*)(*(long*)tp));
	}
	wg_delete_database("1000");




  return 0;
}