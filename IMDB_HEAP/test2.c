#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <malloc.h>
//#include <whitedb/dbapi.h>
#include "dbapi.h"

int main(int argc, char **argv){
	const char* test_str = "TEST STRING HEAP";
	void* db;
  	db = wg_attach_database("1000", 2000000);
  	char* tp = (char*)dbmalloc(db,1024);
  	strcpy(tp,test_str);
  	printf("%s\n",tp);
	wg_delete_database("1000");




  return 0;
}