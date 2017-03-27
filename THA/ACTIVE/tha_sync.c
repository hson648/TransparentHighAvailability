#include "tha_sync.h"
#include "Utils.h"
#include "stdio.h"
#include "tha_db.h"
#include "memory.h"
#include "serialize.h"
#include "stdlib.h"
#include "DS/LinkedList/LinkedListApi.h"
#include "time.h"
#include "string.h"
#include "errno.h"

extern int tha_sync_msg_to_standby(char *msg, int size);
ll_t* orphan_pointer_list 	 = NULL;
ll_t* hash_code_recompute_list 	 = NULL;
static int BULK_SYNC_IN_PROGRESS = 0;
extern FILE *checkpoint_file;
extern char enable_checkpoint ;
extern int state_sync_start(tha_handle_t *handle, char *msg, int size);
ser_buff_t *checkpoint_load_buffer;


static void
tha_send_orphan_pointers_commit(){
	ser_buff_t *b = NULL;
	init_serialized_buffer(&b);
	serialize_hdr_t hdr;
	memset(&hdr, 0, sizeof(serialize_hdr_t));
	hdr.op_code = ORPHAN_POINTER_COMMIT;
	serialize_int32(b, hdr.op_code);
        serialize_int32(b, hdr.payload_size);
        serialize_int32(b, hdr.units);
	state_sync_start(handle, b->b, get_serialize_buffer_size(b));
	free_serialize_buffer(b);
}


static void
tha_send_commit(){
	ser_buff_t *b = NULL;
	init_serialized_buffer(&b);
	serialize_hdr_t hdr;
	memset(&hdr, 0, sizeof(serialize_hdr_t));
	hdr.op_code = COMMIT;
	serialize_int32(b, hdr.op_code);
        serialize_int32(b, hdr.payload_size);
        serialize_int32(b, hdr.units);
	state_sync_start(handle, b->b, get_serialize_buffer_size(b));
	free_serialize_buffer(b);
}

static void
tha_process_commit_orphan_pointers_list(tha_handle_t *handle){
	int i = 0;
	tha_object_db_rec_t *obj_rec = NULL;
	singly_ll_node_t *head = GET_HEAD_SINGLY_LL(orphan_pointer_list);

	printf("%s() : INFO : %d pointers shall be late binded\n", __FUNCTION__, GET_NODE_COUNT_SINGLY_LL(orphan_pointer_list));
	for(; i < (int)GET_NODE_COUNT_SINGLY_LL(orphan_pointer_list); i++){	
		tha_sync_dependency_graph_pending_linkage_list_t *link_data = 
			(tha_sync_dependency_graph_pending_linkage_list_t *)head->data;
		obj_rec = tha_object_db_lookup_by_objid(handle, link_data->target_obj_id);
		if(!obj_rec){
			printf("%s() : Error : obj_rec not found for obj_id = %s\n", __FUNCTION__, obj_rec->obj_id);
			head = GET_NEXT_NODE_SINGLY_LL(head);
			continue;
		}
		memcpy(link_data->from_ptr, &obj_rec->ptr, sizeof(void*));
		head = GET_NEXT_NODE_SINGLY_LL(head);
	}

	head = GET_HEAD_SINGLY_LL(orphan_pointer_list);
	for(i = 0; i < (int)GET_NODE_COUNT_SINGLY_LL(orphan_pointer_list); i++){
		free(head->data);
		head = GET_NEXT_NODE_SINGLY_LL(head);			
	}	

	delete_singly_ll(orphan_pointer_list);

	/*We will process hash_code recomputation as well*/
	head = GET_HEAD_SINGLY_LL(hash_code_recompute_list);
	printf("%s() : INFO : %d hashcodes shall be recomputed\n", __FUNCTION__, GET_NODE_COUNT_SINGLY_LL(hash_code_recompute_list));
	for(i = 0; i < (int)GET_NODE_COUNT_SINGLY_LL(hash_code_recompute_list); i++){
		tha_save_hashcode_recompute_data_t *hash_code_recompute_entity = 
			(tha_save_hashcode_recompute_data_t *)head->data;

		*(hash_code_recompute_entity->hash_code) = 
			hash_code(hash_code_recompute_entity->start_ptr, hash_code_recompute_entity->len);
		free(head->data);
		head = GET_NEXT_NODE_SINGLY_LL(head);
	}

	delete_singly_ll(hash_code_recompute_list);
}


