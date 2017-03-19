#include "tha_db.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "Utils.h"
#include "tha_sync.h"
#include "threadApi.h"
#include "assert.h"
#include "css.h"
#include "serialize.h"
#include "DS/LinkedList/LinkedListApi.h"

int I_AM_ACTIVE_CP = 1;
tha_handle_t *handle = NULL;
ser_buff_t *temp_store = NULL; // avoid unneccesssary lookups

extern void init_tha_listening_thread(tha_handle_t *handle);
extern int init_tha_sending_socket(tha_handle_t *handle);
extern int tha_sync_delete_one_object(tha_handle_t *handle, char *obj_id);
extern ll_t* orphan_pointer_list;
extern ll_t* hash_code_recompute_list;

char *DATA_TYPE[] = {"UINT8" , 		
		     "UINT32", 
		     "INT32" , 
	             "CHAR"  , 
		     "VOID_PTR", 
		     "OBJ_PTR", 
		     "FLOAT"  , 
		     "DOUBLE" ,
		     "OBJ_STRUCT"};

static tha_field_info_t tha_int_t_fields[] = {
	{"primitive", sizeof(int), 0, INT32, "NA", NULL}
	//HA_FIELD_INFO(tha_int_t, val, INT32, 0)
};


static tha_field_info_t tha_uint_t_fields[] = {
	{"primitive", sizeof(unsigned int), 0, UINT32, "NA", NULL}
	//HA_FIELD_INFO(tha_uint_t, val, UINT32, 0)
};


static tha_field_info_t tha_float_t_fields[] = {
	{"primitive", sizeof(float), 0, FLOAT, "NA", NULL}
	//HA_FIELD_INFO(tha_float_t, val, FLOAT, 0)
};


static tha_field_info_t tha_double_t_fields[] = {
	{"primitive", sizeof(double), 0, DOUBLE, "NA", NULL}
	//HA_FIELD_INFO(tha_double_t, val, DOUBLE, 0)
};


static tha_field_info_t tha_char_t_fields[] = {
	{"primitive", sizeof(char), 0, CHAR, "NA", NULL}
	//HA_FIELD_INFO(tha_char_t, val, CHAR, 0)
};

static tha_field_info_t tha_uint8_t_fields[] = {
	{"primitive", sizeof(char), 0, UINT8, "NA", NULL}
	//HA_FIELD_INFO(tha_uint8_t, val, UINT8, 0)
};

static tha_field_info_t tha_void_ptr_t_fields[] = {
	{"primitive", sizeof(void *), 0, VOID_PTR, "NA", NULL}
	//HA_FIELD_INFO(tha_obj_ptr_t, ptr, VOID_PTR, 0)
};

#if 1
static tha_field_info_t tha_obj_ptr_t_fields[] = {
	{"primitive", sizeof(void *), 0, OBJ_PTR, "NA", NULL}
	//HA_FIELD_INFO(tha_obj_ptr_t, ptr, VOID_PTR, 0)
};
#endif

extern void linkedlist_tha_reg(tha_handle_t *handle);
extern void tree_tha_reg(tha_handle_t *handle);
extern void stack_tha_reg(tha_handle_t *handle);
extern void queue_tha_reg(tha_handle_t *handle);

