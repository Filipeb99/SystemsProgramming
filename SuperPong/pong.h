#define WINDOW_SIZE 20
#define PADLE_SIZE 2
#define SOCK_PORT 5005

typedef struct ball_position_t {
    int x, y;
    int up_hor_down; //  -1 up, 0 horizontal, 1 down
    int left_ver_right; //  -1 left, 0 vertical,1 right
    char c;
} ball_position_t;

typedef struct paddle_position_t {
    int x, y;
    int length;
    int score;
} paddle_position_t;

typedef struct message_game {
	ball_position_t ball;
    paddle_position_t paddles[10];
    int idx;
    int num_players;
} message_game;