void
de_serialize_internal_object(tha_handle_t *handle, ser_buff_t *b, // points to the value of this buffer
				 char *obj_ptr, tha_struct_db_rec_t *struct_rec){
	
	int i = 0, n_fields = struct_rec->n_fields;
	tha_field_info_t *fields = struct_rec->fields;

	for(; i < n_fields; i++){
		DATA_TYPE_t fdtype = fields[i].dtype;
		int fsize          = fields[i].size;
		int foffset        = fields[i].offset;

		switch(fdtype){
			case OBJ_PTR:
			case VOID_PTR:
				{
					int nested_ptr_value = 0;
					memcpy(&nested_ptr_value, (char *)(b->b) + b->next, sizeof(int));
					if(nested_ptr_value == NOT_THA_ENABLED_POINTER){	
						serialize_buffer_skip(b, MAX_STRUCTURE_NAME_SIZE);
						break;
					}

					if(nested_ptr_value == 0){
                                                memset(obj_ptr + foffset, 0, fsize);
                                                serialize_buffer_skip(b, MAX_STRUCTURE_NAME_SIZE);
                                                break;
                                        }

					char obj_id[MAX_STRUCTURE_NAME_SIZE];
					de_serialize_string(obj_id, b, MAX_STRUCTURE_NAME_SIZE);
					obj_id[MAX_STRUCTURE_NAME_SIZE -1] = '\0';

					tha_object_db_rec_t *obj_rec =
                                                        tha_object_db_lookup_by_objid(handle, obj_id);

                                        if(!obj_rec){
                                                printf("%s() : WARNING : Could not find THA suported object to which %s->%s points\n",
                                                                __FUNCTION__, struct_rec->struct_name, fields[i].fname);
                                                memset(obj_ptr + foffset, 0, fsize);
						tha_sync_dependency_graph_pending_linkage_list_t *link_data = 
							calloc(1, sizeof(tha_sync_dependency_graph_pending_linkage_list_t));
						link_data->from_ptr = (void **)(obj_ptr + foffset);
						memcpy(link_data->target_obj_id, obj_id, MAX_STRUCTURE_NAME_SIZE);
						singly_ll_add_node_by_val(orphan_pointer_list, link_data);
                                                break;
                                        }

                                        memcpy(obj_ptr + foffset, &obj_rec->ptr, sizeof(void *));
				}
				break;
			case OBJ_STRUCT:
					de_serialize_internal_object(handle, b, obj_ptr + foffset, fields[i].nested_struct_ptr);
				break;
			default:
				de_serialize_string(obj_ptr + foffset, b, fsize);
				break;
		}
		// internal objects do not have hash codes. ToDo
		//hash_array[changed_fld_index + 1] = hash_code(obj_ptr + foffset, fsize);
	}
}

/* obj_ptr = Current object pointer, 
b - serialisex buffer , poitns to detaFields 
struct_rec - structure record for Current object pointer*/

int
de_serialize_object(tha_handle_t *handle, char *obj_ptr, ser_buff_t *b, 
		     tha_struct_db_rec_t *struct_rec, unsigned int *hash_array){

	int rc = SUCCESS, 
	    i = 0, 
	    n_fields_changed = 0, 
	    changed_fld_index = 0, 
	    re_compute_hash_code_flag = 0;	

	tha_field_info_t *fields = struct_rec->fields;
	de_serialize_string((char *)&n_fields_changed,  b, sizeof(int));
	
	for(; i < n_fields_changed; i++){
		de_serialize_string((char *)&changed_fld_index,  b, sizeof(int));
		DATA_TYPE_t fdtype = fields[changed_fld_index].dtype;
		int fsize          = fields[changed_fld_index].size;
		int foffset 	   = fields[changed_fld_index].offset;

		switch(fdtype){
			case OBJ_STRUCT:
				{
					re_compute_hash_code_flag = 1;
					de_serialize_internal_object(handle, b, obj_ptr + foffset, fields[changed_fld_index].nested_struct_ptr);
					if(BULK_SYNC_IN_PROGRESS){
						tha_save_hashcode_recompute_data_t *hash_code_recompute_entity = 
							calloc(1, sizeof(tha_save_hashcode_recompute_data_t));
						hash_code_recompute_entity->hash_code = &hash_array[changed_fld_index + 1];
						hash_code_recompute_entity->start_ptr = (char *)(obj_ptr + foffset);
						hash_code_recompute_entity->len       = fsize;
						singly_ll_add_node_by_val(hash_code_recompute_list, hash_code_recompute_entity);
					}
				}
				break;
			case OBJ_PTR:
			case VOID_PTR:
				{
				/* if the value in this field in serialized buffer is NOT_THA_ENABLED_POINTER
				   then no OP*/
					void *nested_ptr_value = NULL;
					memcpy(&nested_ptr_value, (char *)(b->b) + b->next, fsize);
					if((int)nested_ptr_value == NOT_THA_ENABLED_POINTER){
						serialize_buffer_skip(b, MAX_STRUCTURE_NAME_SIZE);
						break; // it really doesnt matter to compute the hash code in this case
					}
					
					/* Means, Nested pointer value has been updated to NULL*/
					if(nested_ptr_value == NULL){
						memset(obj_ptr + foffset, 0, fsize);
						serialize_buffer_skip(b, MAX_STRUCTURE_NAME_SIZE);
						break;
					}
		
					/*Means, Nested pointer value is the obj_id to which it points*/

					char obj_id[MAX_STRUCTURE_NAME_SIZE];
					de_serialize_string(obj_id, b, MAX_STRUCTURE_NAME_SIZE);
					obj_id[MAX_STRUCTURE_NAME_SIZE -1] = '\0';
					
					tha_object_db_rec_t *obj_rec = 
							tha_object_db_lookup_by_objid(handle, obj_id);

					if(!obj_rec){
						re_compute_hash_code_flag = 1;
						printf("%s() : WARNING : Could not find THA suported object to which %s->%s points\n", 
								__FUNCTION__, struct_rec->struct_name, fields[changed_fld_index].fname);
						memset(obj_ptr + foffset, 0, fsize);
						tha_sync_dependency_graph_pending_linkage_list_t *link_data = 
							calloc(1, sizeof(tha_sync_dependency_graph_pending_linkage_list_t));
						link_data->from_ptr = (void **)(obj_ptr + foffset);
						memcpy(link_data->target_obj_id, obj_id, MAX_STRUCTURE_NAME_SIZE);
						singly_ll_add_node_by_val(orphan_pointer_list, link_data);
						if(BULK_SYNC_IN_PROGRESS){
							tha_save_hashcode_recompute_data_t *hash_code_recompute_entity = 
								calloc(1, sizeof(tha_save_hashcode_recompute_data_t));
							hash_code_recompute_entity->hash_code = &hash_array[changed_fld_index + 1];
							hash_code_recompute_entity->start_ptr = (char *)(obj_ptr + foffset);
							hash_code_recompute_entity->len       = fsize;
							singly_ll_add_node_by_val(hash_code_recompute_list, hash_code_recompute_entity);
						}
						break;
					}

					memcpy(obj_ptr + foffset, &obj_rec->ptr, sizeof(void *));
				}
				break;
			default:	
				de_serialize_string(obj_ptr + foffset, b, fsize);
				break;
		}
		/* Recompute hashcode of the fiel*/
		hash_array[changed_fld_index + 1] = hash_code(obj_ptr + foffset, fsize);	
	}

	/*If one of the fields of the object needs hash_code recomputation, the entire object needs hashcode
	  recomputation*/
	if(re_compute_hash_code_flag && BULK_SYNC_IN_PROGRESS){
		tha_save_hashcode_recompute_data_t *hash_code_recompute_entity = 
			calloc(1, sizeof(tha_save_hashcode_recompute_data_t));
		hash_code_recompute_entity->hash_code = &hash_array[0];
		hash_code_recompute_entity->start_ptr = (char *)(obj_ptr);
		hash_code_recompute_entity->len       = struct_rec->ds_size;;
		singly_ll_add_node_by_val(hash_code_recompute_list, hash_code_recompute_entity);
	}
	return rc;
}



