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
#include <signal.h>
#include <pthread.h>
#include "pong.h"

/* Global Variables */
WINDOW * my_win, * message_win;
static volatile sig_atomic_t quit = 0;

void draw_paddle (WINDOW *win, paddle_position_t * paddle, int my) {
    int ch = '_';
    if (my) ch = '=';
        
    int start_x = paddle->x - paddle->length;
    int end_x = paddle->x + paddle->length;
    for (int x = start_x; x <= end_x; x++) {
        wmove(win, paddle->y, x);
        waddch(win,ch);
    }
}

void draw_ball (WINDOW *win, ball_position_t * ball) {
    wmove(win, ball->y, ball->x);
    waddch(win, ball->c);
}

void intHandler (int a) {
	(void)a;
	quit = 1;
}

void * read_func (void *arg) {
    int sock_fd = *(int *)arg;
    
    message_game m;
    int n_bytes;
    
    while (1) {
        n_bytes = read(sock_fd, &m, sizeof(message_game));
        if (n_bytes <= 0) {
            close(sock_fd);
            endwin();
            perror("server unreachable\n");
            exit(EXIT_FAILURE);
        } else if (n_bytes == sizeof(message_game)) {
            // clear windows to write new info
            wclear(my_win);
            wclear(message_win);
            box(my_win, 0, 0);
            box(message_win, 0, 0);
            
            for (int i = 0; i < m.num_players; i++) {
                // draw new paddles and scores
                if (i == m.idx) {
                    draw_paddle(my_win, &(m.paddles[i]), true);
                    mvwprintw(message_win, i+1, 1,"P%d - %d <---", i+1, m.paddles[i].score);
                } else {
                    draw_paddle(my_win, &(m.paddles[i]), false);
                    mvwprintw(message_win, i+1, 1,"P%d - %d", i+1, m.paddles[i].score);
                }
            }
            
            // draw ball
            draw_ball(my_win, &(m.ball));
            
            wrefresh(my_win);
            wrefresh(message_win);
        }
    }
    
    pthread_exit(NULL);
}

void * quit_func (void * arg) {
    int sock_fd = *(int *)arg;
    while (1) {
        if (quit) {
            close(sock_fd);
            endwin();
            exit(0);
        }
    }
}
    
int main (int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: ./super-pong-client <server IPv4 address>\n");
		exit(0);
	}
	
	// Create an Internet domain socket for the client
    int sock_fd;
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
	
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SOCK_PORT);
    // Convert IPv4 address from text to binary
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        perror("invalid address");
        exit(EXIT_FAILURE);
    }
	
	initscr();		    	/* Start curses mode 		*/
	cbreak();				/* Line buffering disabled	*/
	keypad(stdscr, TRUE);   /* We get F1, F2 etc..		*/
	noecho();			    /* Don't echo() while we do getch */

   	/* Creates a window and draws a border */
	my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
	box(my_win, 0 , 0);	
	wrefresh(my_win);
	keypad(my_win, true);
    
	/* Creates a window and draws a border */
	message_win = newwin(12, WINDOW_SIZE+10, WINDOW_SIZE, 0);
	box(message_win, 0 , 0);	
	wrefresh(message_win);
	
    // Handle CTRL-C for smooth exit
	struct sigaction act;
	act.sa_handler = intHandler;
	sigaction(SIGINT, &act, NULL);
    
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connection failed\n");
        exit(EXIT_FAILURE);
    }
    
    pthread_t read_thread;
    pthread_create(&read_thread, NULL, read_func, &sock_fd);
    
    pthread_t quit_thread;
    pthread_create(&quit_thread, NULL, quit_func, &sock_fd);
    
    int key = -1;
    while (key != 27) {
        if (quit) {
            close(sock_fd);
            endwin();
            exit(0);
        }
        
        // Get and process key
        key = wgetch(my_win);
        if (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN) write(sock_fd, &key, sizeof(key));
        else if (key == 'q') quit = 1;
    }
    
    close(sock_fd);
    endwin();
    exit(0);
}
