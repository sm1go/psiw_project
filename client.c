#include "common.h"

int my_player_id = 0;
int msg_id;
pid_t ui_pid;

void handle_exit(int sig) {
    if (ui_pid > 0) kill(ui_pid, SIGKILL);
    exit(0);
}

void run_ui() {
    GameMessage m;
    while(1) {
        m.mtype = 100;
        m.player_id = getppid();
        m.action = ACTION_STATE;
        msgsnd(msg_id, &m, sizeof(GameMessage)-sizeof(long), 0);
        // Odbieramy na kanale PID_RODZICA + 200
        msgrcv(msg_id, &m, sizeof(GameMessage)-sizeof(long), getppid() + 200, 0);

        write_str(STDOUT_FILENO, "\033[H\033[J"); 
        write_str(STDOUT_FILENO, "============= GRA WOJENNA =============\n");
        write_str(STDOUT_FILENO, "  ID GRACZA: "); write_int(STDOUT_FILENO, my_player_id);
        write_str(STDOUT_FILENO, " | PUNKTY: "); write_int(STDOUT_FILENO, m.score_info);
        write_str(STDOUT_FILENO, "\n---------------------------------------\n");
        write_str(STDOUT_FILENO, "  Zloto: "); write_int(STDOUT_FILENO, m.resource_info);
        write_str(STDOUT_FILENO, " | Status: "); write_str(STDOUT_FILENO, m.game_status ? "TRWA" : "CZEKANIE");
        write_str(STDOUT_FILENO, "\n---------------------------------------\n");
        for(int i=0; i<4; i++) {
            write_str(STDOUT_FILENO, "  ["); write_int(STDOUT_FILENO, i); write_str(STDOUT_FILENO, "] ");
            write_str(STDOUT_FILENO, UNIT_STATS[i].name); write_str(STDOUT_FILENO, ": ");
            write_int(STDOUT_FILENO, m.army_info[i]); write_str(STDOUT_FILENO, "\n");
        }
        write_str(STDOUT_FILENO, "---------------------------------------\n");
        write_str(STDOUT_FILENO, "  1: Buduj | 2: Atakuj\n  Wybor: ");

        if (m.score_info >= WIN_THRESHOLD) {
            write_str(STDOUT_FILENO, "\n\n  *** WYGRANA! ***\n");
            kill(getppid(), SIGINT);
        } else if (m.score_info == -1) {
            write_str(STDOUT_FILENO, "\n\n  *** PORAZKA! ***\n");
            kill(getppid(), SIGINT);
        }
        sleep(1);
    }
}

int main() {
    signal(SIGINT, handle_exit);
    msg_id = msgget(PROJECT_KEY, 0600);
    if (msg_id == -1) { write_str(2, "Blad: Brak serwera!\n"); return 1; }

    GameMessage m;
    m.mtype = 100;
    m.player_id = getpid();
    m.action = ACTION_LOGIN;
    msgsnd(msg_id, &m, sizeof(GameMessage)-sizeof(long), 0);
    msgrcv(msg_id, &m, sizeof(GameMessage)-sizeof(long), getpid(), 0);

    if (m.player_id == -1) { write_str(2, "Brak miejsc!\n"); return 0; }
    my_player_id = m.player_id;

    if ((ui_pid = fork()) == 0) {
        run_ui();
        exit(0);
    }

    char buf[32];
    while(1) {
        int n = read(STDIN_FILENO, buf, 31);
        if (n <= 0) continue;
        buf[n-1] = '\0'; // Usuwamy znak nowej linii
        int choice = string_to_int(buf);

        if (choice == 1) {
            kill(ui_pid, SIGSTOP);
            write_str(STDOUT_FILENO, "\n[BUDOWA] Typ (0-3): ");
            n = read(STDIN_FILENO, buf, 31); buf[n-1] = '\0';
            int type = string_to_int(buf);
            write_str(STDOUT_FILENO, "Ilosc: ");
            n = read(STDIN_FILENO, buf, 31); buf[n-1] = '\0';
            int qty = string_to_int(buf);

            m.mtype = 100;
            m.player_id = my_player_id;
            m.action = ACTION_BUILD;
            m.unit_type = type;
            m.quantity = qty;
            msgsnd(msg_id, &m, sizeof(GameMessage)-sizeof(long), 0);
            kill(ui_pid, SIGCONT);
        } 
        else if (choice == 2) {
            kill(ui_pid, SIGSTOP);
            write_str(STDOUT_FILENO, "\n[ATAK] Podaj ilosci:\n");
            m.mtype = 100;
            m.player_id = my_player_id;
            m.action = ACTION_ATTACK;
            for(int i=0; i<3; i++) {
                write_str(STDOUT_FILENO, " "); write_str(STDOUT_FILENO, UNIT_STATS[i].name); write_str(STDOUT_FILENO, ": ");
                n = read(STDIN_FILENO, buf, 31); buf[n-1] = '\0';
                m.army_info[i] = string_to_int(buf);
            }
            msgsnd(msg_id, &m, sizeof(GameMessage)-sizeof(long), 0);
            kill(ui_pid, SIGCONT);
        }
    }
    return 0;
}