/* Tis fn is to update the object on standby. The object could be static or pre-existing dynamic*/
/* Note : b poits to pay load*/
static int 
tha_update_object(tha_handle_t *handle, serialize_hdr_t *hdr, ser_buff_t *b){
	printf("%s(): units = %d\n", __FUNCTION__, hdr->units);	
	unsigned int *hash_array = NULL; int arr_size = 0;
	char *obj_id = get_serialize_buffer_current_ptr(b);
	tha_object_db_rec_t *obj_rec = tha_object_db_lookup_by_objid(handle, obj_id);
	
	if(!obj_rec){
                printf("%s() : Could not find object record in obj_db for obj_id = %s\n", __FUNCTION__, obj_id);
                return FAILURE;
        }

        tha_struct_db_rec_t *struct_rec = obj_rec->struct_rec;
        if(!struct_rec){
                printf("%s() : Could not find struct record in struct_db for obj_id = %s\n", __FUNCTION__, obj_id);
                return FAILURE;
        }

        serialize_buffer_skip(b, MAX_STRUCTURE_NAME_SIZE);
        serialize_buffer_skip(b, MAX_STRUCTURE_NAME_SIZE);

	int units = hdr->units, obj_index = 0;
	for(; obj_index < units; obj_index++){
		char *current_object_ptr = GET_OBJECT_PTR_AT_INDEX(obj_rec, obj_index);
		hash_array = get_struct_hash_array_fn(obj_rec, &arr_size, obj_index);
		de_serialize_object(handle, current_object_ptr, b, struct_rec, hash_array);
		hash_array[0] = hash_code(current_object_ptr, struct_rec->ds_size);
		hash_array[struct_rec->n_fields + 1] = (unsigned int)obj_rec;
	}
	return SUCCESS;
}


static int
tha_delete_object(tha_handle_t *handle, serialize_hdr_t *hdr, ser_buff_t *b){
	 char *obj_id = get_serialize_buffer_current_ptr(b);
	 tha_object_db_rec_t *object_rec = NULL;
	
	 object_rec = 	tha_object_db_lookup_by_objid(handle, obj_id);
	 if(!object_rec){
		 printf("%s(): Error : could not find the object to delete with obj id = %s\n", 
			__FUNCTION__, obj_id);
		 return FAILURE;
	 }

	FREE_OBJECT_HASH_ARRAY(object_rec);

	free(object_rec->ptr);

	if(tha_remove_object_db_rec(handle, object_rec) == FAILURE){
		printf("%s(): Error : Could not find the object_Rec in objectdb for obj_id = %s\n", __FUNCTION__, obj_id);
		return FAILURE;
	}
	else{
		printf("%s(): object_rec with obj_id = %s successfully removed\n",__FUNCTION__,obj_id);
		return SUCCESS;
	}
}


static int
tha_reset_object(tha_handle_t *handle, serialize_hdr_t *hdr, ser_buff_t *b){

	return SUCCESS;
}


int
mark_changed_objects(tha_handle_t *handle){
	tha_object_db_t *object_db = handle->object_db;
	if(!object_db){
		printf("%s(): Object Db not initialised\n", __FUNCTION__);
		return 0;
	};

	unsigned int *hash_array = NULL;
	int i = 0, arr_size = 0,
	    computed_hash_code = 0, stored_hash_code = 0,
	    count_marked_objects = 0;
	
	tha_object_db_rec_t *obj_rec = NULL;
	obj_rec = object_db->head;


	for(i = 0; i < object_db->count; i++){
		int units = obj_rec->units, obj_index = 0;


		for(obj_index = 0; obj_index < units; obj_index++){	
			char *current_obj_ptr = NULL;
			current_obj_ptr = GET_OBJECT_PTR_AT_INDEX(obj_rec, obj_index);
			hash_array = get_struct_hash_array_fn(obj_rec, &arr_size, obj_index);
			if(!hash_array){
				printf("%s(): Error : Hash Array not found of the object, obj_id = %s. This will not be synced\n", __FUNCTION__, obj_rec->obj_id);
				continue;
			}

			stored_hash_code = hash_array[0]; // hash code of entire structure
			computed_hash_code = hash_code(current_obj_ptr, obj_rec->struct_rec->ds_size);
			if(stored_hash_code != computed_hash_code){
				obj_rec->dirty_bit = 1;
				count_marked_objects++;
				printf("%s(): obj_id  = %s, array_index = %d, marked for sync. old HC = %d, new HC = %d\n",
						__FUNCTION__, obj_rec->obj_id, obj_index, stored_hash_code, computed_hash_code);
				break;
			}	
		}
		obj_rec = obj_rec->next;
	}
	return count_marked_objects;
}

