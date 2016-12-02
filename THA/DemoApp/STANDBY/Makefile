TARGET:exe
INCLUDES=-I .
exe:app_main.o app_fn_db.o	
	gcc -g app_main.o app_fn_db.o -o exe -L ./../../ACTIVE/ -ltha -lpthread
app_fn_db.o:app_fn_db.c
	gcc -g -c ${INCLUDES} app_fn_db.c -o app_fn_db.o
app_main.o:app_main.c
	gcc -g -c ${INCLUDES} app_main.c -o app_main.o
clean:
	rm *.o
	rm exe
