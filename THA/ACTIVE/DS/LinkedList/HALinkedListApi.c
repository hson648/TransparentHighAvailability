#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "HALinkedListApi.h"
#include <assert.h>
#include "./../../tha_db.h"

ha_ll_t* ha_init_singly_ll(){
	ha_ll_t *ptr = NULL;
	ha_calloc(handle, ptr, ha_ll_t, 1);
	return ptr;
}

ha_singly_ll_node_t* ha_singly_ll_init_node(int data){
    ha_singly_ll_node_t* node = NULL;
    ha_calloc(handle, node, ha_singly_ll_node_t, 1);
    if(!node) return NULL;
    node->data = data;
    return node;
}

ha_rc_t 
ha_singly_ll_add_node(ha_ll_t* ll, ha_singly_ll_node_t *node){
    if(!ll) return HA_LL_FAILURE;
    if(!node) return HA_LL_FAILURE;
    if(!HA_GET_HEAD_SINGLY_LL(ll)){
        HA_GET_HEAD_SINGLY_LL(ll) = node;
        HA_INC_NODE_COUNT_SINGLY_LL(ll);
        return HA_LL_SUCCESS;
    }

    node->next = HA_GET_HEAD_SINGLY_LL(ll);
    HA_GET_HEAD_SINGLY_LL(ll) = node;
    HA_INC_NODE_COUNT_SINGLY_LL(ll);
    return HA_LL_SUCCESS;
}

ha_rc_t 
ha_singly_ll_add_node_by_val(ha_ll_t *ll, int data){
    ha_singly_ll_node_t* node = ha_singly_ll_init_node(data);
    return ha_singly_ll_add_node(ll, node);
}

ha_rc_t
ha_singly_ll_remove_node(ha_ll_t *ll, ha_singly_ll_node_t *node){
    if(!ll) return HA_LL_FAILURE;
    if(!HA_GET_HEAD_SINGLY_LL(ll) || !node) return HA_LL_SUCCESS;
    ha_singly_ll_node_t *trav = NULL;
    /*if node is not the last node*/
    if(node->next){
        ha_singly_ll_node_t *temp = NULL;
        node->data = node->next->data;
        temp = node->next;
        node->next = node->next->next;
        ha_free(temp);
        HA_DEC_NODE_COUNT_SINGLY_LL(ll);
        return HA_LL_SUCCESS;
    }

    /* if node is the only node in LL*/
    if(ll->node_count == 1 && HA_GET_HEAD_SINGLY_LL(ll) == node){
    	ha_free(node);
        HA_GET_HEAD_SINGLY_LL(ll) = NULL;
        HA_DEC_NODE_COUNT_SINGLY_LL(ll);
        return HA_LL_SUCCESS;
    }

    /*if node is the last node of the LL*/
    trav = HA_GET_HEAD_SINGLY_LL(ll);
    while(trav->next != node){
        trav = trav->next;
        continue;
    }
    
    trav->next = NULL;
    ha_free(node);
    HA_DEC_NODE_COUNT_SINGLY_LL(ll);
    return HA_LL_SUCCESS;
}

unsigned int
ha_singly_ll_remove_node_by_value(ha_ll_t *ll, int data){
    if(!ll || !HA_GET_HEAD_SINGLY_LL(ll)) return 0;
    unsigned int curren_node_count = HA_GET_NODE_COUNT_SINGLY_LL(ll);
    ha_singly_ll_node_t* trav = HA_GET_HEAD_SINGLY_LL(ll);
    while(trav != NULL){
        if(trav->data == data){
            ha_singly_ll_remove_node(ll, trav);
	    return curren_node_count - HA_GET_NODE_COUNT_SINGLY_LL(ll); 
        }
        trav = trav->next;
    }
    return curren_node_count - HA_GET_NODE_COUNT_SINGLY_LL(ll);
}

void ha_print_singly_LL(ha_ll_t *ll){
    if(!ll) {
        printf("Invalid Linked List\n"); 
        return;
    }
    if(ha_is_singly_ll_empty(ll)){
        printf("Empty Linked List\n");
        return;
    }
    
    ha_singly_ll_node_t* trav = HA_GET_HEAD_SINGLY_LL(ll);
    unsigned int i = 0;
    printf("node count = %d\n", HA_GET_NODE_COUNT_SINGLY_LL(ll));
    while(trav){
        printf("%d. Data = %d\n", i, trav->data);
        i++;
        trav = trav->next;
    }
}

ha_ll_bool_t 
ha_is_singly_ll_empty(ha_ll_t *ll){
    if(!ll) assert(0);
    if(ll->node_count == 0)
        return HA_LL_TRUE;
    return HA_LL_FALSE;
}

void 
ha_reverse_singly_ll(ha_ll_t *ll){
   if(!ll) assert(0) ;
   if(ha_is_singly_ll_empty(ll)) return;
   if(HA_GET_NODE_COUNT_SINGLY_LL(ll) == 1) return;
   ha_singly_ll_node_t *p1 = HA_GET_HEAD_SINGLY_LL(ll), 
                    *p2 = ll->head->next, *p3 = NULL;
   p1->next = NULL;
   do{
        p3 = p2->next;
        p2->next = p1;
        p1 = p2;
        p2 = p3;
   }while(p3);
   ll->head = p1;
   return;
}

void
ha_delete_singly_ll(ha_ll_t *ll){
	if(!ll) return;

	if(ha_is_singly_ll_empty(ll)){
		return;
	}

	ha_singly_ll_node_t *head = HA_GET_HEAD_SINGLY_LL(ll),
			    *next = HA_GET_NEXT_NODE_SINGLY_LL(head);

	do{
		ha_free(head);
		head = next;
		if(next)
			next = HA_GET_NEXT_NODE_SINGLY_LL(next);

	} while(head);

	ll->node_count = 0;
	ll->head = NULL;
}

#if 0
int 
main(int argc, char **argv){
    ha_ll_t* list = init_singly_ll();
    singly_ll_add_node_by_val(list, 1);
    singly_ll_add_node_by_val(list, 2);
    singly_ll_add_node_by_val(list, 3);
    singly_ll_add_node_by_val(list, 4);
    singly_ll_add_node_by_val(list, 5);
    singly_ll_add_node_by_val(list, 6);
    singly_ll_add_node_by_val(list, 7);
    singly_ll_add_node_by_val(list, 8);
    singly_ll_add_node_by_val(list, 9);
    singly_ll_add_node_by_val(list, 10);
#if 0
    unsigned int num_nodes_removed = singly_ll_remove_node_by_value(list, 100);
    printf("number of noded removed = %d\n", num_nodes_removed);
    print_singly_LL(list);
#endif
    reverse_singly_ll(list);
    print_singly_LL(list);

   return 0; 
}

#endif
