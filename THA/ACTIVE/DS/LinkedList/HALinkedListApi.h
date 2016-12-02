#ifndef __HA_LINKEDLISTAPI__
#define __HA_LINKEDLISTAPI__

#define HA_GET_HEAD_SINGLY_LL(ll) (ll->head)
#define HA_INC_NODE_COUNT_SINGLY_LL(ll) (ll->node_count++)
#define HA_DEC_NODE_COUNT_SINGLY_LL(ll) (ll->node_count--)
#define HA_GET_NODE_COUNT_SINGLY_LL(ll) (ll->node_count)
#define HA_GET_NEXT_NODE_SINGLY_LL(node) (node->next)

typedef enum{
    HA_LL_SUCCESS,
    HA_LL_FAILURE
} ha_rc_t;

typedef enum{
    HA_LL_FALSE,
    HA_LL_TRUE
} ha_ll_bool_t;

typedef struct HA_LL_Node{
    int data;
    struct HA_LL_Node *next;
    int *tha_enable;
} ha_singly_ll_node_t;

typedef struct HA_LL{
    unsigned int node_count;
    ha_singly_ll_node_t *head;
    int *tha_enable;
} ha_ll_t;

ha_ll_t* ha_init_singly_ll();
ha_singly_ll_node_t* ha_singly_ll_init_node(int data);
ha_rc_t ha_singly_ll_add_node(ha_ll_t *ll, ha_singly_ll_node_t *node);
ha_rc_t ha_singly_ll_add_node_by_val(ha_ll_t *ll, int data);
ha_rc_t ha_singly_ll_remove_node(ha_ll_t *ll, ha_singly_ll_node_t *node);
unsigned int ha_singly_ll_remove_node_by_value(ha_ll_t *ll, int data);
ha_ll_bool_t ha_is_singly_ll_empty(ha_ll_t *ll);
void ha_print_singly_LL(ha_ll_t *ll);
void ha_reverse_singly_ll(ha_ll_t *ll);
void ha_delete_singly_ll(ha_ll_t *ll);
#endif