tha_handle_t* init_tha(char *standby_ip, _fn_ptr_db fn_ptr_db){

	tha_handle_t *handle = calloc(1, sizeof(tha_handle_t));
	handle->struct_db = calloc(1, sizeof(tha_struct_db_t));
	handle->object_db = calloc(1, sizeof(tha_object_db_t));

	/* Registering HA enable primitive Data types*/
	HA_REG_STRUCT(handle, int		,  	tha_int_t_fields);
	HA_REG_STRUCT(handle, unsigned int	, 	tha_uint_t_fields);
	HA_REG_STRUCT(handle, float		, 	tha_float_t_fields);
	HA_REG_STRUCT(handle, double		, 	tha_double_t_fields);
	HA_REG_STRUCT(handle, char		, 	tha_char_t_fields);
	HA_REG_STRUCT(handle, unsigned char	, 	tha_uint8_t_fields);
	HA_REG_STRUCT(handle, VOID_PTR		,	tha_void_ptr_t_fields);// bcoz there could be variants like void*/ void * / void ** etc which could lead to failure
	
	HA_REG_STRUCT(handle, OBJ_PTR		,	tha_obj_ptr_t_fields);
	
	handle->fn_ptr_db = fn_ptr_db;
	memcpy(handle->standby_ip, standby_ip, strlen(standby_ip));

	/* Adding support for fundamental Data structures */
	linkedlist_tha_reg(handle);
	/* Will add support for below DS later. Need to add support for Arrays of pointers 
	   as a structure member*/
	//stack_tha_reg(handle);
	tree_tha_reg(handle);
	//queue_tha_reg(handle);

	if(I_AM_ACTIVE_CP == 0)
		init_tha_listening_thread(handle);
	else
		init_tha_sending_socket(handle);

	/* These are standby specific data structures*/
	if(I_AM_ACTIVE_CP == 0){
		orphan_pointer_list 	 = init_singly_ll();
		hash_code_recompute_list = init_singly_ll();	
	}

	return handle;
}

int
ha_reg_struct( tha_handle_t *handle, tha_struct_db_rec_t *rec){

	if(tha_struct_db_look_up(handle, rec->struct_name) != NULL){
		printf("%s(): Duplicate Structure Registration: %s\n", __FUNCTION__, rec->struct_name);
		return FAILURE;
	}

	tha_struct_db_t *struct_db = handle->struct_db;

	if(!struct_db){
		printf("%s():THA structure db is NULL\n", __FUNCTION__);
		return FAILURE;
	}

	if(!rec) return SUCCESS;
	
	if(!struct_db->head){
		struct_db->head = rec;
		struct_db->count++;
		return SUCCESS;
	}

	tha_struct_db_rec_t *head = struct_db->head;
	rec->next = head;
	head->prev = rec;
	struct_db->head = rec;
	struct_db->count++;
	return SUCCESS;
}


int
ha_reg_object( tha_handle_t *handle, tha_object_db_rec_t *rec){
	tha_object_db_t *object_db = handle->object_db;
	if(!object_db){
		printf("%s():THA Object db is NULL\n", __FUNCTION__);
		return FAILURE;
	}

	if(!rec) return SUCCESS;
	
	if(!object_db->head){
		object_db->head = rec;
		object_db->count++;
		return SUCCESS;
	}

	tha_object_db_rec_t *head = object_db->head;
	rec->next = head;
	head->prev = rec;
	object_db->head = rec;
	object_db->count++;
	return SUCCESS;
}


int
ha_unreg_object(tha_handle_t *handle, tha_object_db_rec_t *rec){
	

	return 0;
}

