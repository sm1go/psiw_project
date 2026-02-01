#include "common.h"

int msg_id;
int my_player_id = -1; 
int my_msg_type = -1;

void print_line(int line_num, const char* text) {
    printf("\033[%d;1H", line_num);
    printf("\033[K");
    printf("%s", text);
}

void receiver_loop() {
    Message msg;
    while(1) {
        if (msgrcv(msg_id, &msg, sizeof(Message)-sizeof(long), my_msg_type, 0) == -1) {
            exit(1);
        }
        
        if (msg.cmd == CMD_UPDATE) {
            printf("\033[s\033[?25l"); 

            char buffer[MAX_TEXT];
            strcpy(buffer, msg.text);
            
            char* line = strtok(buffer, "\n");
            int row = 2;
            
            printf("\033[1;1H\033[KGRACZ %d", my_player_id + 1);

            while(line != NULL) {
                print_line(row++, line);
                line = strtok(NULL, "\n");
            }
            
            print_line(row, "------------------------------------------------");
            print_line(row+1, "KOMENDY: TRAIN [0-3] [ilosc] | ATTACK [L] [H] [C]");
            print_line(row+2, "(0-Lekka, 1-Ciezka, 2-Jazda, 3-Robotnik)");

            printf("\033[u\033[?25h");
            fflush(stdout);

        } else if (msg.cmd == CMD_RESULT) {
            printf("\033[2J\033[H");
            printf("\n\n================================\n");
            printf("KONIEC GRY: %s\n", msg.text);
            printf("================================\n\n");
            kill(getppid(), SIGKILL);
            exit(0);
        }
    }
}

int main() {
    msg_id = msgget(KEY_MSG, 0666);
    if (msg_id == -1) {
        perror("Brak serwera");
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

    printf("\033[2J");
    for(int i=0; i<10; i++) printf("\n"); 
    
    printf("Zalogowano jako Gracz %d\n", my_player_id + 1);

    if (fork() == 0) {
        receiver_loop();
    } else {
        char buffer[100];
        while(1) {
            printf("\033[K> "); 
            fflush(stdout);
            
            if (fgets(buffer, 100, stdin) == NULL) continue;
            buffer[strcspn(buffer, "\n")] = 0;

            printf("\033[1A\033[K");

            char cmd_str[10];
            int a, b, c;
            
            if (sscanf(buffer, "%s", cmd_str) >= 1) {
                msg.type = 1; 
                msg.source_id = getpid();
                msg.args[0] = my_player_id;
                int valid = 0;

                if (strcmp(cmd_str, "TRAIN") == 0) {
                    if (sscanf(buffer, "%*s %d %d", &a, &b) == 2) {
                        msg.cmd = CMD_TRAIN;
                        msg.args[1] = a; 
                        msg.args[2] = b; 
                        valid = 1;
                    }
                } else if (strcmp(cmd_str, "ATTACK") == 0) {
                    if (sscanf(buffer, "%*s %d %d %d", &a, &b, &c) == 3) {
                        msg.cmd = CMD_ATTACK;
                        msg.args[1] = a; 
                        msg.args[2] = b; 
                        msg.args[3] = c; 
                        valid = 1;
                    }
                }

                if (valid) {
                    msgsnd(msg_id, &msg, sizeof(Message)-sizeof(long), 0);
                }
            }
        }
    }
    return 0;
}