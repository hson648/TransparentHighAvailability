# TransparentHighAvailability
Transparent High Availability project - sync application state to redundant machine for High Availability 

1. This is a Project that implements the Transparent High Availability in C.
2. Started on 7 Oct 2016
3. completed on 28 Oct 2016
4. ToDo : Support for Arrays of pointers, Implementing Checkpoints
5. Arrays of pointers and arrays of internal objects is not yet supported.


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
