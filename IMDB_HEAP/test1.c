#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <malloc.h>
#include "dbmalloc.h"
//#include <whitedb/dbapi.h>
//#include "dbapi.h"

int main(int argc, char **argv){
	const char* test_str = "TEST STRING HEAP";
	// create new db pointer used for allocate db
  void* db  = NULL;//heap_db_attach(20000000);

  // in order to use db, we have to attach the db with shared memroy allocated by OS.
  db = db_heap_create("1000", 2000000);

  char* tp = NULL;
  int i = 0;

  // test: allocate 200000 times and try to copy something into allocaed space
  // inorder to test basic feature and memroy leak
  for( i = 0; i < 2000000; ++i) { 
    // allocate 
    // this statement shows basic way to use dbmalloc: it is almost the same as system one
    // except for the first paramater: db pointer 
    //                     (we have to let malloc know which heap it is using now.)
    // second paramater is size of space, in byte.
  	tp = (char*)dbmalloc(db,1024*sizeof(char));

    strcpy(tp,test_str);
  	//free
    //the free function is the same as system, execept for first paramater pointing to db that is being used
    dbfree(db,tp);
  
  }


  //delete data base that we used in this program
	wg_delete_database("1000");
  //heap_db_detach(db);

  return 0;
}