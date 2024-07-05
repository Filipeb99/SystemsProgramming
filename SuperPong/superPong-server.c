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
	struct list_client_info *next;
	paddle_position_t paddle;
    int sock;
	int idx;
} list_client_info;

/* Global Variables */
int num_clients = 0;
ball_position_t ball;
list_client_info *head = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Start of ball functions */
void place_ball_random (void) {
	ball.y = rand() % (WINDOW_SIZE-2);
	ball.y += 1;
	ball.x = rand() % (WINDOW_SIZE-6);
	ball.x += 3;
    ball.c = 'o';
    ball.up_hor_down = rand() % 3 -1; //  -1 up, 1 - down
    ball.left_ver_right = rand() % 3 -1 ; // 0 vertical, -1 left, 1 right
}

void moove_ball (void) {
    int next_x = ball.x + ball.left_ver_right;
    if (next_x == 0 || next_x == WINDOW_SIZE-1) {
        ball.up_hor_down = rand() % 3 - 1;
        ball.left_ver_right *= -1;
     } else ball.x = next_x;
	
    int next_y = ball.y + ball.up_hor_down;
    if (next_y == 0 || next_y == WINDOW_SIZE-1) {
        ball.up_hor_down *= -1;
        ball.left_ver_right = rand() % 3 - 1;
    } else ball.y = next_y;
}

void ball_moove (void) {
    moove_ball();
    for (list_client_info *aux = head; aux != NULL; aux = aux->next) {
        int x0 = aux->paddle.x - aux->paddle.length, x1 = aux->paddle.x + aux->paddle.length;
        if ((aux->paddle.y == ball.y) && (ball.x >= x0) && (ball.x <= x1)) {
            ball.left_ver_right *= -1;
            ball.up_hor_down *= -1;
            moove_ball();
            aux->paddle.score++;
        }
    }
}
/* End of ball functions */

/* Start of paddle functions */
int paddle_colision (paddle_position_t p1, paddle_position_t p2) {
    if (p1.y == p2.y) {
        int x11 = p1.x - p1.length, x12 = p1.x + p1.length;
        int x21 = p2.x - p2.length, x22 = p2.x + p2.length;
        if ( (x11 >= x21 && x11 <= x22) || (x12 >= x21 && x12 <= x22) ) {
            return 1;
        } else {
            if ((x11 <= 0) || (x12 > WINDOW_SIZE-2)) return 1;
            else return 0;
        }
    } else {
        return 0;
    }
}

void paddle_generator (paddle_position_t *paddle) {
    int colision = 0;
    paddle->length = PADLE_SIZE;
    paddle->score = 0;
    
    do {
        colision = 0;
        paddle->y = rand() % (WINDOW_SIZE-2);
        paddle->y += 1;
        paddle->x = rand() % (WINDOW_SIZE-6);
        paddle->x += 3;
        for (list_client_info *aux = head; aux != NULL; aux = aux->next)
            if (paddle_colision(*paddle, aux->paddle)) colision = 1;
    } while (colision);
}

void moove_paddle (paddle_position_t * paddle, int direction) {
    if (direction == KEY_UP)
        if(paddle->y != 1) paddle->y--;
        
    if (direction == KEY_DOWN)
        if(paddle->y != WINDOW_SIZE-2) paddle->y++;
    
    if (direction == KEY_LEFT)
        if(paddle->x - paddle->length != 1) paddle->x--;
    
    if (direction == KEY_RIGHT)
        if(paddle->x + paddle->length != WINDOW_SIZE-2) paddle->x++;
}

void paddle_moove (paddle_position_t *paddle, int direction, int idx) {
    paddle_position_t tmp = *paddle;
    moove_paddle(&tmp, direction);
    for (list_client_info *aux = head; aux != NULL; aux = aux->next)
        if (paddle_colision(tmp, aux->paddle) && (idx != aux->idx)) return;
    moove_paddle(paddle, direction);
    
    int x0 = paddle->x - paddle->length, x1 = paddle->x + paddle->length;
    if ((paddle->y == ball.y) && (ball.x >= x0) && (ball.x <= x1)) {
        if (direction == KEY_UP) ball.up_hor_down = -1;
        if (direction == KEY_DOWN) ball.up_hor_down = 1;
        if (direction == KEY_LEFT) ball.left_ver_right = -1;
        if (direction == KEY_RIGHT) ball.left_ver_right = 1;
        moove_ball();
        paddle->score++;
    }
}
/* End of paddle functions */

/* Start of list functions */
list_client_info * insertClientList (int sock) {
    list_client_info *aux = head;
    list_client_info *new_client = (list_client_info *)malloc(sizeof(list_client_info));
    if (new_client == NULL) {
        printf("Memory allocation error!\n");
        exit(-1);
    }
    
    paddle_position_t paddle;
    paddle_generator(&paddle);
    new_client->paddle = paddle;
    new_client->sock = sock;
    new_client->idx = num_clients;

    if (head == NULL) {
        head = new_client;
        new_client->next = NULL;
    } else {
        while (aux->next != NULL) aux = aux->next;
        aux->next = new_client;
        new_client->next = NULL;
    }
    return new_client;
}

