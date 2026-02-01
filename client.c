#include "common.h"

int msg_id;
int my_player_id = -1; 
int my_msg_type = -1;

char last_server_msg[MAX_TEXT];
char input_buffer[256];
int input_pos = 0;

struct termios orig_termios;

void write_stdout(const char* str) {
    write(STDOUT_FILENO, str, strlen(str));
}

void reset_terminal_mode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

void set_conio_mode() {
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));
    atexit(reset_terminal_mode);
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
}

void redraw() {
    char out_buf[2048];
    char temp_buf[512];
    
    strcpy(out_buf, "\033[2J\033[H"); 

    if (strlen(last_server_msg) > 0) {
        sprintf(temp_buf, "GRACZ %d\n", my_player_id + 1);
        strcat(out_buf, temp_buf);
        strcat(out_buf, "------------------------------------------------\n");
        strcat(out_buf, last_server_msg);
        strcat(out_buf, "\n------------------------------------------------\n");
    } else {
        strcat(out_buf, "Oczekiwanie na dane z serwera...\n");
    }

    strcat(out_buf, "\nKOMENDY: TRAIN [0-3] [ilosc] | ATTACK [L] [H] [C]\n");
    strcat(out_buf, "(0-Lekka, 1-Ciezka, 2-Jazda, 3-Robotnik)\n");
    
    strcat(out_buf, "\n> ");
    strcat(out_buf, input_buffer);

    write_stdout(out_buf);
}

void handle_input() {
    char c;
    if (read(STDIN_FILENO, &c, 1) > 0) {
        if (c == '\n') {
            Message msg;
            msg.type = 1; 
            msg.source_id = getpid();
            msg.args[0] = my_player_id;
            int valid = 0;

            char cmd_str[10];
            int a, b, c_arg;

            if (sscanf(input_buffer, "%s", cmd_str) >= 1) {
                if (strcmp(cmd_str, "TRAIN") == 0) {
                    if (sscanf(input_buffer, "%*s %d %d", &a, &b) == 2) {
                        msg.cmd = CMD_TRAIN;
                        msg.args[1] = a; 
                        msg.args[2] = b; 
                        valid = 1;
                    }
                } else if (strcmp(cmd_str, "ATTACK") == 0) {
                    if (sscanf(input_buffer, "%*s %d %d %d", &a, &b, &c_arg) == 3) {
                        msg.cmd = CMD_ATTACK;
                        msg.args[1] = a; 
                        msg.args[2] = b; 
                        msg.args[3] = c_arg; 
                        valid = 1;
                    }
                }
            }

            if (valid) {
                msgsnd(msg_id, &msg, sizeof(Message)-sizeof(long), 0);
            }

            memset(input_buffer, 0, 256);
            input_pos = 0;
            redraw();

        } else if (c == 127 || c == 8) {
            if (input_pos > 0) {
                input_buffer[--input_pos] = '\0';
                redraw();
            }
        } else if (c >= 32 && c <= 126) {
            if (input_pos < 255) {
                input_buffer[input_pos++] = c;
                input_buffer[input_pos] = '\0';
                redraw();
            }
        }
    }
}

int main() {
    msg_id = msgget(KEY_MSG, 0666);
    if (msg_id == -1) {
        const char *err = "Brak serwera\n";
        write(STDERR_FILENO, err, strlen(err));
        return 1;
    }

    srand(time(NULL) ^ getpid());
    int temp_key = rand() % 1000 + 50;
    
    Message msg;
    msg.type = 1;
    msg.cmd = CMD_LOGIN;
    msg.source_id = temp_key;
    msgsnd(msg_id, &msg, sizeof(Message)-sizeof(long), 0);

    msgrcv(msg_id, &msg, sizeof(Message)-sizeof(long), temp_key, 0);
    my_player_id = msg.args[0];
    my_msg_type = 10 + my_player_id;

    set_conio_mode();
    memset(last_server_msg, 0, MAX_TEXT);
    memset(input_buffer, 0, 256);

    redraw();

    while(1) {
        if (msgrcv(msg_id, &msg, sizeof(Message)-sizeof(long), my_msg_type, IPC_NOWAIT) != -1) {
            if (msg.cmd == CMD_UPDATE) {
                strcpy(last_server_msg, msg.text);
                redraw();
            } else if (msg.cmd == CMD_RESULT) {
                reset_terminal_mode();
                char end_buf[512];
                sprintf(end_buf, "\033[2J\033[H\n================================\nKONIEC GRY: %s\n================================\n", msg.text);
                write_stdout(end_buf);
                exit(0);
            }
        }

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10000;

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);

        int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        if (ret > 0) {
            handle_input();
        }

        usleep(10000); 
    }
    return 0;
}