void
clean_dirty_bit(tha_handle_t *handle){
	tha_object_db_t *object_db = handle->object_db;
	if(!object_db){
		printf("%s(): Object Db not initialised\n", __FUNCTION__);
		return;
	};
	
	int i = 0;
	tha_object_db_rec_t *obj_rec = NULL;
	obj_rec = object_db->head;
	for(i = 0; i < object_db->count; i++){
		obj_rec->dirty_bit = 0;
		obj_rec = obj_rec->next;
	}
}


int
tha_sync_object(int opcode, tha_handle_t *handle, 
		tha_object_db_rec_t *obj_rec, 
		int ALL_FIELDS_FLAG){

	ser_buff_t *b = NULL;
	
	if(I_AM_ACTIVE_CP == 0)
		return SUCCESS;

	if(ALL_FIELDS_FLAG == SYNC_CHANGED_FIELDS)
		b = serialize_object(handle, obj_rec);
	else
		b = serialize_object_all_fields(handle, obj_rec);

	if(!b) return FAILURE;
	
	int payload_serialized_size = get_serialize_buffer_size(b);
	serialize_hdr_t hdr;
	memset(&hdr, 0, sizeof(serialize_hdr_t));
	/* we will create object on standby if we could not find the object with obj_id
	hence, CREATE_OBJ op code will not be checked on standby*/
	hdr.op_code 	 = opcode;
	hdr.payload_size = payload_serialized_size;
	hdr.units	 = obj_rec->units;

	ser_buff_t *final_ser_buffer = NULL;
	init_serialized_buffer_of_defined_size(&final_ser_buffer, 
				SERIALIZED_HDR_SIZE + payload_serialized_size); 

	// copying the header in final_ser_buffer
	serialize_int32(final_ser_buffer, hdr.op_code);
	serialize_int32(final_ser_buffer, hdr.payload_size);
	serialize_int32(final_ser_buffer, hdr.units);

	// copy the payload now from old serialized buffer to final buffer
	serialize_string(final_ser_buffer, b->b, payload_serialized_size);
	free_serialize_buffer(b);
	ha_incremental_sync(handle, final_ser_buffer->b, SERIALIZED_HDR_SIZE + payload_serialized_size);	

	free_serialize_buffer(final_ser_buffer);	
	return SUCCESS;
}


static int
tha_create_object(tha_handle_t *handle, serialize_hdr_t *hdr, ser_buff_t *b){

	char obj_id[MAX_STRUCTURE_NAME_SIZE];
	de_serialize_string(obj_id, b, MAX_STRUCTURE_NAME_SIZE);

	char *struct_name = get_serialize_buffer_current_ptr(b);

	tha_struct_db_rec_t *struct_rec = NULL;
	struct_rec = tha_struct_db_look_up(handle, struct_name);
	if(!struct_rec){
		printf("%s() : Error : Structure rec not found for object id = %s\n",
				__FUNCTION__, obj_id);
		return FAILURE;;
	}

	int rc = SUCCESS;
	int obj_type = DYN_OBJ, units = hdr->units;
	char *obj_ptr = calloc(units, struct_rec->ds_size);
	rc = tha_add_object(handle, obj_ptr, struct_name, obj_id, units, obj_type);
	if(rc == FAILURE){
		printf("%s() : Object with objid = %s failed to be added\n", __FUNCTION__, obj_id);	
		free(obj_ptr);
	}
	return rc;
}

static int
tha_recv_bulk_sync(tha_handle_t *handle, 
	      serialize_hdr_t *hdr, 
	      ser_buff_t *b){
	
	int rc = FAILURE;
	BULK_SYNC_IN_PROGRESS = 1;	
	char *obj_id = get_serialize_buffer_current_ptr(b);
	tha_object_db_rec_t *object_rec = tha_object_db_lookup_by_objid(handle, obj_id);
	serialize_hdr_t temp_hdr;
	memcpy(&temp_hdr, hdr, sizeof(serialize_hdr_t));
	if(!object_rec){// means object is dynamic
		temp_hdr.op_code = CREATE_OBJ;
		rc = tha_create_object(handle, &temp_hdr, b);
		if(rc == FAILURE) return rc;
		reset_serialize_buffer(b);
		serialize_buffer_skip(b, SERIALIZED_HDR_SIZE);
		temp_hdr.op_code = UPDATE_OBJ;
		rc = tha_update_object(handle, &temp_hdr, b);
		return rc;
	}
	temp_hdr.op_code = UPDATE_OBJ;
	rc = tha_update_object(handle, &temp_hdr, b);
	BULK_SYNC_IN_PROGRESS = 0;
	return rc;
}

/* Serialize & De-Serialize functions*/