void deleteClientFromList (int idx) {
     list_client_info *aux = head, *prev;
 
    if (aux != NULL && (aux->idx == idx)) {
        head = aux->next;
        close(aux->sock);
        free(aux);
        aux = head;
        while (aux != NULL) {
            aux->idx--;
            aux = aux->next;
        }
        return;
    }
    
    while (aux != NULL && (aux->idx != idx)) {
        prev = aux;
        aux = aux->next;
    }
 
    // key was not present in linked list
    if (aux == NULL) return;
 
    // Unlink the node from linked list
    prev->next = aux->next;
    close(aux->sock);
    free(aux);
    aux = prev->next;
    while (aux != NULL) {
        aux->idx--;
        aux = aux->next;
    }
}
/* End of list functions */

/* Start of thread functions */
void * ball_control (void *arg) {
    message_game m;
    while (1) {
        sleep(1);
        pthread_mutex_lock(&mutex);
        ball_moove();
        m.num_players = num_clients;
        m.ball = ball;
        for (list_client_info *aux = head; aux != NULL; aux = aux->next) m.paddles[aux->idx] = aux->paddle;
        
        for (list_client_info *aux = head; aux != NULL; aux = aux->next) {
            m.idx = aux->idx;
            write(aux->sock, &m, sizeof(message_game));
        }
        pthread_mutex_unlock(&mutex);
    }
    pthread_exit(NULL);
}

void * client_processor (void *arg) {
    list_client_info *client = (list_client_info *)arg;
    
    message_game m;
    int direction, n_bytes;
    
    while (1) {
        n_bytes = read(client->sock, &direction, sizeof(int));
        if (n_bytes <= 0) {
            pthread_mutex_lock(&mutex);
            deleteClientFromList(client->idx);
            m.num_players = --num_clients;
            m.ball = ball;
            for (list_client_info *aux = head; aux != NULL; aux = aux->next) m.paddles[aux->idx] = aux->paddle;
            
            for (list_client_info *aux = head; aux != NULL; aux = aux->next) {
                m.idx = aux->idx;
                write(aux->sock, &m, sizeof(message_game));
            }
            pthread_mutex_unlock(&mutex);
            printf("About to exit\n");
            pthread_exit(NULL);
            printf("Exited\n");
        } else if (n_bytes == sizeof(int)) {
            pthread_mutex_lock(&mutex);
            paddle_moove(&(client->paddle), direction, client->idx);
            m.num_players = num_clients;
            m.ball = ball;
            for (list_client_info *aux = head; aux != NULL; aux = aux->next) m.paddles[aux->idx] = aux->paddle;
            
            for (list_client_info *aux = head; aux != NULL; aux = aux->next) {
                m.idx = aux->idx;
                write(aux->sock, &m, sizeof(message_game));
            }
            pthread_mutex_unlock(&mutex);
        }
    }
}
/* End of thread functions */

int main (void) {	    
	// Create an internet domain stream socket for the server
    int sock_fd;
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed\n");
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(SOCK_PORT);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    
    // Set socket options, reuse of address in bind() is allowed
    int opt = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed\n");
        exit(EXIT_FAILURE);
    }
    
    if (bind(sock_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("bind failed\n");
        exit(EXIT_FAILURE);
    }
    
    // Up to 5 clients on waiting list
    if (listen(sock_fd, 20) < 0) {
        perror("listen failed\n");
        exit(EXIT_FAILURE);
    }
    
    pthread_mutex_init(&mutex, NULL);
    
    //set rand seed
    srand(time(NULL));
    
	// create ball
	place_ball_random();
    pthread_t ball_thread;
    pthread_create(&ball_thread, NULL, ball_control, NULL);

    int new_socket, addrlen = sizeof(local_addr);
    
    message_game m;
    
    pthread_t *client_thread = NULL;
    
    list_client_info *last = NULL;
    
    while (1) {
        if ((new_socket = accept(sock_fd, (struct sockaddr *)&local_addr, (socklen_t*)&addrlen)) < 0) {
            perror("accept failed\n");
            exit(EXIT_FAILURE);
        }
        
        pthread_mutex_lock(&mutex);
        if (num_clients < 10) {
            m.ball = ball;
            last = insertClientList(new_socket);
            m.num_players = ++num_clients;
            
            for (list_client_info *aux = head; aux != NULL; aux = aux->next) m.paddles[aux->idx] = aux->paddle;
                    
            for (list_client_info *aux = head; aux != NULL; aux = aux->next) {
                m.idx = aux->idx;
                write(aux->sock, &m, sizeof(message_game));
            }
            
            client_thread = (pthread_t *)malloc(sizeof(pthread_t));
            pthread_create(client_thread, NULL, client_processor, last);
        } else {
            printf("Too many players, wait until one leaves\n");
            close(new_socket);
        }
        
        pthread_mutex_unlock(&mutex);
    }
    
	close(sock_fd);
    pthread_mutex_destroy(&mutex);
	return 0;
}
