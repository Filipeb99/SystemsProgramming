#define WINDOW_SIZE 20
#define PADLE_SIZE 2
#define SOCK_PORT 5000

// Simple state machine for the pong game
typedef enum game_state {BALL_CONTROLLING, IDLE} game_state;

typedef struct ball_position_t
{
    int x, y;
    int up_hor_down; //  -1 up, 0 horizontal, 1 down
    int left_ver_right; //  -1 left, 0 vertical,1 right
    char c;
} ball_position_t;



typedef struct message_game
{
	int msg_type; // 0 - Connect; 1 - Send_ball; 2 - Release_ball; 3 - Move_ball; 4 - Disconnect;
	ball_position_t ball;
	int controlling_ball;
} message_game;
