#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "common.h"

int msg_id;
int my_id = -1;
pid_t my_pid;

void my_write(const char* str) {
    write(1, str, strlen(str));
}

void draw_interface(int gold, int u1, int u2, int u3) {
    char buf[512];
    sprintf(buf, "\033[H\033[J"); 
    write(1, buf, strlen(buf));
    
    sprintf(buf, "=== KLIENT GRY === (PID: %d)\n", my_pid);
    write(1, buf, strlen(buf));
    sprintf(buf, "ZLOTO: %d | LEKKA: %d | CIEZKA: %d | JAZDA: %d\n", gold, u1, u2, u3);
    write(1, buf, strlen(buf));
    sprintf(buf, "------------------------------------------------\n");
    write(1, buf, strlen(buf));
    sprintf(buf, "Komendy:\n b [typ 0-3] [ilosc] - buduj (0:Lekka, 1:Ciezka, 2:Jazda, 3:Rob)\n a [l] [c] [j] - atakuj\n");
    write(1, buf, strlen(buf));
    sprintf(buf, "\033[10;0H> "); 
    write(1, buf, strlen(buf));
}

void receiver_loop() {
    Message msg;
    while(1) {
        if (msgrcv(msg_id, &msg, sizeof(Message) - sizeof(long), my_pid, 0) > 0) {
            if (msg.cmd == MSG_STATE) {
                write(1, "\0337", 2); 
                draw_interface(msg.args[0], msg.args[1], msg.args[2], msg.args[3]);
                write(1, "\0338", 2); 
            } else if (msg.cmd == MSG_RESULT || msg.cmd == MSG_GAME_OVER) {
                write(1, "\0337", 2); 
                write(1, "\033[6;0H", 6); 
                char buf[300];
                sprintf(buf, "KOMUNIKAT: %s\n", msg.text);
                write(1, buf, strlen(buf));
                write(1, "\0338", 2); 
                if (msg.cmd == MSG_GAME_OVER) exit(0);
            } else if (msg.cmd == CMD_LOGIN) {
                my_id = msg.args[0];
            }
        }
    }
}

int main() {
    my_pid = getpid();
    key_t key = ftok(".", KEY_ID);
    msg_id = msgget(key, 0666);
    
    if (msg_id == -1) {
        my_write("Brak kolejki komunikatow. Uruchom serwer.\n");
        return 1;
    }

    Message login;
    login.mtype = 1; 
    login.src_id = my_pid;
    login.cmd = CMD_LOGIN;
    msgsnd(msg_id, &login, sizeof(Message) - sizeof(long), 0);

    if (fork() == 0) {
        receiver_loop();
        exit(0);
    }

    char input[100];
    while(1) {
        int n = read(0, input, 99);
        if (n > 0) {
            input[n] = 0;
            char cmd;
            int a1, a2, a3;
            if (sscanf(input, "%c %d %d %d", &cmd, &a1, &a2, &a3) >= 1) {
                Message ord;
                ord.mtype = 1;
                ord.src_id = my_pid;
                
                if (cmd == 'b') {
                    ord.cmd = CMD_BUILD;
                    ord.args[0] = a1; 
                    ord.args[1] = a2; 
                    msgsnd(msg_id, &ord, sizeof(Message) - sizeof(long), 0);
                } else if (cmd == 'a') {
                    ord.cmd = CMD_ATTACK;
                    ord.args[0] = a1;
                    ord.args[1] = a2;
                    ord.args[2] = a3;
                    msgsnd(msg_id, &ord, sizeof(Message) - sizeof(long), 0);
                }
            }
            write(1, "\033[10;0H\033[K> ", 11); 
        }
    }
}