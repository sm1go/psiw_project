#include "common.h"

int msg_id;
int my_id;

void clear_screen() {
    printf("\033[1;1H\033[2J");
}

void print_dashboard(GameMsg *state) {
    clear_screen();
    printf("==========================================\n");
    printf("     GRA STRATEGICZNA - GRACZ %d\n", my_id + 1);
    printf("==========================================\n");
    printf(" ZLOTO: \033[1;33m%d\033[0m  |  PUNKTY: %d (Wrog: %d)\n",
           state->gold_now, state->score_me, state->score_enemy);
    printf("------------------------------------------\n");
    printf(" TWOJA ARMIA:\n");
    printf(" [0] Lekka Piechota: %d\n", state->units_now[LIGHT]);
    printf(" [1] Ciezka Piechota:%d\n", state->units_now[HEAVY]);
    printf(" [2] Jazda:          %d\n", state->units_now[CAVALRY]);
    printf(" [3] Robotnicy:      %d\n", state->units_now[WORKER]);
    printf("==========================================\n");
    printf(" KOMENDY:\n");
    printf(" b [typ] [ilosc]  -> Buduj (np. b 0 5)\n");
    printf(" a [L] [C] [J]    -> Atakuj (np. a 5 2 1)\n");
    printf(" q                -> Wyjdz\n");
    printf("==========================================\n");
    printf(" Ostatni raport: \033[1;31m%s\033[0m\n", state->text);
    printf("> ");
    fflush(stdout);
}

void receiver_thread() {
    GameMsg msg;
    char last_report[MAX_TEXT] = "Brak";

    while(1) {
        if (msgrcv(msg_id, &msg, sizeof(GameMsg)-sizeof(long), -(20 + my_id), 0) != -1) {

            if (msg.mtype == 10 + my_id) {
                strcpy(msg.text, last_report);
                print_dashboard(&msg);
            }
            else if (msg.mtype == 20 + my_id) {
                strcpy(last_report, msg.text);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uzycie: ./client [1 lub 2]\n");
        return 1;
    }
    my_id = atoi(argv[1]) - 1;

    key_t key = ftok(".", PROJECT_ID);
    msg_id = msgget(key, 0666);
    if (msg_id == -1) {
        printf("Serwer nie dziala!\n");
        return 1;
    }

    GameMsg login;
    login.mtype = 1;
    login.source_id = my_id;
    login.cmd_type = MSG_LOGIN;
    login.count = getpid();
    msgsnd(msg_id, &login, sizeof(GameMsg)-sizeof(long), 0);

    if (fork() == 0) {
        receiver_thread();
        exit(0);
    }

    char cmd_char;
    while(1) {
        scanf(" %c", &cmd_char);

        GameMsg order;
        order.mtype = 1;
        order.source_id = my_id;

        if (cmd_char == 'b') {
            order.cmd_type = MSG_BUILD;
            scanf("%d %d", &order.unit_type, &order.count);
            msgsnd(msg_id, &order, sizeof(GameMsg)-sizeof(long), 0);
        }
        else if (cmd_char == 'a') {
            order.cmd_type = MSG_ATTACK;
            scanf("%d %d %d", &order.attack_army[0], &order.attack_army[1], &order.attack_army[2]);
            order.attack_army[3] = 0;
            msgsnd(msg_id, &order, sizeof(GameMsg)-sizeof(long), 0);
        }
        else if (cmd_char == 'q') {
            kill(0, SIGKILL);
            exit(0);
        }
    }
}