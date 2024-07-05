#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <ncurses.h>
#include <unistd.h>
#include <fcntl.h>  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>  
#include "pong.h"

// Structure used to store client information (node)
typedef struct list_client_info {
    struct sockaddr_in client_addr;
    struct list_client_info *next;
    int controlling_ball;
} list_client_info;

/* Global variables */
int num_clients = 0;
list_client_info *first_client = NULL;
pthread_mutex_t mux = PTHREAD_MUTEX_INITIALIZER;

void createinsertClientList (list_client_info **first_client, struct sockaddr_in client_address) {
	list_client_info *tmp = *first_client;
    list_client_info *new_client = (list_client_info *)malloc(sizeof(list_client_info));
    if (new_client == NULL) {
		printf("Memory allocation error!\n");
		exit(-1);
	}

	new_client->client_addr = client_address;
	new_client->controlling_ball = 0;

    if (*first_client == NULL) { 
        *first_client = new_client;
        new_client->next = NULL;
    } else {
        while(tmp->next != NULL)
        	tmp = tmp->next;
        tmp->next = new_client;
        new_client->next = NULL;
    }
}

void deleteClientFromList (list_client_info **first_client, struct sockaddr_in client_address) {
     list_client_info *tmp = *first_client, *prev;
 
    if (tmp != NULL && (tmp->client_addr.sin_port == client_address.sin_port)) {
        *first_client = tmp->next; 
        free(tmp);
        return;
    }
    
    while (tmp != NULL && (tmp->client_addr.sin_port != client_address.sin_port)) {
        prev = tmp;
        tmp = tmp->next;
    }
 
    // If key was not present in linked list
    if (tmp == NULL) return;
 
    // Unlink the node from linked list
    prev->next = tmp->next;
    free(tmp);
}

void * thread_function(void * arg) {
	int sock_fd = *(int *) arg;
	list_client_info *aux, *tmp;
	message_game m;
	socklen_t client_addr_size = sizeof(struct sockaddr_in);
	int n_bytes, idx, j, flag_sent_to_dif;
	
	while(1) {
		sleep(10);
		if(first_client != NULL) {
			printf("Release ball!\n");
			// Prepare release_ball message and send to client controlling the ball (Step D)
			m.msg_type = 2;
			aux = first_client;
			while (aux != NULL) {
				if (aux->controlling_ball == 1) {
					n_bytes = sendto(sock_fd, &m, sizeof(m), 0, (const struct sockaddr *) &aux->client_addr, client_addr_size);
					break;
				}
				aux = aux->next;
			}
			
			// Prepare send_ball message and send to an existing client (Step E)
			m.msg_type = 1;
			if (num_clients == 1)
				n_bytes = sendto(sock_fd, &m, sizeof(m), 0, (const struct sockaddr *) &first_client->client_addr, client_addr_size);
			else {
				flag_sent_to_dif = 0;
				while (flag_sent_to_dif == 0) {
					aux = first_client;
					j = 0;
					idx = rand()%num_clients;
					while (aux != NULL) {
						// Send to a different client
						if (j == idx && aux->controlling_ball == 0) {
							n_bytes = sendto(sock_fd, &m, sizeof(m), 0, (const struct sockaddr *) &aux->client_addr, client_addr_size);
							flag_sent_to_dif = 1;
							tmp = first_client;
							while(tmp != NULL) {
								if(tmp->controlling_ball == 1)
								{
									tmp->controlling_ball = 0;
									break;
								}
								tmp = tmp->next;
							}
							aux->controlling_ball = 1;
							break;
						}
						j++;
						aux = aux->next;
					}
				}
			}	
		}
	}
	
	return NULL;
}

int main (void) {	    
	// Create an Internet domain datagram socket for the server
    int sock_fd;
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd == -1) {
	    perror("socket: ");
	    exit(-1);
    }
    
    struct sockaddr_in local_addr;
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(SOCK_PORT);
	local_addr.sin_addr.s_addr = INADDR_ANY;

	int err = bind(sock_fd, (struct sockaddr *)&local_addr, sizeof(local_addr));
	if (err == -1) {
		perror("bind");
		exit(-1);
	}

    int n_bytes, j, idx;
    message_game m;
    list_client_info *aux;
	srand(time(NULL));
	
    struct sockaddr_in client_addr;
    socklen_t client_addr_size = sizeof(struct sockaddr_in);
    
    pthread_mutex_init(&mux, NULL);
    
    // Create thread that every 10 seconds will choose a new client to control the ball
    pthread_t thread_id_serv;
    pthread_create(&thread_id_serv, NULL, thread_function, &sock_fd);

    while (1) {
        n_bytes = recvfrom(sock_fd, &m, sizeof(message_game), 0, (struct sockaddr *)&client_addr, &client_addr_size);
        if (n_bytes != sizeof(message_game)) continue;
        
		// If we received a connect message from a new client
		if (m.msg_type == 0) {
			printf("Received Connect message from the client!\n");
			// Insert new client into list (Step B)
            pthread_mutex_lock(&mux);
			createinsertClientList(&first_client,client_addr); 
			num_clients++;
			// Prepare send_ball message and send it to the 1st client (Step C)
			if (num_clients == 1) {
				m.msg_type = 1;
				n_bytes = sendto(sock_fd, &m, sizeof(m), 0, (const struct sockaddr *) &client_addr, client_addr_size);
				printf("Sent 1st ball\n");
			}
            pthread_mutex_unlock(&mux);
		}
				
		// If we received a move_ball message from a client
		if (m.msg_type == 3) {
			printf("Received Move_ball message from the client!\n");
			// Step L
            pthread_mutex_lock(&mux);
			aux = first_client;
			while (aux != NULL) {
				if(aux->client_addr.sin_port != client_addr.sin_port)
					n_bytes = sendto(sock_fd, &m, sizeof(m), 0, (const struct sockaddr *) &aux->client_addr, client_addr_size);
				else
					aux->controlling_ball = 1;
				aux = aux->next;
			}
            pthread_mutex_unlock(&mux);
		}
		
		// if we received a Disconnect message from a client
		if (m.msg_type == 4) {
			printf("Received Disconnect message from the client!\n");
            pthread_mutex_lock(&mux);
			deleteClientFromList(&first_client,client_addr); // delete client that disconnected from list	
			num_clients--;
			// If the client that disconnected was controlling the ball and there are other clients,
            // send the send_ball message to another client
			if (m.controlling_ball == 1) {
				if (first_client != NULL) {
					m.msg_type = 1;
					aux = first_client;
					j = 0;
					idx = rand()%num_clients;
					while (aux != NULL) {
						if (j == idx) {
							n_bytes = sendto(sock_fd, &m, sizeof(m), 0, (const struct sockaddr *) &aux->client_addr, client_addr_size);
							break;
						}
						j++;
						aux = aux->next;
					}
				}
			}
            pthread_mutex_unlock(&mux);
		}
    }
	
    pthread_mutex_destroy(&mux);
	close(sock_fd);
	return 0;
}
