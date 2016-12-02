#include <stdio.h>
#include <stdlib.h>
#include "../../ACTIVE/tha_db.h"

#include "./../../ACTIVE/DS/Trees/tree.h"
#include "./../../ACTIVE/DS/LinkedList/LinkedListApi.h"
#include "app_fn_db.h"
#include "app.h"

/* Global Variables*/

tha_obj_ptr_t tree; 
tha_obj_ptr_t list1;
tha_obj_ptr_t list2;
tha_obj_ptr_t manager;
tha_obj_ptr_t engineer;

void main_menu(){
	Main:
	printf("***************************\n");
	printf("	1. Add element in a list1\n");
	printf("       17. Add element in a list2\n");
	printf("	2. Delete element from the list1\n");
        printf("       20. Delete complete list\n");
	printf("       18. Delete element from the list2\n");
	printf("	3. Add element in a tree\n");
	printf("	4. Dump List1\n");
	printf("       19. Dump List2\n");
	printf("	5. Dump Tree\n");
	printf("	6. Reverse List1\n");
	printf("	7. Dump Structure DB\n");
	printf("	8. Dump Object DB\n");
	printf("	9. exit\n");
	printf("	10. Show Menu\n");
	printf("	11. ha_sync ?\n");
	printf("	12. Tree:LowestCommonAncestor\n");
	printf("	13. Tree: InorderNR\n");
	printf("	14. Tree:removeHalfNodes\n");
	printf("	15. Clean Standby Application State\n");
	printf("	16. Bulk Sync\n");
	printf("	21. No of Objects that will sync\n");
	printf("Enter Choice : ");
	int choice;
	scanf("%d", &choice);
	printf("\n");
	switch(choice){
		case 1:
		{
			int elem;
			printf("Enter new element to be added:");
			scanf("%d", &elem);
			ha_singly_ll_add_node_by_val(list1.ptr, elem);
			//ha_sync(handle);
			break;
		}
		case 17:
		{
			int elem;
			printf("Enter new element to be added:");
			scanf("%d", &elem);
			ha_singly_ll_add_node_by_val(list2.ptr, elem);
			//ha_sync(handle);
			break;
		}
		case 2:
		{
			int elem;
			printf("Enter new element to be removed:");
			scanf("%d", &elem);
			ha_singly_ll_remove_node_by_value(list1.ptr, elem);
			//ha_sync(handle);
			break;
		}
		case 20:
			ha_delete_singly_ll(list1.ptr);;
			break;
		case 18:
		{
			int elem;
			printf("Enter new element to be removed:");
			scanf("%d", &elem);
			ha_singly_ll_remove_node_by_value(list2.ptr, elem);
			//ha_sync(handle);
			break;
		}
		case 3:
		{
			int elem;
			printf("Enter new element to be added:");
			scanf("%d", &elem);
			add_tree_node_by_value(tree.ptr, elem);
			//ha_sync(handle);
			break;
		}
		case 4:
			ha_print_singly_LL(list1.ptr);
			break;	
		case 19:
			ha_print_singly_LL(list2.ptr);
			break;	
		case 5:
			printAllTraversals(tree.ptr);
			break;
		case 6:
			ha_reverse_singly_ll(list1.ptr);
			break;
		case 7:
			dump_tha_struct_db(handle);
			break;	
		case 8:
			dump_tha_object_db(handle);
			break;	
		case 9:
			exit(0);
		case 10:
			goto Main;
		case 11:
			ha_sync(handle);
			break;
		case 12:
			{
				printf("	LowestCommonAncestor : Enter Two node values. Val 1 = ?");
				int n1,n2;
				scanf("%d", &n1);
				printf("		Enter 2nd no, Val 2 = ?");
				scanf("%d", &n2);
				printf("\n");
				printf("	LowestCommonAncestor = %d\n", (LowestCommonAncestor(tree.ptr, n1, n2))->data);
			}
			break;
		case 13:
			InorderNR(tree.ptr);
			break;
		case 14:
			tree.ptr = removeHalfNodes(tree.ptr);
			printf("	Half nodes removed\n");
			break;
		case 15:
			send_tha_clean_application_state_order(handle);
			break;
		case 16:	
			tha_bulk_sync(handle);
			break;
		case 21:
			printf("No of Objects that will sync = %d\n", mark_changed_objects(handle));
			break;
		default:
			goto Main;
	}
	goto Main;
}