int
de_serialize_buffer(tha_handle_t *handle, char *msg , int size){
	int rc = SUCCESS;
	if(!msg || !size){
		printf("%s() : Error : Invalid msg recieved\n", __FUNCTION__);
		return FAILURE;
	}

	ser_buff_t *b = NULL;
	init_serialized_buffer_of_defined_size(&b, size);
	// this will not update b->next or b->size
	memcpy(b->b, msg, size);
	
	serialize_hdr_t hdr;
	de_serialize_string((char *)&hdr.op_code, 	b, sizeof(int));
	de_serialize_string((char *)&hdr.payload_size,  b, sizeof(int));
	de_serialize_string((char *)&hdr.units, 	b, sizeof(int));

	printf("%s() : Header contents : opcode = %d, payload size = %d, units = %d\n", 
			__FUNCTION__, hdr.op_code, hdr.payload_size, hdr.units);

	switch(hdr.op_code){
		case CREATE_OBJ:
			rc = tha_create_object(handle, &hdr, b);
			break;
		case UPDATE_OBJ:
			rc = tha_update_object(handle, &hdr, b);
			break;
		case DELETE_OBJ:
			rc = tha_delete_object(handle, &hdr, b);
			break;
		case RESET_OBJ:
			rc = tha_reset_object(handle, &hdr, b);
			break;
		case CLEAN_STATE:
			tha_clean_application_state(handle);
			break;
		case BULK_SYNC:
			rc = tha_recv_bulk_sync(handle, &hdr, b);
			break;
		case ORPHAN_POINTER_COMMIT:
			tha_process_commit_orphan_pointers_list(handle);
			break;
		case COMMIT:
			printf("%s(): State is Synchronized\n", __FUNCTION__);
			break;
		default:
			printf("%s(): Error : Unknown msg type recieved\n", __FUNCTION__);
			break;
	}
	
	if(rc == FAILURE)
		printf("%s(): Error : Operation opcode = %d Failed\n", __FUNCTION__, hdr.op_code);
	
	free_serialize_buffer(b);
	return rc;	
}

ser_buff_t *
serialize_object(tha_handle_t *handle, tha_object_db_rec_t *obj_rec){
	tha_struct_db_rec_t *struct_rec = NULL;
	unsigned int *hash_array = NULL;
	     int arr_size = 0, i = 0, obj_index=0,
	     computed_hash_code = 0,
	     n_fields_changed = 0,
	     n_fields_offset_in_ser_buffer = 0,
	     units = 0;
	
	tha_field_info_t *field_info = NULL;
	
	struct_rec = obj_rec->struct_rec;
	if(!struct_rec){
		printf("%s(): Error : Attempt to ha_calloc the non-THA supported object\n", __FUNCTION__);
		return NULL;
	}
	
	ser_buff_t *b = NULL;
	init_serialized_buffer(&b);

	units = obj_rec->units;
	// Fill object id
	serialize_string(b, obj_rec->obj_id, MAX_STRUCTURE_NAME_SIZE);
	// fill structure name
	serialize_string(b, struct_rec->struct_name, MAX_STRUCTURE_NAME_SIZE);

	for( obj_index = 0; obj_index < units; obj_index++){
		
		n_fields_changed = 0;
		
		hash_array = get_struct_hash_array_fn(obj_rec, &arr_size, obj_index);

		char *current_obj_ptr = NULL;

		current_obj_ptr = GET_OBJECT_PTR_AT_INDEX(obj_rec, obj_index);
		/* update hash code of the structure*/
		hash_array[0] = hash_code(current_obj_ptr, struct_rec->ds_size);
		// store the pointer location in the buffer where n_fields will be filled later
		n_fields_offset_in_ser_buffer = get_serialize_buffer_current_ptr_offset(b);
		// skip the buffer by int32 size
		serialize_int32(b,0);

		for(i = 1; i < arr_size -1; i++){
			field_info = &(struct_rec->fields[i -1]);
			computed_hash_code = hash_code(current_obj_ptr + field_info->offset , 
							field_info->size);
			if(computed_hash_code == hash_array[i]){
				continue;
			}

			n_fields_changed++;
			printf("obj_id = %s, field = %s, changed\n", obj_rec->obj_id, field_info->fname);

			// update the hashcode
			hash_array[i] = computed_hash_code;
			// Fill the changed field index
			serialize_int32(b, i-1);
			// fill the changed field value

			switch(field_info->dtype){
				case OBJ_PTR:
					{
						void *nested_ptr = NULL;
						memcpy(&nested_ptr, current_obj_ptr + field_info->offset, field_info->size);
						if(nested_ptr == NULL){
							serialize_buffer_skip(b, MAX_STRUCTURE_NAME_SIZE);
							break;	
						}
						tha_object_db_rec_t *nested_object =  tha_object_db_lookup_by_Addr(handle, nested_ptr);
						if(nested_object)
							serialize_string(b, nested_object->obj_id, MAX_STRUCTURE_NAME_SIZE);
						else{
							serialize_int32(b, NOT_THA_ENABLED_POINTER); 
							serialize_buffer_skip(b, MAX_STRUCTURE_NAME_SIZE - sizeof(NOT_THA_ENABLED_POINTER));
						}
					}
					break;
				case VOID_PTR:
					{
						void *nested_ptr = NULL;
						memcpy(&nested_ptr, current_obj_ptr + field_info->offset, field_info->size);
						if(nested_ptr == NULL){
							serialize_buffer_skip(b, MAX_STRUCTURE_NAME_SIZE);
							break;	
						}
						tha_object_db_rec_t *nested_object = tha_object_db_lookup_by_Addr(handle, nested_ptr);
						if(nested_object)
							serialize_string(b, nested_object->obj_id, MAX_STRUCTURE_NAME_SIZE);
						else{
							serialize_int32(b, NOT_THA_ENABLED_POINTER); 
							serialize_buffer_skip(b, MAX_STRUCTURE_NAME_SIZE - sizeof(NOT_THA_ENABLED_POINTER));
						}

					}
					break;
				case OBJ_STRUCT:
					{
						serialize_internal_object(handle, b, current_obj_ptr + field_info->offset, field_info->nested_struct_ptr);
					}
					break;
				default:
					{
						serialize_string(b, current_obj_ptr + field_info->offset, field_info->size);
				
					}
				break;

			}
		}
		copy_in_serialized_buffer_by_offset(b, sizeof(n_fields_changed), (char *)&n_fields_changed, n_fields_offset_in_ser_buffer);
	}
	return b;
}


