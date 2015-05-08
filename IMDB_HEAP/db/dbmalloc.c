#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
/* For Sleep() */
#include <windows.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <sys/timeb.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#include "../config-w32.h"
#else
#include "../config.h"
#endif
//#include "dbapi.h"
#include "dballoc.h"
#include "dbquery.h"
#include "dbdata.h"
#include "dbhash.h"
#include "dblog.h"
#include "dbindex.h"
#include "dbcompare.h"
#include "dblock.h"

/* malloc function: given ptr to db and size of required space, then return ptr to the space */
void* dbmalloc(void* db, int size) {
	wg_int enc_space_data,enc_space_ptr, enc_space_size, enc_reserve;
	void *rec;
	char ptr_tmp[sizeof(void*)+1] = {0x0};

  //create malloc record for new request
  	rec = wg_create_record(db, 3);
  	if(!rec) {
      printf("ERR: Failed to create the heap record\n");
      return NULL;
    }

    // encode space data, get encoded offest/enc_space_data
	enc_space_data = wg_encode_blob_heap(db,size);
	if(enc_space_data == WG_ILLEGAL) {
      printf("ERR: Failed to encode the space data\n");
      return NULL;
    }
	if(wg_set_field(db, rec, 1, enc_space_data)) {
      printf("ERR: field 1 failed\n");
      return NULL;
    }

    // encode pointer, which will be used to find the record, stored in first filed
    void* space_ptr = (void*)(wg_decode_unistr_heap(db, enc_space_data));
    if(!space_ptr) {
    	printf("ERR: decode failed in malloc\n");
    	return NULL;
    }
    // turn pointer to a string and store it in record
  	*((long*)ptr_tmp) = (long)space_ptr;
  	enc_space_ptr = wg_encode_str(db, (char*)ptr_tmp, NULL);
  	if(enc_space_ptr == WG_ILLEGAL) {
      printf("ERR: Failed to encode the space data\n");
      return NULL;
    }
    if(wg_set_field(db, rec, 0, enc_space_ptr)) {
      printf("ERR: field 0 failed.\n");
      return NULL;
    }

    //last one: size of the space
	if(wg_set_field(db, rec, 2, wg_encode_int(db, size))) {
      printf("ERR: field 2 failed.\n");
      return NULL;
    }

    return space_ptr;
}

/*free function: given ptr to db and free pointer, return void*/
void dbfree(void* db, void* free_ptr){
  void* rec;
  char ptr_tmp[sizeof(void*)+1] = {0x0};

  if(!free_ptr) return ;
  *((long*)ptr_tmp) = (long)free_ptr;

  rec = wg_find_record_str(db, 0, WG_COND_EQUAL, (char*)ptr_tmp, NULL);
  //memset( free_ptr, 0, wg_decode_int(db, wg_get_field(db,rec,2) ) );

  if(wg_delete_record(db, rec)) {
    printf("ERR: free error\n");
  }

  return ;
}


gint wg_encode_blob_heap(void* db, wg_int len) {
#ifdef CHECK
  if (!dbcheck(db)) {
    show_data_error(db,"wrong database pointer given to wg_encode_blob");
    return WG_ILLEGAL;
  }
#endif
  return wg_encode_uniblob_heap(db,WG_BLOBTYPE,len);
}


gint wg_encode_uniblob_heap(void* db, gint type, gint len) {
  gint offset;
  if (0) {
  } else {
    offset=find_create_longstr_heap(db,type,len);
    if (!offset) {
      show_data_error_nr(db,"cannot create a blob of size ",len);
      return 0;
    }
    return encode_longstr_offset(offset);
  }
}

