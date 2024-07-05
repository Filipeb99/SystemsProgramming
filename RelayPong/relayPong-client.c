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
#include <signal.h>
#include "pong.h"

/* Structs */
typedef struct server_info {
    int fd_sock;
    struct sockaddr_in server_addr;
} server_info;

typedef struct paddle_position_t {
    int x, y;
    int length;
} paddle_position_t;

/* Global variables */
WINDOW * my_win;
WINDOW * message_win;
game_state g_state;
ball_position_t ball;
paddle_position_t paddle;
static volatile sig_atomic_t quit = 0;
pthread_mutex_t mux = PTHREAD_MUTEX_INITIALIZER;

void new_paddle (paddle_position_t * paddle, int legth) {
    paddle->x = WINDOW_SIZE/2;
    paddle->y = WINDOW_SIZE-2;
    paddle->length = legth;
}

void draw_paddle (WINDOW *win, paddle_position_t * paddle, int delete) {
    int ch;
    if(delete)
        ch = '_';
    else
        ch = ' ';
        
    int start_x = paddle->x - paddle->length;
    int end_x = paddle->x + paddle->length;
    for (int x = start_x; x <= end_x; x++) {
        wmove(win, paddle->y, x);
        waddch(win,ch);
    }
    wrefresh(win);
}

void moove_paddle (paddle_position_t * paddle, int direction) {
    if (direction == KEY_UP && paddle->y != 1)
        paddle->y--;
    if (direction == KEY_DOWN && paddle->y != WINDOW_SIZE-2)
        paddle->y++;
    if (direction == KEY_LEFT && paddle->x - paddle->length != 1)
        paddle->x--;
    if (direction == KEY_RIGHT && (paddle->x + paddle->length) != WINDOW_SIZE-2)
        paddle->x++;
}

void place_ball_random (ball_position_t * ball) {
    ball->x = rand() % WINDOW_SIZE ;
    ball->y = rand() % WINDOW_SIZE ;
    ball->c = 'o';
    ball->up_hor_down = rand() % 3 -1; //  -1 up, 1 - down
    ball->left_ver_right = rand() % 3 -1 ; // 0 vertical, -1 left, 1 right
}

void moove_ball (ball_position_t * ball) {
    int next_x = ball->x + ball->left_ver_right;
    if(next_x == 0 || next_x == WINDOW_SIZE-1)
    {
        ball->up_hor_down = rand() % 3 -1 ;
        ball->left_ver_right *= -1;
        wrefresh(message_win);
     }
     else
	ball->x = next_x;
    
  
    int next_y = ball->y + ball->up_hor_down;
    if(next_y == 0 || next_y == WINDOW_SIZE-1)
    {
        ball->up_hor_down *= -1;
        ball->left_ver_right = rand() % 3 -1;
        wrefresh(message_win);
    }
    else
        ball->y = next_y;
}

void draw_ball (WINDOW *win, ball_position_t * ball, int draw) {
    int ch;
    if(draw)
        ch = ball->c;
    else
        ch = ' ';
    
    wmove(win, ball->y, ball->x);
    waddch(win,ch);
    wrefresh(win);
}

void * thread_function_msgs_recv (void * arg) {
	server_info serv = *(server_info *) arg;
	message_game m_received;
	int n_bytes;
	while (1) {
		n_bytes = recv(serv.fd_sock, &m_received, sizeof(message_game), 0);
		if (n_bytes != sizeof(message_game)) continue;
		// if we receive a Send_ball message from the server
		if(m_received.msg_type == 1)
		{
			pthread_mutex_lock(&mux); 
			g_state = BALL_CONTROLLING;
			draw_ball(my_win, &ball, true);
			draw_paddle(my_win, &paddle, true);
			mvwprintw(message_win, 1,1,"BALL CONTROLING STATE");
			mvwprintw(message_win, 2,1,"Use arrow keys to control");
			mvwprintw(message_win, 3,1,"paddle.");
			wrefresh(message_win);
			pthread_mutex_unlock(&mux); 
		}
		
		// if we receive a Release_ball message from the server
		if(m_received.msg_type == 2)
		{
			pthread_mutex_lock(&mux); 
			g_state = IDLE;
			draw_paddle(my_win, &paddle, false);
			wclear(message_win);
			box(message_win, 0 , 0);
			wrefresh(message_win);
			pthread_mutex_unlock(&mux); 
		}

		// if we receive a Move_ball message from the server
		if (m_received.msg_type == 3) 
		{
			pthread_mutex_lock(&mux);
			draw_ball(my_win, &ball, false);
			ball = m_received.ball;
			draw_ball(my_win, &ball, true);
			pthread_mutex_unlock(&mux);
		}
	}
	
	return NULL;
}