ser_buff_t *
serialize_object_all_fields(tha_handle_t *handle, tha_object_db_rec_t *obj_rec){
	tha_struct_db_rec_t *struct_rec = NULL;
	int i = 0, obj_index=0,
	     units = 0;
	
	tha_field_info_t *field_info = NULL;
	
	struct_rec = obj_rec->struct_rec;
	if(!struct_rec){
		printf("%s(): Error : Attempt to ha_calloc the non-THA supported object\n", __FUNCTION__);
		return NULL;
	}
	
	ser_buff_t *b = NULL;
	init_serialized_buffer(&b);

	units = obj_rec->units;
	// Fill object id
	serialize_string(b, obj_rec->obj_id, MAX_STRUCTURE_NAME_SIZE);
	// fill structure name
	serialize_string(b, struct_rec->struct_name, MAX_STRUCTURE_NAME_SIZE);

	for( obj_index = 0; obj_index < units; obj_index++){
		
		char *current_obj_ptr = NULL;

		current_obj_ptr = GET_OBJECT_PTR_AT_INDEX(obj_rec, obj_index);
		serialize_int32(b, struct_rec->n_fields);

		for(i = 1; i <= struct_rec->n_fields; i++){
			field_info = &(struct_rec->fields[i -1]);
			// Fill the changed field index
			serialize_int32(b, i-1);
			// fill the changed field value

			switch(field_info->dtype){
				case OBJ_PTR:
					{
						void *nested_ptr = NULL;
						memcpy(&nested_ptr, current_obj_ptr + field_info->offset, field_info->size);
						if(nested_ptr == NULL){
							serialize_buffer_skip(b, MAX_STRUCTURE_NAME_SIZE);
							break;	
						}
						tha_object_db_rec_t *nested_object =  tha_object_db_lookup_by_Addr(handle, nested_ptr);
						if(nested_object)
							serialize_string(b, nested_object->obj_id, MAX_STRUCTURE_NAME_SIZE);
						else{
							serialize_int32(b, NOT_THA_ENABLED_POINTER); 
							serialize_buffer_skip(b, MAX_STRUCTURE_NAME_SIZE - sizeof(NOT_THA_ENABLED_POINTER));
						}
					}
					break;
				case VOID_PTR:
					{
						void *nested_ptr = NULL;
						memcpy(&nested_ptr, current_obj_ptr + field_info->offset, field_info->size);
						if(nested_ptr == NULL){
							serialize_buffer_skip(b, MAX_STRUCTURE_NAME_SIZE);
							break;	
						}
						tha_object_db_rec_t *nested_object = tha_object_db_lookup_by_Addr(handle, nested_ptr);
						if(nested_object)
							serialize_string(b, nested_object->obj_id, MAX_STRUCTURE_NAME_SIZE);
						else{
							serialize_int32(b, NOT_THA_ENABLED_POINTER); 
							serialize_buffer_skip(b, MAX_STRUCTURE_NAME_SIZE - sizeof(NOT_THA_ENABLED_POINTER));
						}

					}
					break;
				case OBJ_STRUCT:
					{
						serialize_internal_object(handle, b, current_obj_ptr + field_info->offset, field_info->nested_struct_ptr);
					}
					break;
				default:
					{
						serialize_string(b, current_obj_ptr + field_info->offset, field_info->size);
				
					}
				break;

			}
		}
	}
	return b;
}



void
serialize_internal_object(tha_handle_t *handle, ser_buff_t *b,
                          char *obj_ptr,
                          tha_struct_db_rec_t *struct_rec){

/* Note : Internal objects do not have any hash code arrays. So, internal objects have to
be synced fully irrespective of which fields have changed. We will improve on this later.*/

        int n_fields = 0, i = 0;
        tha_field_info_t *fields = NULL;

        fields = struct_rec->fields;
        n_fields = struct_rec->n_fields;

        for(i = 0; i < n_fields; i++){
                switch((&fields[i])->dtype){
                        case VOID_PTR:
                        {
                                /* This pointer could be the pointer to THA registered object Or Non-Tha registered Object*/
                                  void *nested_ptr = NULL;
                                  memcpy(&nested_ptr, obj_ptr , (&fields[i])->size);
				  if(nested_ptr == NULL){
					serialize_buffer_skip(b, MAX_STRUCTURE_NAME_SIZE);
					break;
			          }
				  /* If it is a VOID_PTR, we have no choice other than to look into object db*/
                                  tha_object_db_rec_t *nested_object = tha_object_db_lookup_by_Addr(handle, nested_ptr);
                                  if(nested_object)
                                          serialize_string(b, nested_object->obj_id, MAX_STRUCTURE_NAME_SIZE);
                                  else{
                                  	serialize_int32(b, NOT_THA_ENABLED_POINTER);
					serialize_buffer_skip(b, MAX_STRUCTURE_NAME_SIZE - sizeof(NOT_THA_ENABLED_POINTER));
				  }
                                  obj_ptr += (&fields[i])->size;
                        }
			break;
			case OBJ_PTR:
                        {
                                /* This pointer could be the pointer to THA registered object Or Non-Tha registered Object*/
                                  void *nested_ptr = NULL;
                                  memcpy(&nested_ptr, obj_ptr , (&fields[i])->size);
				  if(nested_ptr == NULL){
					serialize_buffer_skip(b, MAX_STRUCTURE_NAME_SIZE);
					break;
			          }
                                  tha_object_db_rec_t *nested_object = tha_object_db_lookup_by_Addr(handle, nested_ptr);
                                  if(nested_object)
                                          serialize_string(b, nested_object->obj_id, MAX_STRUCTURE_NAME_SIZE);
                                  else{
                                  	serialize_int32(b, NOT_THA_ENABLED_POINTER);
					serialize_buffer_skip(b, MAX_STRUCTURE_NAME_SIZE - sizeof(NOT_THA_ENABLED_POINTER));
				  }
                                  obj_ptr += (&fields[i])->size;

                        }
                        break;
                        case OBJ_STRUCT:
                        {
                                serialize_internal_object(handle ,b, obj_ptr, (&fields[i])->nested_struct_ptr);
                                obj_ptr += (&fields[i])->size;;
                        }
                        break;
                        default:
                        {
                                serialize_string(b, obj_ptr, (&fields[i])->size);
                                obj_ptr += (&fields[i])->size;
                        }
                      break;
                } // switch ends
        }
}

