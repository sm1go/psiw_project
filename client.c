#include "common.h"

int my_player_id = 0;
int mid;
pid_t ui_pid;

// Obsługa wyjścia - zabijamy dziecko i kończymy proces główny
void quit_client(int s) { 
    if (ui_pid > 0) kill(ui_pid, SIGKILL); 
    print_s("\n[KLIENT] Rozlaczono.\n");
    exit(0); 
}

void run_display() {
    Msg m;
    while(1) {
        m.mtype = CHAN_SERVER;
        m.player_id = my_player_id;
        m.action = ACT_STATE;
        
        if (msgsnd(mid, &m, sizeof(Msg)-sizeof(long), 0) == -1) break;
        if (msgrcv(mid, &m, sizeof(Msg)-sizeof(long), (my_player_id == 1) ? CHAN_P1 : CHAN_P2, 0) == -1) break;

        // Jeśli wykryto koniec gry, wychodzimy z pętli odświeżania
        if (m.score >= WIN_THRESHOLD || m.score == -1) break;

        print_s("\033[H\033[J"); // Czyść ekran
        print_s("=== GRA STRATEGICZNA | TWOJE ID: "); print_i(my_player_id); print_s(" ===\n");
        print_s("Punkty: "); print_i(m.score); print_s("/"); print_i(WIN_THRESHOLD);
        print_s(" | Zloto: "); print_i(m.gold); 
        print_s(" | Status: "); print_s(m.active ? "GRA TRWA\n" : "OCZEKIWANIE NA PRZECIWNIKA\n");
        print_s("--------------------------------------------\n");
        for(int i=0; i<4; i++) {
            print_s("["); print_i(i); print_s("] "); print_s(STATS[i].name);
            print_s(": "); print_i(m.army[i]); print_s("\n");
        }
        print_s("--------------------------------------------\n");
        print_s("1: Buduj jednostki | 2: Atakuj baze wroga\nTwoj wybor: ");

        sleep(1);
    }

    // --- EKRAN KOŃCOWY (POZA PĘTLĄ) ---
    // Czyścimy ekran ostatni raz, żeby wyświetlić tylko wynik
    print_s("\033[H\033[J"); 
    print_s("\n\n\n\n\n"); // Margines górny
    print_s("          ########################################\n");
    if (m.score >= WIN_THRESHOLD) {
        print_s("          #           !!! WYGRANA !!!            #\n");
    } else {
        print_s("          #           ... PRZEGRANA ...          #\n");
    }
    print_s("          ########################################\n");
    print_s("\n\n          Gra zakonczona. Zamykanie...\n\n");
    
    sleep(3); // Dajemy 3 sekundy na przeczytanie, zanim Shell przejmie terminal
    kill(getppid(), SIGINT); // Dopiero teraz zamykamy proces główny
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

    if (m.player_id == -1) { print_s("Serwer pelny.\n"); return 0; }
    my_player_id = m.player_id;

    if ((ui_pid = fork()) == 0) { 
        run_display(); 
        exit(0); 
    }

    char buffer[32];
    while(1) {
        // Proces główny czeka na wejście. Jeśli otrzyma sygnał SIGINT od dziecka, 
        // funkcja read zostanie przerwana i wywoła się quit_client
        int n = read(STDIN_FILENO, buffer, 31);
        if (n <= 0) continue;
        buffer[n-1] = '\0';
        int choice = str_to_i(buffer);

        if (choice == 1) {
            kill(ui_pid, SIGSTOP);
            print_s("\n[BUDOWA] Typ (0-3): "); n = read(STDIN_FILENO, buffer, 31);
            if (n>0) buffer[n-1] = '\0';
            m.mtype = CHAN_SERVER; m.player_id = my_player_id; m.action = ACT_BUILD;
            m.u_type = str_to_i(buffer);
            print_s("Ilosc: "); n = read(STDIN_FILENO, buffer, 31);
            if (n>0) buffer[n-1] = '\0';
            m.u_count = str_to_i(buffer);
            msgsnd(mid, &m, sizeof(Msg)-sizeof(long), 0);
            kill(ui_pid, SIGCONT);
        } else if (choice == 2) {
            kill(ui_pid, SIGSTOP);
            print_s("\n[ATAK] Jednostki:\n");
            m.mtype = CHAN_SERVER; m.player_id = my_player_id; m.action = ACT_ATTACK;
            for(int i=0; i<3; i++) {
                print_s(" "); print_s(STATS[i].name); print_s(": ");
                n = read(STDIN_FILENO, buffer, 31); if (n>0) buffer[n-1] = '\0';
                m.army[i] = str_to_i(buffer);
            }
            msgsnd(mid, &m, sizeof(Msg)-sizeof(long), 0);
            kill(ui_pid, SIGCONT);
        }
    }
    return 0;
}