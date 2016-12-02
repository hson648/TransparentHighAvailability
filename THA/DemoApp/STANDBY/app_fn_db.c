#include "app_fn_db.h"

int
compare_fn_int(int key1, int key2){
	return 0;
}

void
app_fn_ptr_db(void **dst, int fn_id){
        switch(fn_id){
                case COMPARE_FN_INT:
                        *dst = compare_fn_int;
                         break;
                default:
                        break;
        }
}