int
tha_add_object(tha_handle_t *handle, void *obj_ptr, char *str_name, 
		char *obj_id, int units, int obj_type){

	tha_struct_db_t *struct_db = handle->struct_db;
	tha_object_db_t *object_db = handle->object_db;
	tha_field_info_t *fields = NULL;

	if(!object_db || !struct_db || !obj_ptr || !str_name || !units) return FAILURE;
	int rc = FAILURE, computed_hash_code = 0, arr_size = 0, 
	    i = 0, obj_index = 0;

	unsigned int *hash_array = NULL;

	tha_object_db_rec_t *obj_rec = NULL;
	tha_struct_db_rec_t *struct_rec = NULL;
	struct_rec = tha_struct_db_look_up(handle, str_name);
	if(!struct_rec){
		printf("%s(): Error : Attempt to create HA Object for unregistered structure. str_name = %s\n", __FUNCTION__, str_name);
		return FAILURE;
	}

	obj_rec = tha_object_db_lookup_by_Addr(handle, obj_ptr);
	if(obj_rec){
		printf("%s() : Error : Attempt to add the duplicate object\n", __FUNCTION__);
		return FAILURE;
	}

	obj_rec = calloc(1, sizeof(tha_object_db_rec_t));
	obj_rec->ptr = obj_ptr;
	strncpy(obj_rec->obj_name, str_name, MAX_STRUCTURE_NAME_SIZE-1);
	obj_rec->obj_name[MAX_STRUCTURE_NAME_SIZE-1] = '\0';


	if(obj_type == STATIC_OBJ)
		strncpy(obj_rec->obj_id, obj_id, MAX_STRUCTURE_NAME_SIZE-1);
	// means its a dynamic object calloc'ed on Active itself
	else if(obj_id == NULL){
		char addr[32], obj_id_temp[MAX_STRUCTURE_NAME_SIZE];
		memset(addr, 0, 32);	
		itoa((unsigned int)obj_ptr , addr, 10);
		memset(obj_id_temp, 0, MAX_STRUCTURE_NAME_SIZE);
		strcpy(obj_id_temp, "Dyn");
		strcat(obj_id_temp, addr);
		strncpy(obj_rec->obj_id, obj_id_temp, MAX_STRUCTURE_NAME_SIZE-1);
	}
	// Its a dyncamic object synced from Active and i am standby
	else 
		strncpy(obj_rec->obj_id, obj_id, MAX_STRUCTURE_NAME_SIZE-1);

	obj_rec->obj_id[MAX_STRUCTURE_NAME_SIZE-1] = '\0';
	obj_rec->units = units;
	obj_rec->object_type = obj_type;	
	obj_rec->struct_rec = struct_rec;
	obj_rec->tha_enable_offset = calloc((obj_rec->struct_rec->n_fields + 2) * units, 
					sizeof(unsigned int));

	rc = ha_reg_object(handle, obj_rec);
	if(rc == FAILURE){
		printf("%s(): object registration with THA failed, Obj_id = %s\n", __FUNCTION__, obj_rec->obj_id);
		return rc;
	}
	/* Computing initial Hash code */

	for(obj_index = 0 ; obj_index < units; obj_index++){

		char *current_obj_ptr = NULL;
		hash_array = get_struct_hash_array_fn(obj_rec, &arr_size, obj_index);

		current_obj_ptr = GET_OBJECT_PTR_AT_INDEX(obj_rec, obj_index);

		// hash code of structure	
		computed_hash_code = hash_code(current_obj_ptr, struct_rec->ds_size);
		hash_array[0] = computed_hash_code;

		fields = struct_rec->fields;
		if(obj_type == STATIC_OBJ){
			for(i = 0; i < struct_rec->n_fields; i++){
				hash_array[i+1] = hash_code(current_obj_ptr + fields->offset, fields->size);
				fields++;
			}
		}
		hash_array[struct_rec->n_fields + 1] = (unsigned int)obj_rec;
	}
	/* if the object is Dynamic and freshly calloc'ed, Synced to Standby right away*/
	if(obj_type == DYN_OBJ)
		tha_sync_object(CREATE_OBJ, handle, obj_rec, SYNC_CHANGED_FIELDS);

	return SUCCESS;
}


/* Dumping Function */

void
dump_struct_db_rec(tha_struct_db_rec_t *rec){
	if(!rec) return;
	int j = 0;
	tha_field_info_t *field = NULL;
	printf(ANSI_COLOR_CYAN "|----------------------------------------------------------------------|\n" ANSI_COLOR_RESET);
	printf(ANSI_COLOR_YELLOW "| %-20s | size = %-8d | #flds = %-3d | tha_off = %-3d |\n" ANSI_COLOR_RESET, rec->struct_name, rec->ds_size, rec->n_fields, 0 );
	printf(ANSI_COLOR_CYAN "|-----------------------------------------------------------------|------------------------------------------------------------------------------------------------------------|\n" ANSI_COLOR_RESET);
	field = rec->fields;
	for(j = 0; j < rec->n_fields; j++){
		printf("  %-20s |", "");
		printf("%-3d. %-20s | dtype = %-15s | size = %-5d | offset = %-6d|  nstructname = %-20s | nested_str_ptr = %-9p |\n", 
				j, field->fname, DATA_TYPE[field->dtype], field->size, field->offset, field->nested_str_name, field->nested_struct_ptr);
		printf("  %-20s |", "");
		printf(ANSI_COLOR_CYAN "-------------------------------------------------------------------------------------------------------------------------------------------------------|\n" ANSI_COLOR_RESET);
		field++;
	}

}

