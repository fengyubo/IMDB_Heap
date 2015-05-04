#include <stdio.h>
//#include <whitedb/dbapi.h>
#include "dbapi.h"

int main(int argc, char **argv){
	void *db, *rec;
	wg_int enc;

  	db = wg_attach_database("1000", 2000000);

  	/* create some records for testing */
  	rec = wg_create_record(db, 2);
  	char tmp[1024] = "this is a memory test";
  	char* type = "memory";
  	//enc = wg_encode_blob(db, (char*)tmp, (char*)type, 1024); /* will match */
  	int test_address = wg_encode_blob(db, (char*)tmp, (char*)type, 1024);
 printf("address = 0x%x\n",test_address);
	//wg_set_field(db, rec, 0, wg_encode_int(db, 1024));
	// wg_set_field(db, rec, 1, enc);

 //  	rec = wg_find_record_int(db, 0, WG_COND_EQUAL, 1024, NULL);
 //  	//if(rec)
	// char *tp = wg_decode_blob(db, wg_get_field(db,rec,1));
	// if(tp){
	// 	printf("%s\n",tp);
	// }


	wg_delete_database("1000");




  return 0;
}