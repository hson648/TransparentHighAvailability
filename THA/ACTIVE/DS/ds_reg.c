#include "./LinkedList/HALinkedListApi.h"
#include "./Trees/tree.h"
#include "./Stack/stack.h"
#include "./Queue/Queue.h"
#include "./../tha_db.h"
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>

void linkedlist_tha_reg(tha_handle_t *handle){

	static tha_field_info_t ha_singly_ll_node_t_fields[] = {
		 HA_FIELD_INFO(ha_singly_ll_node_t, data, INT32, 0),
		 HA_FIELD_INFO(ha_singly_ll_node_t, next, OBJ_PTR, ha_singly_ll_node_t)
	};

	HA_REG_STRUCT(handle, ha_singly_ll_node_t, ha_singly_ll_node_t_fields);

	static tha_field_info_t ha_ll_t_fields[] = {
		HA_FIELD_INFO(ha_ll_t, node_count, INT32, 0),
		HA_FIELD_INFO(ha_ll_t, head, OBJ_PTR, ha_singly_ll_node_t)
	};

	HA_REG_STRUCT(handle, ha_ll_t, ha_ll_t_fields);
}

void stack_tha_reg(tha_handle_t *handle){
	
	static tha_field_info_t stack_t_fields[] = {
		HA_FIELD_INFO(stack_t, top, INT32, 0),
		HA_FIELD_INFO(stack_t, slot, VOID_PTR, 0),
		HA_FIELD_INFO(stack_t, count_of_push, INT32, 0),
		HA_FIELD_INFO(stack_t, count_of_pop, INT32, 0)
	};

	HA_REG_STRUCT(handle, stack_t, stack_t_fields);
}

void tree_tha_reg(tha_handle_t *handle){

	static tha_field_info_t tree_node_t_fields[] = {
		HA_FIELD_INFO(tree_node_t, left, OBJ_PTR, tree_node_t),
		HA_FIELD_INFO(tree_node_t, right, OBJ_PTR, tree_node_t),
		HA_FIELD_INFO(tree_node_t, data, INT32, 0),
		HA_FIELD_INFO(tree_node_t, isVisited, CHAR, 0)
	};

	HA_REG_STRUCT(handle, tree_node_t, tree_node_t_fields);

	static tha_field_info_t tree_t_fields[] = {
		HA_FIELD_INFO(tree_t, root, OBJ_PTR, tree_node_t),
		//HA_FIELD_INFO(tree_t, stack, OBJ_PTR, stack_t)
	};

	HA_REG_STRUCT(handle, tree_t, tree_t_fields);
}

void queue_tha_reg(tha_handle_t *handle){
	
	static tha_field_info_t Queue_t_fields[]={
		HA_FIELD_INFO(Queue_t, elem, VOID_PTR, 0),
		HA_FIELD_INFO(Queue_t, front, UINT32, 0),
		HA_FIELD_INFO(Queue_t, rear, UINT32, 0),
		HA_FIELD_INFO(Queue_t, count, UINT32, 0)
	};

	HA_REG_STRUCT(handle, Queue_t, Queue_t_fields);
}
