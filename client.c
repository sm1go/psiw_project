#include "common.h"

int msg_id;
int my_player_id = -1; 
int my_msg_type = -1;

void receiver_loop() {
    Message msg;
    while(1) {
        if (msgrcv(msg_id, &msg, sizeof(Message)-sizeof(long), my_msg_type, 0) == -1) {
            exit(1);
        }
        if (msg.cmd == CMD_UPDATE) {
            printf("\033[2J\033[H"); 
            printf("GRACZ %d | %s\n", my_player_id + 1, msg.text);
            printf("KOMENDY: TRAIN [0-3] [ilosc] | ATTACK [L] [H] [C]\n");
            printf("(0-Lekka, 1-Ciezka, 2-Jazda, 3-Robotnik)\n> ");
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

    printf("Zalogowano jako Gracz %d\n", my_player_id + 1);

    if (fork() == 0) {
        receiver_loop();
    } else {
        char buffer[100];
        while(1) {
            fgets(buffer, 100, stdin);
            char cmd_str[10];
            int a, b, c;
            
            if (sscanf(buffer, "%s %d %d %d", cmd_str, &a, &b, &c) >= 2) {
                msg.type = 1; 
                msg.source_id = getpid();
                msg.args[0] = my_player_id;

                if (strcmp(cmd_str, "TRAIN") == 0) {
                    msg.cmd = CMD_TRAIN;
                    msg.args[1] = a; 
                    msg.args[2] = b; 
                    msgsnd(msg_id, &msg, sizeof(Message)-sizeof(long), 0);
                } else if (strcmp(cmd_str, "ATTACK") == 0) {
                    msg.cmd = CMD_ATTACK;
                    msg.args[1] = a; 
                    msg.args[2] = b; 
                    msg.args[3] = c; 
                    msgsnd(msg_id, &msg, sizeof(Message)-sizeof(long), 0);
                }
            }
        }
    }
    return 0;
}