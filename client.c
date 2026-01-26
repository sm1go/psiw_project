#include "common.h"

int msgid;
int player_id;

void display_state(struct UpdateMsg *msg) {
    write(1, "\033[2J\033[H", 7); 
    my_print("=== GRA STRATEGICZNA ===\n");
    my_print("Gracz: "); my_print_int(player_id); my_print("\n");
    my_print("Zasoby: "); my_print_int(msg->self.resources); my_print("\n");
    my_print("Punkty Zwyciestwa: "); my_print_int(msg->self.victory_points); my_print(" / 5\n");
    my_print("------------------------\n");
    my_print("TWOJE JEDNOSTKI:\n");
    my_print("0. Lekka P. : "); my_print_int(msg->self.units[0]); 
    my_print(" (W prod: "); my_print_int(msg->self.production_queue[0]); my_print(")\n");
    my_print("1. Ciezka P.: "); my_print_int(msg->self.units[1]);
    my_print(" (W prod: "); my_print_int(msg->self.production_queue[1]); my_print(")\n");
    my_print("2. Jazda    : "); my_print_int(msg->self.units[2]);
    my_print(" (W prod: "); my_print_int(msg->self.production_queue[2]); my_print(")\n");
    my_print("3. Robotnicy: "); my_print_int(msg->self.units[3]);
    my_print(" (W prod: "); my_print_int(msg->self.production_queue[3]); my_print(")\n");
    my_print("------------------------\n");
    my_print("PRZECIWNIK:\n");
    my_print("Lekka: "); my_print_int(msg->enemy.units[0]); my_print(" | ");
    my_print("Ciezka: "); my_print_int(msg->enemy.units[1]); my_print(" | ");
    my_print("Jazda: "); my_print_int(msg->enemy.units[2]); my_print("\n");
    my_print("Punkty Wroga: "); my_print_int(msg->enemy.victory_points); my_print("\n");
    my_print("------------------------\n");
    if(my_strlen(msg->message) > 0) {
        my_print("KOMUNIKAT: "); my_print(msg->message); my_print("\n");
    }
    my_print("------------------------\n");
    my_print("KOMENDY: [t typ ilosc] (trenuj), [a u0 u1 u2] (atak)\n");
    my_print("> ");
}

void receiver_process() {
    struct UpdateMsg msg;
    long type = (player_id == 1) ? 10 : 20;
    while(1) {
        if(msgrcv(msgid, &msg, sizeof(struct UpdateMsg) - sizeof(long), type, 0) != -1) {
            display_state(&msg);
            if(my_strlen(msg.message) >= 6 && msg.message[0] == 'K' && msg.message[1] == 'O') {
                exit(0);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if(argc < 2) {
        my_print("Uzycie: ./client [1 lub 2]\n");
        return 1;
    }
    player_id = my_atoi(argv[1]);
    msgid = msgget(KEY_MSG, 0666);
    
    struct MsgBuf login;
    login.mtype = MSG_TYPE_LOGIN;
    login.player_id = player_id;
    msgsnd(msgid, &login, sizeof(struct MsgBuf) - sizeof(long), 0);

    if(fork() == 0) {
        receiver_process();
    } else {
        char buf[50];
        while(1) {
            int n = read(0, buf, 50);
            if(n > 0) {
                buf[n] = '\0';
                struct MsgBuf req;
                req.mtype = player_id; 
                req.player_id = player_id;

                if(buf[0] == 't') {
                    req.command = 1;
                    req.unit_type = buf[2] - '0';
                    char num_buf[10];
                    int k=0;
                    for(int i=4; i<n && buf[i]>='0' && buf[i]<='9'; i++) {
                        num_buf[k++] = buf[i];
                    }
                    num_buf[k] = '\0';
                    req.count = my_atoi(num_buf);
                    msgsnd(msgid, &req, sizeof(struct MsgBuf) - sizeof(long), 0);
                }
                else if(buf[0] == 'a') {
                    req.command = 2;
                    char u0[5], u1[5], u2[5];
                    int idx = 2, k=0;
                    while(buf[idx] != ' ' && idx < n) u0[k++] = buf[idx++]; u0[k] = '\0'; idx++; k=0;
                    while(buf[idx] != ' ' && idx < n) u1[k++] = buf[idx++]; u1[k] = '\0'; idx++; k=0;
                    while(buf[idx] != ' ' && buf[idx] != '\n' && idx < n) u2[k++] = buf[idx++]; u2[k] = '\0';
                    
                    req.units_to_attack[0] = my_atoi(u0);
                    req.units_to_attack[1] = my_atoi(u1);
                    req.units_to_attack[2] = my_atoi(u2);
                    msgsnd(msgid, &req, sizeof(struct MsgBuf) - sizeof(long), 0);
                }
            }
        }
    }
    return 0;
}