void THA_APPLICATION_GLOBAL_STATE_INITIALISATION(){

	/*Step 11: Initialise the handle to global objects of the application. Note, this code fragment
	will execute only on Active MM. */
	tree.ptr = init_tree();
	list1.ptr = (void *)ha_init_singly_ll();
	list2.ptr = (void *)ha_init_singly_ll();

#if 1
	/* Initalise global objects as well*/
	ha_calloc(handle, manager.ptr,  manager_t,  1);
	ha_calloc(handle, engineer.ptr, engineer_t, 1);

	strcpy(((manager_t *)(manager.ptr))->manager_name, "Govind");
	((manager_t *)(manager.ptr))->engineer = engineer.ptr;

	strcpy(((engineer_t *)(engineer.ptr))->eng_name, "Abhishek");
	((engineer_t *)(engineer.ptr))->manager = manager.ptr;
#endif
	/* Just run ha_sync(handle) once after Registration of Global Objects 
	to ensure sync the calloc'd element of Static global objects to standby, if any.
	If you do not do this, next ha_sync() will do this. However, Why leave any temporal
	window to let Active and Standby out of sync untill then*/

	/*Step 12 : call ha_sync once to sync all initialsed data to standby MM*/
	ha_sync(handle);
	/*Step 13: Now Active and Standby are in Sync witl all Registeration and initialsation
	Done. Transfer control to Active application to execute.*/
	main_menu();
}

int
main(int argc, char **argv){
	/*Step 1 : Set the below variable to 1 Or zero depending the instance of this application
	is to be run in Active mode ot standby mode. I_AM_ACTIVE_CP is a THA library global variable
	exposed to application*/

	I_AM_ACTIVE_CP =0;

	/*Step 2 : handle is also THA library global variable exposed to application. Set this variable as below
	This will register the default structures int/float/double/ptr  and standard APIs like Trees/LinkedList which
	is THA enable*/

	handle = init_tha("127.0.0.1", app_fn_ptr_db);

	/*Step 3 : Add "int *tha_enable" as the last field in all structures which needs HA. see app.h"*/

	/*step 4 : For structures which needs HA, declare and initialise the array of fields as shown below. 
	You can be selective which fields needs HA or not. Fields of a structure which do not needs HA
	should not be mentioned in the array. Sequence of fields specified in the array is irrelevant.*/
	static tha_field_info_t engineer_t_fields[] = {
		HA_FIELD_INFO(engineer_t, eng_name, CHAR, 0),
		HA_FIELD_INFO(engineer_t, manager, OBJ_PTR, manager_t)
	};

	static tha_field_info_t manager_t_fields[] = {
		HA_FIELD_INFO(manager_t, manager_name, CHAR, 0),
		HA_FIELD_INFO(manager_t, engineer, OBJ_PTR, engineer_t)

	};

	/*Step 5: After you have declared and initialised the static arrays of fields in Step 4,
	  register the structure and its fields as below. Now THA framework is aware of your structure fully
	  which needs HA. */
	HA_REG_STRUCT(handle, manager_t, manager_t_fields);
	HA_REG_STRUCT(handle, engineer_t, engineer_t_fields);

	/*Step 6 : There could be cyclic dependency in between structures which needs HA. Call the below
	fn when all the HA structures have been registered with THA. This fn will resolve cyclic dependency.
	After Completetion of Registration of all structures, perform late binding. you should not register any
	new structures after below fn call, if you do, call this fn again*/	

	tha_perform_structure_late_binding(handle);

	/* Step 7: Application may have Global objects. In this step , you needs to register the handle 
	of the global objects which needs THA. Register global objects handle. No Need for ha_memset, 
	as it is taken care of  by macro. In case, you dont have a handle to global object, but a global
	object directly, (manager and engineer global variables in our case), register them as well here.*/

	THA_ADD_GLOBAL_OBJECT(handle, tha_obj_ptr_t, tree);    // registering the pointer to the global tree
	THA_ADD_GLOBAL_OBJECT(handle, tha_obj_ptr_t, list1);   // registering the pointer to the global list 1
	THA_ADD_GLOBAL_OBJECT(handle, tha_obj_ptr_t, list2);   // registering the pointer to global list 2
	THA_ADD_GLOBAL_OBJECT(handle, tha_obj_ptr_t, manager); // register global objects directly
	THA_ADD_GLOBAL_OBJECT(handle, tha_obj_ptr_t, engineer);// register global objects directly.

	/* Now application will adopt the Active and Standby Role*/

	/*Step 8 : Here, Application will start in Active Or standby Mode depending on the value
	of I_AM_ACTIVE_CP set in step 1.*/
	if(I_AM_ACTIVE_CP == 0){
		printf("Application slipping into standby mode\n");
	/*Step 9 : Pass the pointer to the fn here which needs to be executed by application
	 in standby mode */
		tha_acquire_standby_state(main_menu);
	}

	/*Step 10 : pass the pointer to the fn which will do initialisation of global objects handle registered in
	 step 7 : tree, list1, list2*/
	tha_acquire_active_state(THA_APPLICATION_GLOBAL_STATE_INITIALISATION);
		
	return 0;
}
