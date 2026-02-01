#include "common.h"

int my_id = 0;
int mid;
pid_t ui_pid;

void quit(int s) { if (ui_pid > 0) kill(ui_pid, SIGKILL); exit(0); }

void run_display() {
    Msg m;
    while(1) {
        m.mtype = CHAN_SERVER;
        m.player_id = my_id;
        m.action = ACT_STATE;
        msgsnd(mid, &m, sizeof(Msg)-sizeof(long), 0);
        msgrcv(mid, &m, sizeof(Msg)-sizeof(long), (my_id == 1) ? CHAN_P1 : CHAN_P2, 0);

        print_s("\033[H\033[J"); // Clear
        print_s("=== GRA STRATEGICZNA | GRACZ "); print_i(my_id); print_s(" ===\n");
        print_s("Punkty: "); print_i(m.score); print_s("/"); print_i(WIN_THRESHOLD);
        print_s(" | Zloto: "); print_i(m.gold); 
        print_s(" | Status: "); print_s(m.active ? "GRA TRWA\n" : "CZEKANIE\n");
        print_s("------------------------------------\n");
        for(int i=0; i<4; i++) {
            print_s("["); print_i(i); print_s("] "); print_s(STATS[i].name);
            print_s(": "); print_i(m.army[i]); print_s("\n");
        }
        print_s("------------------------------------\n");
        print_s("1: Buduj | 2: Atakuj\nWybor: ");

        if (m.score >= WIN_THRESHOLD) { print_s("\n\n*** WYGRANA! ***\n"); kill(getppid(), SIGINT); }
        else if (m.score == -1) { print_s("\n\n*** PORAZKA! ***\n"); kill(getppid(), SIGINT); }
        sleep(1);
    }
}

int main() {
    signal(SIGINT, quit);
    mid = msgget(PROJECT_KEY, 0600);
    if (mid == -1) { print_s("Serwer nie dziala!\n"); return 1; }

    Msg m;
    m.mtype = CHAN_SERVER;
    m.sender_pid = getpid();
    m.action = ACT_LOGIN;
    msgsnd(mid, &m, sizeof(Msg)-sizeof(long), 0);
    msgrcv(mid, &m, sizeof(Msg)-sizeof(long), getpid(), 0);

    if (m.player_id == -1) { print_s("Brak miejsc.\n"); return 0; }
    my_id = m.player_id;

    if ((ui_pid = fork()) == 0) { run_display(); exit(0); }

    char b[32];
    while(1) {
        int n = read(STDIN_FILENO, b, 31);
        if (n <= 0) continue;
        b[n-1] = '\0';
        int c = str_to_i(b);

        if (c == 1) {
            kill(ui_pid, SIGSTOP);
            print_s("\n[BUDOWA] Typ (0-3): "); n = read(STDIN_FILENO, b, 31); b[n-1] = '\0';
            int t = str_to_i(b);
            print_s("Ilosc: "); n = read(STDIN_FILENO, b, 31); b[n-1] = '\0';
            int q = str_to_i(b);
            m.mtype = CHAN_SERVER; m.player_id = my_id; m.action = ACT_BUILD;
            m.u_type = t; m.u_count = q;
            msgsnd(mid, &m, sizeof(Msg)-sizeof(long), 0);
            kill(ui_pid, SIGCONT);
        } else if (c == 2) {
            kill(ui_pid, SIGSTOP);
            print_s("\n[ATAK] Podaj ilosci wojsk:\n");
            m.mtype = CHAN_SERVER; m.player_id = my_id; m.action = ACT_ATTACK;
            for(int i=0; i<3; i++) {
                print_s(" "); print_s(STATS[i].name); print_s(": ");
                n = read(STDIN_FILENO, b, 31); b[n-1] = '\0';
                m.army[i] = str_to_i(b);
            }
            msgsnd(mid, &m, sizeof(Msg)-sizeof(long), 0);
            kill(ui_pid, SIGCONT);
        }
    }
    return 0;
}