int
tha_sync_object_db(tha_handle_t *handle){
        int i = 0, rc = SUCCESS, count_dirty_objects = 0;
        count_dirty_objects = mark_changed_objects(handle);

        if(!count_dirty_objects){
		printf("%s() : No. of Objects to sync = %d\n", __FUNCTION__, count_dirty_objects);
                return SUCCESS;
        }

	printf("%s() : No. of Objects to sync = %d\n", __FUNCTION__, count_dirty_objects);
        tha_object_db_t *object_db = handle->object_db;

        tha_object_db_rec_t *object_rec = object_db->head;

        for(; i < object_db->count; i++){
		if(!object_rec->dirty_bit){
			object_rec = object_rec->next;
			continue;
		}
                rc = tha_sync_object(UPDATE_OBJ, handle, object_rec, SYNC_CHANGED_FIELDS);
                if(rc == FAILURE){
                        printf("%s() : Error : Object %s could not be synced. skippinng ...\n",
                                __FUNCTION__, object_rec->obj_id);
                        object_rec = object_rec->next;
                        continue;
                }
                object_rec = object_rec->next;
        }
        clean_dirty_bit(handle);
        return rc;
}

int
tha_sync_delete_one_object(tha_handle_t *handle, char *obj_id){
	serialize_hdr_t hdr;
	memset(&hdr, 0, sizeof(serialize_hdr_t));	
	
	hdr.op_code 	 = DELETE_OBJ;
	hdr.payload_size = MAX_STRUCTURE_NAME_SIZE;
	hdr.units	 = 0; // Not required

	ser_buff_t *b = NULL;

	init_serialized_buffer_of_defined_size(&b, SERIALIZED_HDR_SIZE + hdr.payload_size);

	// copying the header in final_ser_buffer
	serialize_int32(b, hdr.op_code);
        serialize_int32(b, hdr.payload_size);
        serialize_int32(b, hdr.units);
	
	// copy the payload now 
	serialize_string(b, obj_id, MAX_STRUCTURE_NAME_SIZE); 
	tha_sync_msg_to_standby((char *)(b->b), get_serialize_buffer_size(b));
	free_serialize_buffer(b);	
	return SUCCESS;
} 

void
send_tha_clean_application_state_order(tha_handle_t *handle){
	serialize_hdr_t hdr;
	memset(&hdr, 0, sizeof(serialize_hdr_t));
	hdr.op_code      = CLEAN_STATE;
	hdr.payload_size = 0;
	hdr.units        = 0; 
	
	ser_buff_t *b = NULL;

	init_serialized_buffer_of_defined_size(&b, SERIALIZED_HDR_SIZE + hdr.payload_size);
	
	serialize_int32(b, hdr.op_code);
        serialize_int32(b, hdr.payload_size);
        serialize_int32(b, hdr.units);
	tha_sync_msg_to_standby((char *)(b->b), get_serialize_buffer_size(b));
	free_serialize_buffer(b);
}

/* This is basically DFS algorithm. If object A has a pointer to object B
(A -----> B), then B should be sync before A.*/

static void 
add_to_dependency_graph(tha_handle_t *handle, 
			tha_object_db_rec_t *object_rec){

	object_rec->dirty_bit = 1;
	int i = 0, obj_index = 0;

	tha_field_info_t *field_info = NULL;
	for(; obj_index < object_rec->units; obj_index++){
		for(i = 0; i < object_rec->struct_rec->n_fields; i++){
			field_info = tha_get_struct_field_info_by_index(object_rec, i);
			switch(field_info->dtype){
				case OBJ_PTR:
					{
						void *nested_ptr = NULL;
						memcpy(&nested_ptr, GET_OBJECT_PTR_AT_INDEX(object_rec, obj_index) + field_info->offset, field_info->size);
						if(nested_ptr == NULL || (int)nested_ptr == NOT_THA_ENABLED_POINTER)
							continue;

						tha_object_db_rec_t *nested_object = tha_object_db_lookup_by_Addr(handle, nested_ptr); 
						if(!nested_object->dirty_bit)
							add_to_dependency_graph(handle, nested_object);
					}		
					break;
				case VOID_PTR:
					{
						void *nested_ptr = NULL;
						memcpy(&nested_ptr, GET_OBJECT_PTR_AT_INDEX(object_rec, obj_index) + field_info->offset, field_info->size);
						if(nested_ptr == NULL || (int)nested_ptr == NOT_THA_ENABLED_POINTER)
							continue;

						tha_object_db_rec_t *nested_object = tha_object_db_lookup_by_Addr(handle, nested_ptr);
						if(!nested_object)
							continue;
						if(!nested_object->dirty_bit)
							add_to_dependency_graph(handle, nested_object);
					}
					break;
				default:
					break;
			} // switch 
		}
	}
	printf("%s(): Syncing object obj_id = %s, structure = %s\n", __FUNCTION__, object_rec->obj_id, object_rec->struct_rec->struct_name);
	tha_sync_object(BULK_SYNC, handle, object_rec, SYNC_ALL_FIELDS);
}

