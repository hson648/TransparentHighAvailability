#include <stdio.h>
#include "tha_db.h"
#include "app.h"
#include "stdlib.h"
#include "string.h"
#include <signal.h>

#if 0
static void
ctrlC_signal_handler(int sig){
        printf("ctrl C pressed\n");
	I_AM_ACTIVE_CP = !I_AM_ACTIVE_CP;
	printf("Application Role switched to %s\n", I_AM_ACTIVE_CP ? "ACTIVE" : "STANDBY");
	if(I_AM_ACTIVE_CP)
		tha_acquire_active_state(NULL);
	else
		tha_acquire_standby_state(NULL);
}
#endif

int
main(int argc, char **argv){

	// I am Active CP
	I_AM_ACTIVE_CP = 1;
	handle = init_tha("127.0.0.1", NULL);

	//signal(SIGINT, ctrlC_signal_handler);
	
	/* Registering a structure*/

	static tha_field_info_t person_t_fields[] = {
		HA_FIELD_INFO(person_t, name, CHAR, 0),
		HA_FIELD_INFO(person_t, age, INT32, 0),
		HA_FIELD_INFO(person_t, address, CHAR, 0),
		HA_FIELD_INFO(person_t, designation, CHAR, 0),
		HA_FIELD_INFO(person_t, occupation, OBJ_PTR, emp_t),
		HA_FIELD_INFO(person_t, laptop, OBJ_STRUCT, lap_t)
	};

	static tha_field_info_t emp_t_fields[] = {
		HA_FIELD_INFO(emp_t, name, CHAR, 0),
		HA_FIELD_INFO(emp_t, age, INT32, 0), 
		HA_FIELD_INFO(emp_t, address, CHAR, 0),
		HA_FIELD_INFO(emp_t, designation, CHAR, 0)
	};

	static tha_field_info_t lap_t_fields[] = {
		 HA_FIELD_INFO(lap_t, model, CHAR, 0),
	         HA_FIELD_INFO(lap_t, company, CHAR, 0),
	         HA_FIELD_INFO(lap_t, price, UINT32, 0),
	         HA_FIELD_INFO(lap_t, yr, UINT32, 0),
		 HA_FIELD_INFO(lap_t, serial_no, UINT32, 0),
		 HA_FIELD_INFO(lap_t, no_acc, UINT32, 0),
		 HA_FIELD_INFO(lap_t, lap_hw, OBJ_STRUCT, laptop_hw_t)
	};

	static tha_field_info_t laptop_hw_t_fields[] = {
                 HA_FIELD_INFO(laptop_hw_t, screen_size, FLOAT, 0),
		 HA_FIELD_INFO(laptop_hw_t, discount, FLOAT, 0),
		 HA_FIELD_INFO(laptop_hw_t, stock_qty, UINT32, 0),
		 HA_FIELD_INFO(laptop_hw_t, warranty, INT32, 0),
		 HA_FIELD_INFO(laptop_hw_t, ram_size, INT32, 0),
		 HA_FIELD_INFO(laptop_hw_t, HDsize, INT32, 0),
                 HA_FIELD_INFO(laptop_hw_t, disk_type, CHAR, 0),
		 HA_FIELD_INFO(laptop_hw_t, HDMI, INT32, 0) 
	};

	static tha_field_info_t engineer_t_fields[] = {
		HA_FIELD_INFO(engineer_t, eng_name, CHAR, 0),
		HA_FIELD_INFO(engineer_t, manager, OBJ_PTR, manager_t)
	};

	static tha_field_info_t manager_t_fields[] = {
		HA_FIELD_INFO(manager_t, manager_name, CHAR, 0),
		HA_FIELD_INFO(manager_t, engineer, OBJ_PTR, engineer_t)

	};
	
	HA_REG_STRUCT(handle, emp_t, emp_t_fields); // emp_t must be registered first
	HA_REG_STRUCT(handle, laptop_hw_t, laptop_hw_t_fields);
	HA_REG_STRUCT(handle, lap_t, lap_t_fields);	
	HA_REG_STRUCT(handle, person_t, person_t_fields);	

	HA_REG_STRUCT(handle, manager_t, manager_t_fields);
	HA_REG_STRUCT(handle, engineer_t, engineer_t_fields);

	tha_perform_structure_late_binding(handle);

	/* for Stabndby CP : Main Thread should block itself here after completing the registration of 
	structures and 	Global variables */
	person_t person1[2];
	person_t person2;
	THA_ADD_GLOBAL_OBJECT_BY_REF(handle, person1, person_t);
	THA_ADD_GLOBAL_OBJECT_BY_VAL(handle, person2, person_t);
	
	int aaa = 0;
	THA_ADD_GLOBAL_OBJECT_BY_VAL(handle, aaa, int);
		
	if(I_AM_ACTIVE_CP == 0){
		printf("Application slipping into standby mode\n");
		tha_acquire_standby_state(NULL);
	}

	//tha_acquire_active_state();
	
	//dump_tha_struct_db(handle);

	//person_t *person = NULL;
	//emp_t  *emp  = NULL;


	manager_t *manager = NULL;
	ha_calloc(handle, manager, manager_t, 1);
	
	engineer_t *engineer = NULL;
	ha_calloc(handle, engineer, engineer_t, 1);

	strcpy(manager->manager_name, "Govind");
	manager->engineer = engineer;

	strcpy(engineer->eng_name, "Abhishek");
	engineer->manager = manager;

#if 0




	strcpy(person1[0].name, "Abhishek");
	person1[0].age	= 30;
	strcpy(person1[0].address, "Bangalore");
	strcpy(person1[0].designation, "SE3@Brocade");

	emp_t  *emp  = NULL;
	ha_calloc(handle, emp, emp_t, 1);
	
	emp_t  *non_tha_emp  = NULL;
	non_tha_emp = calloc(1, sizeof(emp_t));
	
	
	strcpy(emp->name, "Abhishek");
	emp->age = 30;
	strcpy(emp->address, "Mahadevpura");
	strcpy(emp->designation, "SE3");
	
	strcpy(non_tha_emp->name, "Shivani");
	non_tha_emp->age = 29;
	strcpy(non_tha_emp->address, "Chandigarh");
	strcpy(non_tha_emp->designation, "PhD");
	

	strcpy(person1[0].laptop.model, "Dell");
	strcpy(person1[0].laptop.company, "Brocade");
	person1[0].laptop.price = 70000;
	person1[0].laptop.yr = 2012;
	person1[0].laptop.serial_no = 4040;
	person1[0].laptop.no_acc = 0;
	person1[0].laptop.lap_hw.screen_size = 21.2f;
	person1[0].laptop.lap_hw.discount = 10.03f;
	person1[0].laptop.lap_hw.stock_qty = 10;
	person1[0].laptop.lap_hw.warranty = 4;
	person1[0].laptop.lap_hw.ram_size = 8048;
	person1[0].laptop.lap_hw.HDsize = 128;
	strcpy(person1[0].laptop.lap_hw.disk_type, "SDD");
	person1[0].laptop.lap_hw.HDMI = 1;
	person1[0].occupation = emp;
	
	ha_memcpy(&person1[1], &person1[0], sizeof(person_t));
	ha_memset(&person1[0], 0, sizeof(person_t));
	ha_memset_at_index(person1, 1, 0, sizeof(person_t));
#endif

#if 0
	static person_t person1[50];
	person1.age = 30;
	strcpy((char *)&person1.name, "Abhishek");	
	person2.age=30;
	strcpy((char *)&person2.name, "Abhishek");
	lenovo.price =1;
	//lenovo[1].price =1;	

#endif
	//dump_tha_object_db(handle);
	//THA_ADD_GLOBAL_OBJECT(handle, person_t, person2);
	//THA_ADD_GLOBAL_OBJECT(handle, lap_t, lenovo);
	//THA_ADD_GLOBAL_OBJECT(handle, emp_t, emp);

	#if 0
	//person1.age = 34;
	//strcpy(person1.designation, "BrocadeEmp");
	person1[1].age = 35;
	strcpy(person1[1].designation, "JuniperEmp");
	person1[3].age = 36;
	strcpy(person1[2].designation, "JuniperEmp");
	//lenovo.price = 12345;
	#endif
	//ha_sync(handle);
	//dump_tha_object_db(handle);
	//person1.laptop.lap_hw.HDsize = 256;
	//ha_sync(handle);	
	//person1.occupation = non_tha_emp;
	//ha_sync(handle);
	//dump_tha_object_db(handle);

#if 0
	
	ha_calloc(handle, person, person_t, 3);	
	person->age = 23;
	(person+2)->age = 34;	
	//dump_tha_object_db(handle);
	//ha_calloc(handle, emp, emp_t, 1);
	ha_sync(handle);
	//ha_calloc(handle, ptr, emp_t, 2);	
	//ha_calloc(handle, ptr2, person_t, 1);	
	//ha_sync(handle);
	//ha_calloc(handle, ptr2, person_t, 10);
#endif
#if 0
	ha_calloc(handle, ptr, person_t, 5);	
	ha_calloc(handle, ptr, person_t, 1);	
	//ha_calloc(handle, ptr, emp_t, 2);	
	ha_calloc(handle, ptr, person_t, 1);	
	ha_calloc(handle, ptr, lap_t, 10);
	 dump_tha_object_db(handle);
         ha_free(handle, ptr);
	 dump_tha_object_db(handle);
         ha_free(handle, ptr1);
	 dump_tha_object_db(handle);
         ha_free(handle, ptr2);
	 dump_tha_object_db(handle);
#endif
	//person1.age = 31;
	//mark_changed_objects(handle);
	//dump_tha_struct_db(handle);
	//dump_tha_object_db(handle);
	//clean_dirty_bit(handle);
	//dump_tha_object_db(handle);
	//int *arr = NULL, i = 0;
	//get_struct_hash_array(&person1, person_t, arr);
	ha_sync(handle);
	tha_acquire_active_state(NULL);
	scanf("\n");
	return 0;
}
