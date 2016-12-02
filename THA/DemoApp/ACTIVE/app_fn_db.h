#ifndef __APP_FN_DB__
#define __APP_FN_DB__


/*fn IDS*/
#define COMPARE_FN_INT	0


/* fn prototypes */

int
compare_fn_int(int key1, int key2);


/* fn ptr registration fn*/
void
app_fn_ptr_db(void **dst, int fn_id);
#endif 
