CC=gcc
CFLAGS=-g -Wall
LIBS=-lpthread 
OBJ=tha_db.o Utils.o tha_sock.o tha_sync.o serialize.o threadApi.o
SLIB=tha.so
STATICLIB=libtha.a
LL=DS/LinkedList/HALinkedListApi.o DS/LinkedList/LinkedListApi.o
TREE=DS/Trees/tree.o
Q=DS/Queue/Queue.o
STACK=DS/Stack/stack.o
DS_OBJ=${STACK} ${LL} ${Q} ${TREE} DS/ds_reg.o
TARGET:exe ${SLIB} ${STATICLIB}
%.o:%.c
	${CC} ${CFLAGS} -c -I . $<
${STACK}:DS/Stack/stack.c
	(cd DS/Stack; make)
${LL}:DS/LinkedList/HALinkedListApi.c DS/LinkedList/LinkedListApi.c
	(cd DS/LinkedList; make)
${TREE}:DS/Trees/tree.c
	(cd DS/Trees; make)
${Q}:DS/Queue/Queue.c
	(cd DS/Queue; make)
exe: main.o ${OBJ} ${DS_OBJ}
	${CC} ${CFLAGS} main.o ${OBJ} ${DS_OBJ} -o exe ${LIBS}
main.o:main.c
	${CC} ${CFLAGS} -c main.c -o main.o
DS/ds_reg.o:DS/ds_reg.c
	${CC} ${CFLAGS} -c DS/ds_reg.c -o DS/ds_reg.o
${SLIB}: ${OBJ} ${DS_OBJ}
	${CC} ${CFLAGS} ${OBJ} -shared -o ${SLIB} ${LIBS}
${STATICLIB}: ${OBJ} ${DS_OBJ}
	ar rs ${STATICLIB} ${OBJ} ${DS_OBJ}
clean:
	rm *.o
	rm exe
	rm ${SLIB}	
	(cd DS/LinkedList; make clean)
	(cd DS/Trees; make clean)
	(cd DS/Queue; make clean)
	(cd DS/Stack; make clean)
	rm DS/ds_reg.o
	rm ${STATICLIB}