void
dump_tha_struct_db(tha_handle_t *handle){
	tha_struct_db_t *struct_db = handle->struct_db;
	if(!struct_db) return;
	
	printf("printing THA structure db\n");
	int i = 0;
	tha_struct_db_rec_t *rec = NULL;
	rec = struct_db->head;
	printf("No of Structures Registered = %d\n", struct_db->count);
	for(i = 0; i < struct_db->count; i++){
		printf("structure No : %d (%p)\n", i, rec);
		dump_struct_db_rec(rec);
		rec = rec->next;
	}	
}


void
dump_tha_internal_object(char *current_object_ptr, tha_struct_db_rec_t *struct_rec, int level){

	int n_fields = struct_rec->n_fields;
	tha_field_info_t *field = NULL;

	int field_index = 0;
	printf("lvl : %d : Internal Object : %s\n", level, struct_rec->struct_name);	
	printf("-----------------------------\n");
	for(; field_index < n_fields; field_index++){
		field = &(struct_rec->fields[field_index]);
		
		switch(field->dtype){
			case UINT8:
			case INT32:	
			case UINT32:
				printf("%s->%s = %d\n", struct_rec->struct_name,
						field->fname, *(int *)(current_object_ptr + field->offset));
				break;
			case CHAR:
				printf("%s->%s = %s\n", struct_rec->struct_name, field->fname,  (char *)(current_object_ptr + field->offset));
				break;
			case FLOAT:
				printf("%s->%s = %f\n", struct_rec->struct_name, field->fname,  *(float *)(current_object_ptr + field->offset));
				break;
			case DOUBLE:
				printf("%s->%s = %f\n", struct_rec->struct_name, field->fname,  *(double *)(current_object_ptr + field->offset));
				break;
			case OBJ_PTR:
				printf("%s->%s = %p\n", struct_rec->struct_name, field->fname,  (void *)*(int *)(current_object_ptr + field->offset));
				break;
			case OBJ_STRUCT:
				dump_tha_internal_object((char *)(current_object_ptr + field->offset), field->nested_struct_ptr, ++level);
				break;
			case VOID_PTR:
				printf("%s->%s = %p\n", struct_rec->struct_name, field->fname,  (void *)*(int *)(current_object_ptr + field->offset));
				break;
		}
	}	
}

void
dump_tha_object(tha_object_db_rec_t *obj_rec){

	int n_fields = obj_rec->struct_rec->n_fields;
	tha_field_info_t *field = NULL;

	int units = obj_rec->units, obj_index = 0,
	    field_index = 0;

	unsigned int *hash_array = NULL;
	int  arr_size = 0;

	for(; obj_index < units; obj_index++){
		hash_array = get_struct_hash_array_fn(obj_rec, &arr_size, obj_index);
		printf("Struct HC: %d\nFields : \n", hash_array[0]);
		char *current_object_ptr = GET_OBJECT_PTR_AT_INDEX(obj_rec, obj_index);

		for(field_index = 0; field_index < n_fields; field_index++){
			field = tha_get_struct_field_info_by_index(obj_rec, field_index);
			switch(field->dtype){
				case UINT8:
				case INT32:	
				case UINT32:
					printf("%s[%d]->%s = %d, HC:%d\n", obj_rec->struct_rec->struct_name, obj_index, 
						field->fname, *(int *)(current_object_ptr + field->offset), hash_array[field_index+1]);
					break;
				case CHAR:
					printf("%s[%d]->%s = %s, HC:%d\n", obj_rec->struct_rec->struct_name, obj_index, field->fname,  (char *)(current_object_ptr + field->offset), hash_array[field_index+1]);
					break;
				case FLOAT:
					printf("%s[%d]->%s = %f, HC:%d\n", obj_rec->struct_rec->struct_name, obj_index, field->fname,  *(float *)(current_object_ptr + field->offset), hash_array[field_index+1]);
					break;
				case DOUBLE:
					printf("%s[%d]->%s = %f, HC:%d\n", obj_rec->struct_rec->struct_name, obj_index, field->fname,  *(double *)(current_object_ptr + field->offset), hash_array[field_index+1]);
					break;
				case OBJ_PTR:
					printf("%s[%d]->%s = %p, HC:%d\n", obj_rec->struct_rec->struct_name, obj_index, field->fname,  (void *)*(int *)(current_object_ptr + field->offset), hash_array[field_index+1]);
					break;
				case OBJ_STRUCT:
					dump_tha_internal_object((char *)(current_object_ptr + field->offset), field->nested_struct_ptr, 0);
					break;
				case VOID_PTR:
					printf("%s[%d]->%s = %p, HC:%d\n", obj_rec->struct_rec->struct_name, obj_index, field->fname,  (void *)*(int *)(current_object_ptr + field->offset), hash_array[field_index+1]);
					break;
			}
		}
		printf("Back ptr = %p\n\n", (tha_object_db_rec_t *)hash_array[field_index+1]);
	}		
}


