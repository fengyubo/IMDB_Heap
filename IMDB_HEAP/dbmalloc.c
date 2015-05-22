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
#include "config-w32.h"
#else
#include "config.h"
#endif

#include "dbmalloc.h"

// api for create database and attach exists db
// void* 
// db_heap_create(char* db_name, int size) {
// 	void* db = NULL;

//     return (db = wg_attach_database((char*)db_name, size)) == NULL ? NULL : db;

// }

// void*
// db_heap_attach(int size) {
// 	void* db = NULL;

//     return (db = wg_attach_local_database(size)) == NULL ? NULL : db;

// }

// void
// db_heap_delete(char* db_name) {
// 	wg_delete_database(db_name);
// 	return ;
// }

/* malloc function: given ptr to db and size of required space, then return ptr to the space */
void* 
dbmalloc(void* db, int size) {
	wg_int enc_space_data,enc_space_ptr, enc_space_size, enc_reserve;
	void *rec;

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
    // store pointer it in record: in double way
  	enc_space_ptr = wg_encode_double(db, (long int)space_ptr);
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
void 
dbfree(void* db, void* free_ptr){
  void* rec;

  if(!free_ptr) return ;

  rec = wg_find_record_double(db, 0, WG_COND_EQUAL, (long int)free_ptr, NULL);
  if( !rec ) {
  	printf("ERR: no such pointer\n");
  	return ;
  }
  if(wg_delete_record_heap(db, rec)) {
    printf("ERR: free error\n");
  }

  return ;
}

/** Delete record from database
 * returns 0 on success
 * returns -1 if the record is referenced by others and cannot be deleted.
 * returns -2 on general error
 * returns -3 on fatal error
 *
 * XXX: when USE_BACKLINKING is off, this function should be used
 * with extreme care.
 */
static gint wg_delete_record_heap(void* db, void *rec) {
  gint offset;
  gint* dptr;
  gint* dendptr;
  gint data;

#ifdef CHECK
  if (!dbcheck(db)) {
    fprintf(stderr,"wg data deleting error: wrong database pointer given to wg_delete_record");
    return -2;
  }
#endif

#ifdef USE_BACKLINKING
  if(*((gint *) rec + RECORD_BACKLINKS_POS))
    return -1;
#endif

#ifdef USE_DBLOG
  /* Log first, modify shared memory next */
  if(dbmemsegh(db)->logging.active) {
    if(wg_log_delete_record(db, ptrtooffset(db, rec)))
      return -3;
  }
#endif

  /* Remove data from index */
  if(!is_special_record(rec)) {
    if(wg_index_del_rec(db, rec) < -1)
      return -3; /* index error */
  }

  offset = ptrtooffset(db, rec);
#if defined(CHECK) && defined(USE_CHILD_DB)
  /* Check if it's a local record */
  if(!is_local_offset(db, offset)) {
    fprintf(stderr,"wg data handling error: not deleting an external record");
    return -2;
  }
#endif
  /* Loop over fields, freeing them */
  dendptr = (gint *) (((char *) rec) + datarec_size_bytes(*((gint *)rec)));
  for(dptr=(gint *)rec+RECORD_HEADER_GINTS; dptr<dendptr; dptr++) {
    data = *dptr;

#ifdef USE_BACKLINKING
    /* Is the field value a record pointer? If so, remove the backlink. */
#ifdef USE_CHILD_DB
    if(wg_get_encoded_type(db, data) == WG_RECORDTYPE &&
      is_local_offset(db, decode_datarec_offset(data))) {
#else
    if(wg_get_encoded_type(db, data) == WG_RECORDTYPE) {
#endif
      gint *child = (gint *) wg_decode_record(db, data);
      gint *next_offset = child + RECORD_BACKLINKS_POS;
      gcell *old = NULL;

      while(*next_offset) {
        old = (gcell *) offsettoptr(db, *next_offset);
        if(old->car == offset) {
          gint old_offset = *next_offset;
          *next_offset = old->cdr; /* remove from list chain */
          wg_free_listcell(db, old_offset); /* free storage */
          goto recdel_backlink_removed;
        }
        next_offset = &(old->cdr);
      }
      fprintf(stderr,"wg data handling error: Corrupt backlink chain");
      return -3; /* backlink error */
    }
recdel_backlink_removed:
#endif

    if(isptr(data)) free_field_encoffset_heap(db,data);
  }

  /* Free the record storage */
  wg_free_object(db,
    &(dbmemsegh(db)->datarec_area_header),
    offset);

  return 0;
}

/** properly removes ptr (offset) to data
*
* assumes fielddata is offset to allocated data
* depending on type of fielddata either deallocates pointed data or
* removes data back ptr or decreases refcount
*
* in case fielddata points to record or longstring, these
* are freed only if they have no more pointers
*
* returns non-zero in case of error
*/

static gint free_field_encoffset_heap(void* db,gint encoffset) {
  gint offset;
#if 0
  gint* dptr;
  gint* dendptr;
  gint data;
  gint i;
#endif
  gint tmp;
  gint* objptr;
  gint* extrastr;

  // takes last three bits to decide the type
  // fullint is represented by two options: 001 and 101
  switch(encoffset&NORMALPTRMASK) {
    case DATARECBITS:
#if 0
/* This section of code in quarantine */
      // remove from list
      // refcount check
      offset=decode_datarec_offset(encoffset);
      tmp=dbfetch(db,offset+sizeof(gint)*LONGSTR_REFCOUNT_POS);
      tmp--;
      if (tmp>0) {
        dbstore(db,offset+LONGSTR_REFCOUNT_POS,tmp);
      } else {
        // free frompointers structure
        // loop over fields, freeing them
        dptr=offsettoptr(db,offset);
        dendptr=(gint*)(((char*)dptr)+datarec_size_bytes(*dptr));
        for(i=0,dptr=dptr+RECORD_HEADER_GINTS;dptr<dendptr;dptr++,i++) {
          data=*dptr;
          if (isptr(data)) free_field_encoffset(db,data);
        }
        // really free object from area
        wg_free_object(db,&(dbmemsegh(db)->datarec_area_header),offset);
      }
#endif
      break;
    case LONGSTRBITS:
      offset=decode_longstr_offset(encoffset);
#ifdef USE_CHILD_DB
      if(!is_local_offset(db, offset))
        break; /* Non-local reference, ignore it */
#endif
      // refcount check
      tmp=dbfetch(db,offset+sizeof(gint)*LONGSTR_REFCOUNT_POS);
      tmp--;
      if (tmp>0) {
        dbstore(db,offset+sizeof(gint)*LONGSTR_REFCOUNT_POS,tmp);
      } else {
        objptr = (gint *) offsettoptr(db,offset);
        extrastr=(gint*)(((char*)(objptr))+(sizeof(gint)*LONGSTR_EXTRASTR_POS));
        tmp=*extrastr;
        // remove from hash
        wg_remove_from_strhash_heap(db,encoffset);
        // remove extrastr
        if (tmp!=0) free_field_encoffset_heap(db,tmp);
        *extrastr=0;
        // really free object from area
        wg_free_object(db,&(dbmemsegh(db)->longstr_area_header),offset);
      }
      break;
    case SHORTSTRBITS:
#ifdef USE_CHILD_DB
      offset = decode_shortstr_offset(encoffset);
      if(!is_local_offset(db, offset))
        break; /* Non-local reference, ignore it */
      wg_free_shortstr(db, offset);
#else
      wg_free_shortstr(db,decode_shortstr_offset(encoffset));
#endif
      break;
    case FULLDOUBLEBITS:
#ifdef USE_CHILD_DB
      offset = decode_fulldouble_offset(encoffset);
      if(!is_local_offset(db, offset))
        break; /* Non-local reference, ignore it */
      wg_free_doubleword(db, offset);
#else
      wg_free_doubleword(db,decode_fulldouble_offset(encoffset));
#endif
      break;
    case FULLINTBITSV0:
#ifdef USE_CHILD_DB
      offset = decode_fullint_offset(encoffset);
      if(!is_local_offset(db, offset))
        break; /* Non-local reference, ignore it */
      wg_free_word(db, offset);
#else
      wg_free_word(db,decode_fullint_offset(encoffset));
#endif
      break;
    case FULLINTBITSV1:
#ifdef USE_CHILD_DB
      offset = decode_fullint_offset(encoffset);
      if(!is_local_offset(db, offset))
        break; /* Non-local reference, ignore it */
      wg_free_word(db, offset);
#else
      wg_free_word(db,decode_fullint_offset(encoffset));
#endif
      break;

  }
  return 0;
}


/* Remove longstr from strhash
*
*  Internal langstr etc are not removed by this op.
*
*/

static gint wg_remove_from_strhash_heap(void* db, gint longstr) {
  db_memsegment_header* dbh = dbmemsegh(db);
  gint type;
  gint* extrastrptr;
  char* extrastr;
  char* data;
  gint length;
  gint hash;
  gint chainoffset;
  gint hashchain;
  gint nextchain;
  gint offset;
  gint* objptr;
  gint fldval;
  gint objsize;
  gint strsize;
  gint* typeptr;

  //printf("wg_remove_from_strhash called on %d\n",longstr);
  //wg_debug_print_value(db,longstr);
  //printf("\n\n");
  offset=decode_longstr_offset(longstr);
  objptr=(gint*) offsettoptr(db,offset);
  // get string data elements
  //type=objptr=offsettoptr(db,decode_longstr_offset(data));
  extrastrptr=(gint *) (((char*)(objptr))+(LONGSTR_EXTRASTR_POS*sizeof(gint)));
  fldval=*extrastrptr;
  if (fldval==0) extrastr=NULL;
  else extrastr=wg_decode_str(db,fldval);
  data=((char*)(objptr))+(LONGSTR_HEADER_GINTS*sizeof(gint));
  objsize=getusedobjectsize(*objptr);
  strsize=objsize-(((*(objptr+LONGSTR_META_POS))&LONGSTR_META_LENDIFMASK)>>LONGSTR_META_LENDIFSHFT);
  length=strsize;
  typeptr=(gint*)(((char*)(objptr))+(+LONGSTR_META_POS*sizeof(gint)));
  type=(*typeptr)&LONGSTR_META_TYPEMASK;
  //type=wg_get_encoded_type(db,longstr);
  // get hash of data elements and find the location in hashtable/chains
  hash=wg_hash_typedstr(db,data,extrastr,type,length);
  chainoffset=((dbh->strhash_area_header).arraystart)+(sizeof(gint)*hash);
  hashchain=dbfetch(db,chainoffset);
  while(hashchain!=0) {
    if (hashchain==longstr) {
      nextchain=dbfetch(db,decode_longstr_offset(hashchain)+(LONGSTR_HASHCHAIN_POS*sizeof(gint)));
      dbstore(db,chainoffset,nextchain);
      break;
    }
    chainoffset=decode_longstr_offset(hashchain)+(LONGSTR_HASHCHAIN_POS*sizeof(gint));
    hashchain=dbfetch(db,chainoffset);
  }
  //HEAP: this part is modified, there probably some bug, since ignoring the non-exists hash ptr, which is not caused by heap blob
//  show_consistency_error_nr(db,"string not found in hash during deletion, offset",offset);
  return 0;
}

gint 
wg_encode_blob_heap(void* db, wg_int len) {
#ifdef CHECK
  if (!dbcheck(db)) {
    fprintf(stderr,"wg data handling error: wrong database pointer given to wg_encode_blob");
    return WG_ILLEGAL;
  }
#endif
  return wg_encode_uniblob_heap(db,WG_BLOBTYPE,len);
}


gint 
wg_encode_uniblob_heap(void* db, gint type, gint len) {
  gint offset;
  if (0) {
  } else {
    offset=find_create_longstr_heap(db,type,len);
    if (!offset) {
      fprintf(stderr,"wg data handling error: cannot create a blob of size ");
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

char* 
wg_decode_unistr_heap(void* db, gint data) {
  gint* objptr;
  char* dataptr;
  if (islongstr(data)) {
    objptr = (gint *) offsettoptr(db,decode_longstr_offset(data));
    dataptr=((char*)(objptr))+(LONGSTR_HEADER_GINTS*sizeof(gint));
    return dataptr;
  }
  fprintf(stderr,"wg data handling error: data given to wg_decode_unistr is not long string");
  return NULL;
}
