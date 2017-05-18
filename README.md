# TransparentHighAvailability
Transparent High Availability project - sync application state to redundant machine for High Availability 

Abstract

This is the High Availability Project, design to sync the application heap state to standby/redundant machine via sockets. The machines are names as Active and standby machines respectively. Any operation done by the user on Active which results in Heap Memory state change of the application is synced to the standby state. The goal is to maintain the Active and standby application heap state the mirror images of each other, so that, in the event when Active maachine collapses, the application can resume on standby from the same state. HA Infra has been designed that only changes data structure are synced to standby machine. Design follow Registration model - Developer need to register new Data structure to be HA supported with the infra, and infra will take care ti sync these data structure whenever they are updated. Develoiper need not write any Serialize/deserialize code for new structures to be syncd. In Addition, infa supports Memory leak detection as well as checheckpointing of the application state. User can save and reload application state from persistent storage as well. 


1. This is a Project that implements the Transparent High Availability in C.
2. Started on 7 Oct 2016
3. completed on 28 Oct 2016
4. ToDo : Support for Arrays of pointers
5. Arrays of pointers is not yet supported
6. Checkpointing is Supported.


/* Run this project*/

Requirement:

1. You should have gcc compiler ( this project was build with version gcc version 4.8.4)
2. run make in THA/ACTIVE. This will compile the THA library
3. goto THA/DemoApp/ACTIVE, run make again. This will create the executable for Demo App to be run on Active
4. copy all files from THA/DemoApp/ACTIVE to THA/DemoApp/STANDBY
5. in THA/DemoApp/STANDBY dir, open app_main.c
6. in main(), change the value of the variable from 1 to 0 for I_AM_ACTIVE_CP. Save and close the file
7. run make in THA/DemoApp/STANDBY. This will create executable to be run on standby machine
8. run THA/DemoApp/STANDBY/exe first, then run THA/DemoApp/ACTIVE/exe
9. Now enjoy the program, you will see the objects being synced to standby whenever you choose option 11 in main menu.