void
dump_object_db_rec(tha_object_db_rec_t *rec){
	printf(ANSI_COLOR_CYAN "|--------------------------------------------------------------------------------|\n" ANSI_COLOR_RESET);
	printf("| %-2d | %-10p | %-20s | %-20s | %-3d | %-8s |\n", 
		rec->dirty_bit, rec->ptr, rec->obj_id, rec->obj_name, rec->units, rec->object_type == 1 ? "STATIC" : "DYN");
	printf(ANSI_COLOR_CYAN "|--------------|-----------------------------------------------------------------|\n" ANSI_COLOR_RESET);
	printf("| Hash codes : |\n");
	printf("|--------------|\n");	
	dump_tha_object(rec);
}

void
dump_tha_object_db(tha_handle_t *handle){
	tha_object_db_t *object_db = handle->object_db;
	if(!object_db) return;
	
	printf("printing THA Object db\n");
	int i = 0;
	tha_object_db_rec_t *rec = NULL;
	rec = object_db->head;
	printf("No of Objects Registered = %d\n", object_db->count);
	for(i = 0; i < object_db->count; i++){
		printf("Object No : %d (%p)\n", i, rec);
		dump_object_db_rec(rec);
		rec = rec->next;
	}
}

void 
tha_perform_structure_late_binding(tha_handle_t *handle){
	
	tha_struct_db_t *struct_db = handle->struct_db;
	tha_struct_db_rec_t *head = struct_db->head,
			    *nested_struct = NULL;
	
	tha_field_info_t *fields = NULL;
	int n_fields = 0, i = 0;
	while(head){
		if(!head->late_binding_flag){
			head = head->next;
			continue;
		}

		n_fields = head->n_fields;
		fields = head->fields;

		for(i = 0; i < n_fields; i++){
			if(!(fields[i].dtype == OBJ_PTR || fields[i].dtype == OBJ_STRUCT))
				continue;			
			if(fields[i].nested_struct_ptr == NULL){
				nested_struct = tha_struct_db_look_up(handle, fields[i].nested_str_name);
				if(!nested_struct)
					printf("%s() : Error : Late Binding Failed for Structure = %s, field = %s, Tageget structure = %s\n", 
							__FUNCTION__, head->struct_name, fields[i].fname, fields[i].nested_str_name);
				fields[i].nested_struct_ptr = nested_struct;
				printf("%s() : INFO: Late Binding for Structure = %s, field = %s, Tageget structure = %s\n", __FUNCTION__, head->struct_name, fields[i].fname, fields[i].nested_str_name);
			}
		}
		head->late_binding_flag = 0;
		head = head->next;
	}
}


/* Look up Api implementation*/

/* struct db look up function*/

tha_struct_db_rec_t* 
tha_struct_db_look_up(tha_handle_t *handle,
                        char *struct_name){

	tha_struct_db_t *struct_db = handle->struct_db;
	if(!struct_db || !struct_db->head) return NULL;
	int i = 0;
	tha_struct_db_rec_t *struct_rec = NULL;
	struct_rec = struct_db->head;
	for(i = 0; i < struct_db->count; i++){
		if(strncmp(struct_name, struct_rec->struct_name, MAX_STRUCTURE_NAME_SIZE) == 0)
			return struct_rec;
		struct_rec = struct_rec->next;
		continue;
	}
	return NULL;
}

