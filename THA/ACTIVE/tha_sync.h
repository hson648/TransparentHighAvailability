#ifndef __THA_SYNC__
#define __THA_SYNC__

#include "common.h"

typedef struct _tha_handle_t tha_handle_t;
typedef struct _tha_object_db_rec_t tha_object_db_rec_t;
typedef struct _tha_struct_db_rec_t tha_struct_db_rec_t;
typedef struct _tha_field_info_ tha_field_info_t;
/* Serialized Structure*/

typedef struct _serialized_object_t{
  char obj_id[MAX_STRUCTURE_NAME_SIZE];
  char struct_name[MAX_STRUCTURE_NAME_SIZE];
  int n_fields; // no of changed fields
  int field_index[0];
  char field_val[0];
} serialized_object_t;


typedef struct _tha_sync_pending_linkage{
	void **from_ptr;
	char target_obj_id[MAX_STRUCTURE_NAME_SIZE];
} tha_sync_dependency_graph_pending_linkage_list_t;


typedef struct _tha_save_hashcode_recompute_data{
	int *hash_code;
	char *start_ptr;
	int len;
} tha_save_hashcode_recompute_data_t;

/* Operations */
#define CREATE_OBJ		0
#define UPDATE_OBJ		1
#define DELETE_OBJ		2
#define RESET_OBJ		3
#define CLEAN_STATE		4
#define BULK_SYNC		5
#define ORPHAN_POINTER_COMMIT	6
#define COMMIT			7
#define ROLE_CHANGE_TO_ACTIVE	8


#define SYNC_ALL_FIELDS		1
#define SYNC_CHANGED_FIELDS	0

typedef struct _serialize_hdr_t{
	int op_code;      // create/delete/update
	int payload_size; // excluding header
	int units;	  // meaningful only for Ist header instance.
} serialize_hdr_t;

typedef struct serialized_buffer ser_buff_t;

#define SERIALIZED_HDR_SIZE	(sizeof(((serialize_hdr_t *)0)->op_code) +	 	\
				 sizeof(((serialize_hdr_t *)0)->payload_size) + 	\
				 sizeof(((serialize_hdr_t *)0)->units))

ser_buff_t*
serialize_object(tha_handle_t *handle, tha_object_db_rec_t *obj_rec);

ser_buff_t *
serialize_object_all_fields(tha_handle_t *handle, tha_object_db_rec_t *obj_rec);

int
de_serialize_buffer(tha_handle_t *handle, char *msg , int size);

int
de_serialize_object(tha_handle_t *handle, char *obj_ptr, ser_buff_t *b, 
			tha_struct_db_rec_t *struct_rec);

void
serialize_internal_object(tha_handle_t *handle, ser_buff_t *b,
                          char *obj_ptr,
                          tha_struct_db_rec_t *struct_rec);
void
de_serialize_internal_object(tha_handle_t *handle, ser_buff_t *b,
                                 char *obj_ptr, tha_struct_db_rec_t *struct_rec);


int
mark_changed_objects(tha_handle_t *handle);

void
clean_dirty_bit(tha_handle_t *handle);

int
tha_sync_object(int opcode, tha_handle_t *handle, tha_object_db_rec_t *obj_rec, int ALL_FIELDS_FLAG);

int
ha_incremental_sync(tha_handle_t *handle, char *msg, int size);

int
ha_sync(tha_handle_t *handle);

int
tha_sync_object_db(tha_handle_t *handle);

int
tha_bulk_sync(tha_handle_t *handle);

#endif