void * thread_function(void * arg) {
	server_info serv = *(server_info *) arg;
	message_game m;
	int n_bytes;
	while (1) {
        pthread_mutex_lock(&mux);
		if (g_state == BALL_CONTROLLING) {
			draw_ball(my_win, &ball, false);
			
			moove_ball(&ball);
			// add verification if paddle hit the ball and change ball movement (to symmetric)
			if ((paddle.y == ball.y) && (ball.x >= paddle.x - paddle.length) && (ball.x <= paddle.x + paddle.length)) {
				ball.up_hor_down *= -1;
				ball.left_ver_right *= -1;
				moove_ball(&ball);
			}
            
			draw_ball(my_win, &ball, true);

			// prepare and send Move_ball message to server
			m.msg_type = 3;
			m.ball = ball;
            
			n_bytes = sendto(serv.fd_sock, &m, sizeof(message_game), 0, (const struct sockaddr *)&serv.server_addr, sizeof(serv.server_addr));
        }
        pthread_mutex_unlock(&mux);
        sleep(1);
	}
	
	return NULL;
}

void intHandler(int a) {
	signal(a, SIG_IGN);
	quit = 1;
}

void * quit_func (void * arg) {
    server_info serv = *(server_info *) arg;
    message_game m;
    int n_bytes;
    
    while (1) {
        pthread_mutex_lock(&mux);
        if (quit) {
            m.msg_type = 4;
            if(g_state == BALL_CONTROLLING)
                m.controlling_ball = 1;
            else
                m.controlling_ball = 0;
            n_bytes = sendto(serv.fd_sock, &m, sizeof(message_game), 0, (const struct sockaddr *)&serv.server_addr, sizeof(serv.server_addr));
            break;
        }
        pthread_mutex_unlock(&mux);
    }
    pthread_mutex_unlock(&mux);
    close(serv.fd_sock);
    endwin();
    pthread_mutex_destroy(&mux);
    exit(0);
}

int main (int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: ./relay-pong-client <server IPv4 address>\n");
		exit(0);
	}
	
	unsigned long addr_ip = 0;
	// check if server address is valid
	if (inet_pton(AF_INET, argv[1], &addr_ip) != 1) {
		printf("Invalid IPv4 address!\n");
		exit(-1);
	}
	
	// create an Internet domain datagram socket for the client
	int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock_fd == -1) {
		perror("socket failed\n");
		exit(-1);
	}
	
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SOCK_PORT);
	inet_pton(AF_INET, argv[1], &server_addr.sin_addr);
	int n_bytes;
	g_state = IDLE; // define initial state
	
	if (pthread_mutex_init(&mux, NULL) != 0) {
        printf("pthread_mutex_init failed\n");
        return 1;
    }
	
	initscr();		    	/* Start curses mode 		*/
	cbreak();				/* Line buffering disabled	*/
	keypad(stdscr, TRUE);   /* We get F1, F2 etc..		*/
	noecho();			    /* Don't echo() while we do getch */

   	/* Ceates a window and draws a border */
	my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
	box(my_win, 0 , 0);	
	wrefresh(my_win);
	keypad(my_win, true);
	/* Creates a window and draws a border */
	message_win = newwin(6, WINDOW_SIZE+10, WINDOW_SIZE, 0);
	box(message_win, 0 , 0);	
	wrefresh(message_win);

	new_paddle(&paddle, PADLE_SIZE);
	place_ball_random(&ball);

	signal(SIGINT, intHandler);
    
    // Send connect message (Step A)
    message_game m;
    m.msg_type = 0;
    n_bytes = sendto(sock_fd, &m, sizeof(message_game), 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));
    
    // Create thread to handle and calculate ball movements and send fd and sockaddr of server
    pthread_t thread_id_ball;
    server_info *serv = (server_info*)malloc(sizeof(server_info));
    (*serv).fd_sock = sock_fd;
    (*serv).server_addr = server_addr;
    pthread_create(&thread_id_ball, NULL, thread_function, (void *) serv);
    
    // Create another thread to handle messages recv from server
    pthread_t thread_id_msg_recv;
    pthread_create(&thread_id_msg_recv, NULL, thread_function_msgs_recv, (void *) serv);
    
    pthread_t quit_thread;
    pthread_create(&quit_thread, NULL, quit_func, (void *) serv);

	int key = -1;
	while (key != 27) {
		key = wgetch(my_win);
		if (g_state == BALL_CONTROLLING) {
			pthread_mutex_lock(&mux);
			mvwprintw(message_win, 3,8," Key pressed: %c",key);
			wrefresh(message_win); 
			pthread_mutex_unlock(&mux);
			
			if (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN) {
				// Step K
				pthread_mutex_lock(&mux);
				draw_paddle(my_win, &paddle, false);
				moove_paddle (&paddle, key);
				draw_paddle(my_win, &paddle, true);
				pthread_mutex_unlock(&mux);	
			}      
		}
		
		if (key == 'q') quit = 1;
	}

	close(sock_fd);
	endwin();
	pthread_mutex_destroy(&mux);
	return 0;
}