/* Object DB lookup function*/

tha_object_db_rec_t* 
tha_object_db_lookup_by_name(tha_handle_t *handle,  char *object_name){
	
	tha_object_db_t *object_db = handle->object_db;
	if(!object_db || !object_db->head) return NULL;
	int i = 0;
	tha_object_db_rec_t *obj_rec = NULL;
	obj_rec = object_db->head;
	for(i = 0; i < object_db->count; i++){
		if(strncmp(object_name, obj_rec->obj_name, MAX_STRUCTURE_NAME_SIZE) == 0)
			return obj_rec;
		obj_rec = obj_rec->next;
		continue;
	}
	return NULL;
}

tha_object_db_rec_t* 
tha_object_db_lookup_by_Addr(tha_handle_t *handle , void *addr){

	tha_object_db_t *object_db = handle->object_db;	
	if(!object_db || !object_db->head || !addr) return NULL;
	int i = 0;
	tha_object_db_rec_t *obj_rec = NULL;
	obj_rec = object_db->head;
	for(i = 0; i < object_db->count; i++){
		if(obj_rec->ptr == addr)
			return obj_rec;
		obj_rec = obj_rec->next;
		continue;
	}
	return NULL;
}


tha_object_db_rec_t* 
tha_object_db_lookup_by_objid(tha_handle_t *handle,  char *obj_id){
	
	tha_object_db_t *object_db = handle->object_db;
	if(!object_db || !object_db->head) return NULL;
	int i = 0;
	tha_object_db_rec_t *obj_rec = NULL;
	obj_rec = object_db->head;
	for(i = 0; i < object_db->count; i++){
		if(strncmp(obj_id, obj_rec->obj_id, MAX_STRUCTURE_NAME_SIZE) == 0)
			return obj_rec;
		obj_rec = obj_rec->next;
		continue;
	}
	return NULL;
}

unsigned int *
get_struct_hash_array_fn(tha_object_db_rec_t *obj_rec, int *hash_array_size, int obj_index){
	if(!obj_rec){
		printf("%s(): Error : Object record in tha_object_db not found\n", __FUNCTION__);
		return NULL;
	}
	tha_struct_db_rec_t *struct_rec = obj_rec->struct_rec;
	if(!struct_rec){
		printf("%s(): Error : Structure Record not found for Object Record\n", __FUNCTION__);
		return NULL;
	}	
	*hash_array_size = (struct_rec->n_fields + 2);

	unsigned int *tha_enable_offset = obj_rec->tha_enable_offset;
	return tha_enable_offset + (obj_index * (*hash_array_size));
}

tha_field_info_t*
tha_get_struct_field_info_by_index(tha_object_db_rec_t *obj_rec, int field_index){
	tha_struct_db_rec_t *struct_rec = obj_rec->struct_rec;
	return &struct_rec->fields[field_index];	
}

int
bind_field_nested_structure(tha_handle_t *handle, tha_field_info_t *fields, int n_fields){
	int i = 0, LATE_BINDING_FLAG = SUCCESS;
	tha_struct_db_rec_t *struct_rec = NULL;

	for(; i < n_fields; i++){
		if(!(fields->dtype == OBJ_STRUCT || fields->dtype == OBJ_PTR)){
			fields++;
			continue;
		}

		struct_rec = tha_struct_db_look_up(handle, fields->nested_str_name);
		if(!struct_rec){
			printf("%s(): WARNING : Registration failed for field : %s, Nested structure : %s, fdtype = %d."
				"Probably, you should register nested structure first Or tha_perform_structure_late_binding()"
				" call will take care of this.\n", __FUNCTION__, fields->fname, fields->nested_str_name, fields->dtype);
			LATE_BINDING_FLAG = FAILURE;
			fields++;
			continue;
		}
		
		fields->nested_struct_ptr = struct_rec;	
		fields++;
	}
	return LATE_BINDING_FLAG;
}

