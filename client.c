#include "common.h"

int current_player_id = 0;
int msg_queue_id;
pid_t ui_pid;

void render_display() {
    GameMessage m;
    while(1) {
        m.mtype = TYPE_SERVER;
        m.player_id = getppid();
        m.action = ACTION_STATE;
        msgsnd(msg_queue_id, &m, sizeof(GameMessage)-sizeof(long), 0);
        msgrcv(msg_queue_id, &m, sizeof(GameMessage)-sizeof(long), getppid(), 0);

        write_str(STDOUT_FILENO, "\033[H\033[J"); // Clear
        write_str(STDOUT_FILENO, "============= GRA WOJENNA 2025 =============\n");
        write_str(STDOUT_FILENO, "  ID GRACZA: "); write_int(STDOUT_FILENO, current_player_id);
        write_str(STDOUT_FILENO, " | PUNKTY: "); write_int(STDOUT_FILENO, m.score_info);
        write_str(STDOUT_FILENO, "/"); write_int(STDOUT_FILENO, WIN_THRESHOLD);
        write_str(STDOUT_FILENO, "\n--------------------------------------------\n");
        write_str(STDOUT_FILENO, "  Zloto: "); write_int(STDOUT_FILENO, m.resource_info);
        write_str(STDOUT_FILENO, " | Status: "); write_str(STDOUT_FILENO, m.game_status ? "TRWA" : "CZEKANIE");
        write_str(STDOUT_FILENO, "\n--------------------------------------------\n");
        write_str(STDOUT_FILENO, "  ARMIA W BAZIE:\n");
        for(int i=0; i<4; i++) {
            write_str(STDOUT_FILENO, "  ["); write_int(STDOUT_FILENO, i); write_str(STDOUT_FILENO, "] ");
            write_str(STDOUT_FILENO, UNIT_STATS[i].name); write_str(STDOUT_FILENO, " : ");
            write_int(STDOUT_FILENO, m.army_info[i]); write_str(STDOUT_FILENO, "\n");
        }
        write_str(STDOUT_FILENO, "--------------------------------------------\n");
        write_str(STDOUT_FILENO, "  AKCJA: 1 - Buduj | 2 - Atakuj\n  Wybor: ");

        if (m.score_info >= WIN_THRESHOLD) {
            write_str(STDOUT_FILENO, "\n\n  *** WYGRALES! KONIEC GRY ***\n");
            kill(getppid(), SIGINT);
        } else if (m.score_info == -1) {
            write_str(STDOUT_FILENO, "\n\n  *** PRZEGRALES! PRZECIWNIK WYGRAL ***\n");
            kill(getppid(), SIGINT);
        }
        sleep(1);
    }
}

int main() {
    msg_queue_id = msgget(PROJECT_KEY, 0600);
    if (msg_queue_id == -1) { write_str(2, "Serwer nieaktywny!\n"); return 1; }

    GameMessage m;
    m.mtype = TYPE_SERVER;
    m.player_id = getpid();
    m.action = ACTION_LOGIN;
    msgsnd(msg_queue_id, &m, sizeof(GameMessage)-sizeof(long), 0);
    msgrcv(msg_queue_id, &m, sizeof(GameMessage)-sizeof(long), getpid(), 0);

    if (m.player_id == -1) { write_str(2, "Brak miejsc!\n"); return 0; }
    current_player_id = m.player_id;

    if ((ui_pid = fork()) == 0) {
        render_display();
        exit(0);
    }

    char buf[16];
    while(1) {
        int n = read(STDIN_FILENO, buf, 15);
        if (n <= 0) continue;
        buf[n] = '\0';
        int choice = atoi(buf);

        if (choice == 1) {
            kill(ui_pid, SIGSTOP);
            write_str(STDOUT_FILENO, "\n[BUDOWA] Typ (0-3): ");
            n = read(STDIN_FILENO, buf, 15); buf[n] = '\0';
            int t = atoi(buf);
            write_str(STDOUT_FILENO, "Ilosc: ");
            n = read(STDIN_FILENO, buf, 15); buf[n] = '\0';
            int q = atoi(buf);

            m.mtype = TYPE_SERVER;
            m.player_id = current_player_id;
            m.action = ACTION_BUILD;
            m.unit_type = t;
            m.quantity = q;
            msgsnd(msg_queue_id, &m, sizeof(GameMessage)-sizeof(long), 0);
            kill(ui_pid, SIGCONT);
        } 
        else if (choice == 2) {
            kill(ui_pid, SIGSTOP);
            write_str(STDOUT_FILENO, "\n[ATAK] Podaj ilosci wojsk:\n");
            m.mtype = TYPE_SERVER;
            m.player_id = current_player_id;
            m.action = ACTION_ATTACK;
            for(int i=0; i<3; i++) {
                write_str(STDOUT_FILENO, " "); write_str(STDOUT_FILENO, UNIT_STATS[i].name); write_str(STDOUT_FILENO, ": ");
                n = read(STDIN_FILENO, buf, 15); buf[n] = '\0';
                m.army_info[i] = atoi(buf);
            }
            msgsnd(msg_queue_id, &m, sizeof(GameMessage)-sizeof(long), 0);
            kill(ui_pid, SIGCONT);
        }
    }
    return 0;
}