static gint find_create_longstr_heap(void* db, gint type, gint length) {
  db_memsegment_header* dbh = dbmemsegh(db);
  gint offset;
  size_t i;
  gint tmp;
  gint lengints;
  gint lenrest;
  char* lstrptr;
  gint old=0;
  gint res;
  if (0) {
  } else {
 	  // allocate a new string
    lengints=length/sizeof(gint);  // 7/4=1, 8/4=2, 9/4=2,
    lenrest=length%sizeof(gint);  // 7%4=3, 8%4=0, 9%4=1,
    if (lenrest) lengints++;
    offset=wg_alloc_gints(db,
                     &(dbmemsegh(db)->longstr_area_header),
                    lengints+LONGSTR_HEADER_GINTS);//this is the ptr to head of all things
    if (!offset) {
      //show_data_error_nr(db,"cannot create a data string/blob of size ",length);
      return 0;
    }
    lstrptr=(char*)(offsettoptr(db,offset));
    // clear string contents: this is optional
    memset(lstrptr+(LONGSTR_HEADER_GINTS*sizeof(gint)),0,length);
    //zero the rest
    for(i=0;lenrest && i<sizeof(gint)-lenrest;i++) {
      *(lstrptr+length+(LONGSTR_HEADER_GINTS*sizeof(gint))+i)=0;
    }
    //if no more extra string, then this part is modified
    dbstore(db,offset+LONGSTR_EXTRASTR_POS*sizeof(gint),0);
    // store metainfo: full obj len and str len difference, plus type
    tmp=(getusedobjectsize(*((gint*)lstrptr))-length)<<LONGSTR_META_LENDIFSHFT;
    tmp=tmp|type; // subtype of str stored in lowest byte of meta
    //printf("storing obj size %d, str len %d lengints %d lengints*4 %d lenrest %d lendiff %d metaptr %d meta %d \n",
    //  getusedobjectsize(*((gint*)lstrptr)),strlen(data),lengints,lengints*4,lenrest,
    //  (getusedobjectsize(*((gint*)lstrptr))-length),
    //  ((gint*)(offsettoptr(db,offset)))+LONGSTR_META_POS,
    //  tmp);
    dbstore(db,offset+LONGSTR_META_POS*sizeof(gint),tmp); // type and str length diff
    dbstore(db,offset+LONGSTR_REFCOUNT_POS*sizeof(gint),0); // not pointed from anywhere yet
    dbstore(db,offset+LONGSTR_BACKLINKS_POS*sizeof(gint),0); // no backlinks yet
    // encode
    res=encode_longstr_offset(offset);
    // store to hash and update hashchain
    //dbstore(db,((dbh->strhash_area_header).arraystart)+(sizeof(gint)*hash),res);
    //printf("hasharrel 2 %d \n",hasharrel);
    //dbstore(db,offset+LONGSTR_HASHCHAIN_POS*sizeof(gint),hasharrel); // store old hash array el
    // return result
    return res;
  }
}

char* wg_decode_unistr_heap(void* db, gint data) {
  gint* objptr;
  char* dataptr;
  if (islongstr(data)) {
    objptr = (gint *) offsettoptr(db,decode_longstr_offset(data));
    dataptr=((char*)(objptr))+(LONGSTR_HEADER_GINTS*sizeof(gint));
    return dataptr;
  }
  show_data_error(db,"data given to wg_decode_unistr is not long string");
  return NULL;
}

// error message print
// could be opt
static gint show_data_error(void* db, char* errmsg) {
#ifdef WG_NO_ERRPRINT
#else
  fprintf(stderr,"wg data handling error: %s\n",errmsg);
#endif
  return -1;

}

static gint show_data_error_nr(void* db, char* errmsg, gint nr) {
#ifdef WG_NO_ERRPRINT
#else
  fprintf(stderr,"wg data handling error: %s %d\n", errmsg, (int) nr);
#endif
  return -1;

}

static gint show_data_error_double(void* db, char* errmsg, double nr) {
#ifdef WG_NO_ERRPRINT
#else
  fprintf(stderr,"wg data handling error: %s %f\n",errmsg,nr);
#endif
  return -1;

}

static gint show_data_error_str(void* db, char* errmsg, char* str) {
#ifdef WG_NO_ERRPRINT
#else
  fprintf(stderr,"wg data handling error: %s %s\n",errmsg,str);
#endif
  return -1;
}