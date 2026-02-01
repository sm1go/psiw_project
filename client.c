#include "common.h"

int my_id = 0;
int msgid;
pid_t ui_child_pid;

void sys_print(const char *text) {
    write(STDOUT_FILENO, text, strlen(text));
}

void run_ui_loop() {
    GameMsg m;
    char buffer[1024];
    while(1) {
        m.mtype = 1;
        m.player_id = getppid();
        m.action_type = M_STATE;
        msgsnd(msgid, &m, sizeof(GameMsg)-sizeof(long), 0);
        msgrcv(msgid, &m, sizeof(GameMsg)-sizeof(long), getppid(), 0);

        sys_print("\033[H\033[J"); // Czyści ekran
        sys_print("============= GRA STRATEGICZNA =============\n");
        sprintf(buffer, "  GRACZ: %d | Punkty Zwyciestwa: %d/%d\n", my_id, m.points, WIN_POINTS);
        sys_print(buffer);
        sys_print("--------------------------------------------\n");
        sprintf(buffer, "  Zloto: %-8d | Status: %s\n", m.resources, m.status ? "AKTYWNA" : "OCZEKIWANIE");
        sys_print(buffer);
        sys_print("--------------------------------------------\n");
        sys_print("  ARMIA W BAZIE:\n");
        for(int i=0; i<4; i++) {
            sprintf(buffer, "  [%d] %-16s : %d\n", i, UNITS[i].name, m.army[i]);
            sys_print(buffer);
        }
        sys_print("--------------------------------------------\n");
        sys_print("  KOMENDY: 1 - Buduj jednostki | 2 - Atakuj\n");
        sys_print("  Twoj wybor: ");

        if (m.points >= WIN_POINTS) {
            sys_print("\n\n  *** GRATULACJE! WYGRALES WOJNE! ***\n");
            kill(getppid(), SIGINT);
        } else if (m.points == -1) {
            sys_print("\n\n  *** PORAZKA! TWOJA BAZA UPADLA! ***\n");
            kill(getppid(), SIGINT);
        }
        sleep(1);
    }
}

int main() {
    msgid = msgget(KEY, 0600);
    if (msgid == -1) { sys_print("Blad: Serwer nie dziala!\n"); return 1; }

    GameMsg m;
    m.mtype = 1;
    m.player_id = getpid();
    m.action_type = M_LOGIN;
    msgsnd(msgid, &m, sizeof(GameMsg)-sizeof(long), 0);
    msgrcv(msgid, &m, sizeof(GameMsg)-sizeof(long), getpid(), 0);

    if (m.player_id == -1) { sys_print("Serwer pelny!\n"); return 0; }
    my_id = m.player_id;

    if ((ui_child_pid = fork()) == 0) {
        run_ui_loop();
        exit(0);
    }

    char in_buf[16];
    while(1) {
        int bytes = read(STDIN_FILENO, in_buf, 15);
        if (bytes <= 0) continue;
        in_buf[bytes] = '\0';
        int choice = atoi(in_buf);

        if (choice == 1) {
            kill(ui_child_pid, SIGSTOP);
            sys_print("\n--- BUDOWANIE ---\nTyp (0-Lekka, 1-Ciezka, 2-Jazda, 3-Rob): ");
            bytes = read(STDIN_FILENO, in_buf, 15); in_buf[bytes] = '\0';
            int type = atoi(in_buf);
            sys_print("Ilosc: ");
            bytes = read(STDIN_FILENO, in_buf, 15); in_buf[bytes] = '\0';
            int count = atoi(in_buf);

            m.mtype = 1;
            m.player_id = my_id;
            m.action_type = M_BUILD;
            m.unit_idx = type;
            m.count = count;
            msgsnd(msgid, &m, sizeof(GameMsg)-sizeof(long), 0);
            kill(ui_child_pid, SIGCONT);
        } 
        else if (choice == 2) {
            kill(ui_child_pid, SIGSTOP);
            sys_print("\n--- ATAK (Podaj ilosci) ---\n");
            m.mtype = 1;
            m.player_id = my_id;
            m.action_type = M_ATTACK;
            for(int i=0; i<3; i++) {
                sprintf(in_buf, " %s: ", UNITS[i].name); sys_print(in_buf);
                bytes = read(STDIN_FILENO, in_buf, 15); in_buf[bytes] = '\0';
                m.army[i] = atoi(in_buf);
            }
            msgsnd(msgid, &m, sizeof(GameMsg)-sizeof(long), 0);
            kill(ui_child_pid, SIGCONT);
        }
    }
    return 0;
}