#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <memory.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <assert.h>
#include "tha_sync.h"
#include "tha_db.h"
#include "threadApi.h"
#include <arpa/inet.h>

#define DEST_PORT	2000
#define LISTENING_PORT  2000

typedef struct sock_arg{
        int sockfd;
        struct sockaddr_in dest_addr; 
} sock_arg_t;

static sock_arg_t gsock_send;
static sock_arg_t gsock_recv;

extern int I_AM_ACTIVE_CP;
extern tha_handle_t *handle;

char enable_checkpoint = CHECKPOINT_DISABLE; 
FILE * checkpoint_file = NULL;


int
tha_sync_msg_to_standby(char *msg, int size){
	int rc = SUCCESS;

	rc = sendto(gsock_send.sockfd, msg , size, 0,
		(struct sockaddr *)&gsock_send.dest_addr, sizeof(struct sockaddr));
	printf("%s(): %d bytes sent\n", __FUNCTION__, rc);
	return rc;
}


int
state_sync_start(tha_handle_t *handle, char *msg, int size){
	if(msg){
		if(enable_checkpoint == CHECKPOINT_ENABLE){
			fwrite(msg, size, 1, checkpoint_file);
			return 0;
		}
		return tha_sync_msg_to_standby(msg, size);
	}
	else 
		return tha_sync_object_db(handle);
}


int
ha_sync(tha_handle_t *handle){
	if(I_AM_ACTIVE_CP == 0) return SUCCESS;
	return state_sync_start(handle, NULL, 0);
}

int
ha_incremental_sync(tha_handle_t *handle, char *msg, int size){
	return state_sync_start(handle, msg, size);	
}

int
init_tha_sending_socket(tha_handle_t *handle){
	int sockfd = 0, rc = SUCCESS;
	struct sockaddr_in dest;
	dest.sin_family = AF_INET;
	dest.sin_port = DEST_PORT;
	struct hostent *host = (struct hostent *)gethostbyname(handle->standby_ip);	
	dest.sin_addr = *((struct in_addr *)host->h_addr);

	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sockfd == -1){
		printf("%s() : THA  socket creation failed\n", __FUNCTION__);
		return -1;
	}
	gsock_send.sockfd = sockfd;
        memcpy(&gsock_send.dest_addr, &dest, sizeof(struct sockaddr_in));	
	return rc;
}


static void
process_sock_msg(tha_handle_t *handle, char *recv_msg, int len){
	de_serialize_buffer(handle, recv_msg , len);
}

void*
recv_sock_msg(void *handle){
	int tha_sock_fd = 0, len, addr_len;
        char rec_buff[1024*1024];
        struct sockaddr_in server_addr,
                           client_addr;

        if ((tha_sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP )) == -1){
                printf("%s(): Error : socket creation failed\n", __FUNCTION__);
                exit(1);
        }

        server_addr.sin_family      = AF_INET;
        server_addr.sin_port 	    = LISTENING_PORT;
        server_addr.sin_addr.s_addr = INADDR_ANY;

        addr_len = sizeof(struct sockaddr);
        if (bind(tha_sock_fd,(struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)
        {
            printf("%s() : Error : socket bind failed\n", __FUNCTION__);
            exit(1);
        }
	gsock_recv.sockfd = tha_sock_fd;
	memcpy(&gsock_recv.dest_addr, &server_addr, sizeof(struct sockaddr_in));
READ:
        len = recvfrom(tha_sock_fd, rec_buff, 1024*10, 0, (struct sockaddr *)&client_addr, (socklen_t *)&addr_len);

	process_sock_msg((tha_handle_t*)handle, rec_buff, len);
	
	goto READ;

	return NULL;
}


void
init_tha_listening_thread(tha_handle_t *handle){
	 _pthread_t sock_recv_thread[1];
	 int DETACHED =0;
     	 pthread_init(&sock_recv_thread[0], 0 , DETACHED);
         pthread_create(&(sock_recv_thread[0].pthread_handle), 
			&sock_recv_thread[0].attr, recv_sock_msg, (void *)handle);
	 pthread_join(sock_recv_thread[0].pthread_handle, NULL);
}

void
wait_for_interrupt(){
	char read_buff[16];
	fd_set readfds;
	while (1) {
	puts("\n-----------------------------------");
        puts("   1. dump_tha_object_db");
        puts("   2. dump structure db");
	puts("   3. hafailover");
	puts("   4. exit");
	puts("   5. clean application state\n");
        puts("-----------------------------------\n");
	printf("choice = (1-5) : ");
		FD_ZERO(&readfds);
		FD_SET(0,&readfds);
		select(1,  &readfds, NULL, NULL, NULL);
		if (FD_ISSET(0, &readfds)){
			if(fgets(read_buff, 16, stdin) == NULL){
				printf("%s() : Error : error occurred in reading from stdin\n", __FUNCTION__);
				continue;
			}

			int choice;
			choice = atoi(read_buff);
			switch(choice){
				case 1:
					dump_tha_object_db(handle);
					break;
				case 2:
					dump_tha_struct_db(handle);
					break;
				case 3:
					printf("performing hafailover\n");
					break;
				case 4:
					exit(0);
				case 5:
					printf("Clean Application State\n");
					tha_clean_application_state(handle);
					break;
				default: ;
			}	
		}
		continue;
	}
}

typedef void (*cotrol_fn)();

void
tha_acquire_standby_state(cotrol_fn fn){
	printf("Taking Standby Role\n");
	if(fn == NULL)
		wait_for_interrupt();
	fn();	
}

void
tha_acquire_active_state(cotrol_fn fn){
	printf("Taking Active Role\n");
	if(fn == NULL)
		wait_for_interrupt();
	fn();
}

