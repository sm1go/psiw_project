#include "common.h"

int my_player_id = 0;
int mid;
pid_t ui_pid;

void quit_client(int s) { 
    if (ui_pid > 0) kill(ui_pid, SIGKILL); 
    exit(0); 
}

void run_display() {
    Msg m;
    while(1) {
        m.mtype = CHAN_SERVER;
        m.player_id = my_player_id;
        m.action = ACT_STATE;
        msgsnd(mid, &m, sizeof(Msg)-sizeof(long), 0);
        msgrcv(mid, &m, sizeof(Msg)-sizeof(long), (my_player_id == 1) ? CHAN_P1 : CHAN_P2, 0);

        print_s("\033[H\033[J"); // Czyszczenie ekranu
        print_s("=== GRA STRATEGICZNA | TWOJE ID: "); print_i(my_player_id); print_s(" ===\n");
        print_s("Punkty: "); print_i(m.score == -1 ? 0 : m.score); print_s("/"); print_i(WIN_THRESHOLD);
        print_s(" | Zloto: "); print_i(m.gold); 
        print_s(" | Status: "); print_s(m.active ? "GRA TRWA\n" : "OCZEKIWANIE NA PRZECIWNIKA\n");
        print_s("--------------------------------------------\n");
        for(int i=0; i<4; i++) {
            print_s("["); print_i(i); print_s("] "); print_s(STATS[i].name);
            print_s(": "); print_i(m.army[i]); print_s("\n");
        }
        print_s("--------------------------------------------\n");
        print_s("1: Buduj jednostki | 2: Atakuj baze wroga\nTwoj wybor: ");

        // LOGIKA KONCA GRY
        if (m.score >= WIN_THRESHOLD) {
            print_s("\n\n============================================\n");
            print_s("   KONIEC GRY: GRACZ O ID "); print_i(my_player_id); print_s(" WYGRAL!\n");
            print_s("============================================\n");
            sleep(1);
            kill(getppid(), SIGINT); // Zamknij proces glowny
        } else if (m.score == -1) {
            int winner_id = (my_player_id == 1) ? 2 : 1;
            print_s("\n\n============================================\n");
            print_s("   KONIEC GRY: GRACZ O ID "); print_i(winner_id); print_s(" WYGRAL!\n");
            print_s("============================================\n");
            sleep(1);
            kill(getppid(), SIGINT); // Zamknij proces glowny
        }
        sleep(1);
    }
}

int main() {
    signal(SIGINT, quit_client);
    mid = msgget(PROJECT_KEY, 0600);
    if (mid == -1) { print_s("Blad: Serwer nie jest uruchomiony!\n"); return 1; }

    Msg m;
    m.mtype = CHAN_SERVER;
    m.sender_pid = getpid();
    m.action = ACT_LOGIN;
    msgsnd(mid, &m, sizeof(Msg)-sizeof(long), 0);
    msgrcv(mid, &m, sizeof(Msg)-sizeof(long), getpid(), 0);

    if (m.player_id == -1) { print_s("Serwer pelny. Sprobuj pozniej.\n"); return 0; }
    my_player_id = m.player_id;

    if ((ui_pid = fork()) == 0) { 
        run_display(); 
        exit(0); 
    }

    char buffer[32];
    while(1) {
        int n = read(STDIN_FILENO, buffer, 31);
        if (n <= 0) continue;
        buffer[n-1] = '\0';
        int choice = str_to_i(buffer);

        if (choice == 1) {
            kill(ui_pid, SIGSTOP);
            print_s("\n[BUDOWA] Typ (0-3): "); n = read(STDIN_FILENO, buffer, 31); buffer[n-1] = '\0';
            int t = str_to_i(buffer);
            print_s("Ilosc: "); n = read(STDIN_FILENO, buffer, 31); buffer[n-1] = '\0';
            int q = str_to_i(buffer);
            m.mtype = CHAN_SERVER; m.player_id = my_player_id; m.action = ACT_BUILD;
            m.u_type = t; m.u_count = q;
            msgsnd(mid, &m, sizeof(Msg)-sizeof(long), 0);
            kill(ui_pid, SIGCONT);
        } else if (choice == 2) {
            kill(ui_pid, SIGSTOP);
            print_s("\n[ATAK] Podaj jednostki do wyslania:\n");
            m.mtype = CHAN_SERVER; m.player_id = my_player_id; m.action = ACT_ATTACK;
            for(int i=0; i<3; i++) {
                print_s(" "); print_s(STATS[i].name); print_s(": ");
                n = read(STDIN_FILENO, buffer, 31); buffer[n-1] = '\0';
                m.army[i] = str_to_i(buffer);
            }
            msgsnd(mid, &m, sizeof(Msg)-sizeof(long), 0);
            kill(ui_pid, SIGCONT);
        }
    }
    return 0;
}