static void
format_time(char *output){
	time_t rawtime;
	struct tm * timeinfo;

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );

	sprintf(output, "%d%d%d%d%d%d",	
			timeinfo->tm_mday, timeinfo->tm_mon + 1, 
			timeinfo->tm_year + 1900, timeinfo->tm_hour, 
			timeinfo->tm_min, timeinfo->tm_sec);
}


int
tha_bulk_sync(tha_handle_t *handle){
	
	if(I_AM_ACTIVE_CP == 0)
		return SUCCESS;

  	if(enable_checkpoint){
		char fname[64], time_stamp[32];
		strcpy(fname, "CHP_");
		format_time(time_stamp);
		strcat(fname, time_stamp);
		printf("Application state  checkointed to file %s\n", fname);
		checkpoint_file = fopen(fname, "wb");		
	}	
	
	
	tha_object_db_t *object_db = handle->object_db;
	tha_object_db_rec_t *object_rec = object_db->head;

	while(object_rec){
		if(object_rec->dirty_bit){
			object_rec = object_rec->next;
			continue;
		}

		/* For Garbage collection, lets start wuth Static objects only*/
		#if 1
		if(object_rec->object_type == DYN_OBJ){
			object_rec = object_rec->next;
			continue;
		}
		#endif
		add_to_dependency_graph(handle, object_rec);
		object_rec = object_rec->next;
	}

	/* Dump count of Unreachable objects*/
	int unreachable_objects = 0;
	object_rec = object_db->head;

	while(object_rec){
		if(!object_rec->dirty_bit)
			unreachable_objects++;
		object_rec = object_rec->next;
	}
	printf("Warning : No. Of Leaked Objects = %d, Dumping...\n", unreachable_objects);

#if 1
	object_rec = object_db->head;
	while(object_rec){
		if(!object_rec->dirty_bit)
			dump_tha_object(object_rec);
		object_rec = object_rec->next;
	}
#endif	
	clean_dirty_bit(handle);
	tha_send_orphan_pointers_commit();
	tha_send_commit();
	if(enable_checkpoint == CHECKPOINT_ENABLE && checkpoint_file){
		fclose(checkpoint_file);
		checkpoint_file = NULL;
	}
	return SUCCESS;
}


void
load_checkpoint (char *CHP_FILE_NAME){
	printf("Loading checkpoint from file %s\n", CHP_FILE_NAME);	
	/* This process should be done when checkpoint is  disabled*/
	if( enable_checkpoint == CHECKPOINT_ENABLE ){
		printf("Checkpoint load aborted, disable the  checkpoint first\n");
		return;
	}
	/*Active should clean Standby state now*/
	send_tha_clean_application_state_order(handle);
	/* Active should clean its own state now*/
	tha_clean_application_state(handle);
	/* Load checkpoint file now*/

	checkpoint_file = fopen(CHP_FILE_NAME, "rb");
	if (checkpoint_file == NULL){
		printf ("%s(): Error in open() checkpoint file : %s\n", __FUNCTION__, CHP_FILE_NAME);
		printf("errno = %d\n", errno);
		return;
	}

	int i = 1;
	serialize_hdr_t msg_hdr;

	while(1){
		if(fread(checkpoint_load_buffer->b, SERIALIZED_HDR_SIZE, 1, checkpoint_file)){
#if 1
			de_serialize_string((char *)&msg_hdr.op_code, checkpoint_load_buffer, sizeof(msg_hdr.op_code));
			de_serialize_string((char *)&msg_hdr.payload_size, checkpoint_load_buffer, sizeof(msg_hdr.payload_size));
			de_serialize_string((char *)&msg_hdr.units, checkpoint_load_buffer, sizeof(msg_hdr.units));
			printf("%s() : %d : header contents : op_code = %d, payload_size = %d, units = %d\n", 
					__FUNCTION__, i++, msg_hdr.op_code, msg_hdr.payload_size, msg_hdr.units);
#endif
			fread(checkpoint_load_buffer->b + SERIALIZED_HDR_SIZE,  msg_hdr.payload_size, 1, checkpoint_file);
			/* process the object read on Active*/
			de_serialize_buffer(handle, checkpoint_load_buffer->b, SERIALIZED_HDR_SIZE + msg_hdr.payload_size);
			/* Sync the object read to Standby*/
			ha_incremental_sync(handle, checkpoint_load_buffer->b, SERIALIZED_HDR_SIZE + msg_hdr.payload_size);	
			/* reset buffer to process the next object*/
			reset_serialize_buffer(checkpoint_load_buffer);	
			continue;
		}
		break;
	}
	
	fclose(checkpoint_file);
	checkpoint_file = NULL;
}