int
tha_remove_object_db_rec(tha_handle_t *handle, 
	tha_object_db_rec_t *object_rec){

	tha_object_db_t *object_db = handle->object_db;

	if(!object_rec) return FAILURE;

	// case 0 : if object_rec is the only record in object db
	if(object_rec == object_db->head && object_db->count == 1){
		object_db->head = NULL;
		object_db->count--;
		free(object_rec);
		return SUCCESS;
	}
	
	// case 1 : if object_rec is first record
	if(object_rec == object_db->head){
		object_db->head = object_db->head->next;
		object_db->head->prev = NULL;
		free(object_rec);
		object_db->count--;
		return SUCCESS;
	}

	// case 2 : if object_rec is last record
	if(object_rec->next == NULL){
		object_rec->prev->next = NULL;
		free(object_rec);
		object_db->count--;
		return SUCCESS;
	}
	
	// case 3 : if object_rec is in the middle
	if(object_rec->next && object_rec->prev){
		object_rec->prev->next = object_rec->next;
		object_rec->next->prev = object_rec->prev;
		object_db->count--;
		free(object_rec);
		return SUCCESS;
	}
	
	printf("%s(): Error : Could not find object_rec with obj_id = : %s in object_db\n", 
			__FUNCTION__, object_rec->obj_id);
	return FAILURE;		
}

void
free_object_in_object_record(tha_object_db_rec_t *object_rec){

        FREE_OBJECT_HASH_ARRAY(object_rec);
        free(object_rec->ptr);
	object_rec->ptr = NULL;
}



void
internal_ha_free(tha_object_db_rec_t *object_rec){

	if(object_rec == NULL || object_rec->ptr == NULL)
		return;
#if 0
	if(I_AM_ACTIVE_CP == 0)
		return;
#endif

	free_object_in_object_record(object_rec);
	
	if(tha_sync_delete_one_object(handle, object_rec->obj_id) != SUCCESS){
		printf("%s(): Error : Del Sync to standby failed\n", __FUNCTION__);
	}

	if(tha_remove_object_db_rec(handle, object_rec) == FAILURE)
		printf("%s(): Error : Could not find the object_Rec in objectdb\n", __FUNCTION__);
	else
		printf("%s(): object_rec with obj_id = %s successfully removed\n", __FUNCTION__, object_rec->obj_id);	
}

void
ha_free(void *ptr){

	tha_object_db_rec_t *object_rec = NULL;
#if 0
	if(I_AM_ACTIVE_CP == 0)
		return;
#endif
	object_rec = tha_object_db_lookup_by_Addr(handle, ptr);
	if(!object_rec){
		printf("%s(): Error : attempt to free the Non-THA dynamic Object\n", __FUNCTION__);
		return;
	}

	free_object_in_object_record(object_rec);
	
	if(tha_sync_delete_one_object(handle, object_rec->obj_id) != SUCCESS){
		printf("%s(): Error : Del Sync to standby failed\n", __FUNCTION__);
	}

	if(tha_remove_object_db_rec(handle, object_rec) == FAILURE)
		printf("%s(): Error : Could not find the object_Rec in objectdb\n", __FUNCTION__);
	else
		printf("%s(): object_rec with obj_id = %s successfully removed\n", __FUNCTION__, object_rec->obj_id);	
}

void
tha_clean_application_state(tha_handle_t *handle){

	tha_object_db_t *object_db = handle->object_db;
	tha_object_db_rec_t *head = object_db->head, 
			    *temp = NULL;

	/* Del All Dynamic Objects*/
	if(I_AM_ACTIVE_CP == 1)
		return;
FIRST:
	if(head && head->object_type == DYN_OBJ){
		free_object_in_object_record(head);
		tha_remove_object_db_rec(handle, head);
		head = object_db->head;
		goto FIRST;
	}

	while(head){
		if(head->object_type == STATIC_OBJ){
			head = head->next;
			continue;
		}

		free_object_in_object_record(head);
		temp = head->prev;
		tha_remove_object_db_rec(handle, head);
		head = temp->next;
	}

	head = object_db->head;
	
	/*Reset all Static objects*/
	while(head){
		int n_fields = head->struct_rec->n_fields, units = head->units;
		memset(head->tha_enable_offset, 0, (n_fields + 2) * units * sizeof(unsigned int));
		memset(head->ptr, 0, head->struct_rec->ds_size * units);
		head = head->next;
	}
}



