#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

/* --- DEFINICJE ZGODNE Z KLIENTEM --- */
#define MSG_LOGOWANIE 10
#define MSG_BUDUJ     11
#define MSG_ATAK      12
#define MSG_STAN      13
#define MSG_WYNIK     14
#define MSG_START     15

#define KLUCZ 12345

// Struktura identyczna jak w Twoim kliencie
struct msgbuf {
    long mtype;
    int id_gracza;
    int wybor;
    int jednostki[4];
    int zasoby;
    int czas;
    int indeks_jednostki;
    int liczba;
    int punkty;
};

// Struktura stanu gry w pamięci współdzielonej
typedef struct {
    int resources[3];      // resources[1] dla gracza 1, [2] dla gracza 2
    int units[3][4];       // units[gracz][typ]
    int score[3];          // punkty
    int players_count;     // liczba zalogowanych
    int player_pids[3];    // PID-y graczy
    int game_started;
} GameState;

// Stałe gry
int COST[] = {100, 250, 550, 150};
double ATTACK_VAL[] = {1.0, 1.5, 3.5, 0.0};
double DEFENSE_VAL[] = {1.2, 3.0, 1.2, 0.0};

int sem_id;
int shm_id;
int msg_id;
GameState *gs;

/* --- FUNKCJE POMOCNICZE --- */

void sem_lock() {
    struct sembuf sb = {0, -1, 0};
    semop(sem_id, &sb, 1);
}

void sem_unlock() {
    struct sembuf sb = {0, 1, 0};
    semop(sem_id, &sb, 1);
}

void print_log(const char *s) {
    write(1, s, strlen(s));
}

void cleanup(int sig) {
    print_log("\nZamykanie serwera...\n");
    shmdt(gs);
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    msgctl(msg_id, IPC_RMID, NULL);
    exit(0);
}

void init_game() {
    sem_lock();
    gs->players_count = 0;
    gs->game_started = 0;
    for(int i=1; i<=2; i++) {
        gs->resources[i] = 300; // Startowe złoto
        gs->score[i] = 0;
        gs->player_pids[i] = 0;
        for(int j=0; j<4; j++) gs->units[i][j] = 0;
    }
    sem_unlock();
}

void resolve_combat(int attacker_id, int *attack_units) {
    int defender_id = (attacker_id == 1) ? 2 : 1;
    
    double SA = 0; // Siła ataku
    for(int i=0; i<3; i++) SA += attack_units[i] * ATTACK_VAL[i];

    double SB = 0; // Siła obrony
    for(int i=0; i<3; i++) SB += gs->units[defender_id][i] * DEFENSE_VAL[i];

    if (SA > SB) {
        gs->score[attacker_id]++;
        // Niszczymy wszystko obrońcy
        for(int i=0; i<3; i++) gs->units[defender_id][i] = 0;
    } else {
        // Straty obrońcy
        if (SB > 0) {
            for(int i=0; i<3; i++) {
                int lost = (int)(gs->units[defender_id][i] * (SA/SB));
                gs->units[defender_id][i] -= lost;
                if (gs->units[defender_id][i] < 0) gs->units[defender_id][i] = 0;
            }
        }
    }
    
    // Proste straty atakującego (traci wysłane jednostki - uproszczenie logiczne dla projektu)
    // W bardziej zaawansowanej wersji trzeba by liczyć straty zwrotne.
    // Tutaj atakujący traci to, co wysłał (bo wyszli z bazy i walczyli).
}

int main() {
    signal(SIGINT, cleanup);

    // 1. Tworzenie zasobów
    msg_id = msgget(KLUCZ, IPC_CREAT | 0600);
    shm_id = shmget(KLUCZ, sizeof(GameState), IPC_CREAT | 0600);
    sem_id = semget(KLUCZ, 1, IPC_CREAT | 0600);

    // Ustawienie semafora na 1 (otwarty)
    semctl(sem_id, 0, SETVAL, 1);

    gs = (GameState*)shmat(shm_id, NULL, 0);
    init_game();

    print_log("Serwer uruchomiony. Oczekiwanie na graczy...\n");

    if (fork() == 0) {
        // --- PROCES LOGIKI CZASOWEJ (ZŁOTO) ---
        while(1) {
            sleep(1);
            sem_lock();
            if (gs->players_count > 0) {
                for(int i=1; i<=2; i++) {
                    // 50 bazowo + 5 za robotnika
                    int gain = 50 + (gs->units[i][3] * 5);
                    gs->resources[i] += gain;
                }
            }
            sem_unlock();
        }
    } else {
        // --- PROCES OBSŁUGI KOMUNIKATÓW ---
        struct msgbuf msg, response;
        
        while(1) {
            // Odbieramy cokolwiek skierowanego do serwera (używamy typu 0 - FIFO, 
            // ale tutaj musimy uważać, żeby nie odebrać wiadomości skierowanych do klientów.
            // Klienci wysyłają typy: MSG_LOGOWANIE, MSG_BUDUJ itd.
            // Klienci nasłuchują na PID.
            // Serwer odbierze wszystko, co NIE jest PID-em klienta? 
            // Najbezpieczniej: serwer nasłuchuje na typach > 0, ale klienci wysyłają konkretne typy.
            // Użyjemy -100 (odbierz najmniejszy typ, ale < 100), zakładając że PIDy są duże.
            // Ale prościej: w msgrcv typ=0 odbiera pierwszą z kolejki.
            // Musimy jednak ignorować wiadomości, które serwer SAM wysłał do klientów (id=PID).
            // Rozwiązanie: Serwer czyta typy od 10 do 20 (nasze definicje). PIDy są zazwyczaj duże.
            
            if (msgrcv(msg_id, &msg, sizeof(msg) - sizeof(long), -20, 0) != -1) {
                
                sem_lock();
                
                // --- LOGOWANIE ---
                if (msg.mtype == MSG_LOGOWANIE) {
                    response.mtype = msg.liczba; // Odsyłamy na PID procesu
                    if (gs->players_count < 2) {
                        gs->players_count++;
                        int new_id = gs->players_count;
                        gs->player_pids[new_id] = msg.liczba;
                        response.id_gracza = new_id;
                        print_log("Zalogowano gracza.\n");
                    } else {
                        response.id_gracza = -1; // Serwer pełny
                    }
                    msgsnd(msg_id, &response, sizeof(response) - sizeof(long), 0);
                }
                
                // --- CZY MOŻNA GRAĆ (START) ---
                else if (msg.mtype == MSG_START) {
                    response.mtype = msg.id_gracza > 0 ? gs->player_pids[msg.id_gracza] : msg.liczba;
                    response.liczba = (gs->players_count == 2) ? 1 : 0;
                    msgsnd(msg_id, &response, sizeof(response) - sizeof(long), 0);
                }

                // --- STAN GRY ---
                else if (msg.mtype == MSG_STAN) {
                    int pid_dest = gs->player_pids[msg.id_gracza];
                    response.mtype = pid_dest;
                    response.punkty = gs->score[msg.id_gracza];
                    response.zasoby = gs->resources[msg.id_gracza];
                    for(int i=0; i<4; i++) response.jednostki[i] = gs->units[msg.id_gracza][i];
                    msgsnd(msg_id, &response, sizeof(response) - sizeof(long), 0);
                }

                // --- BUDOWANIE ---
                else if (msg.mtype == MSG_BUDUJ) {
                    int p_id = msg.id_gracza;
                    int u_idx = msg.indeks_jednostki;
                    int count = msg.liczba;
                    int total_cost = COST[u_idx] * count;
                    
                    int pid_dest = gs->player_pids[p_id];
                    response.mtype = pid_dest;

                    if (gs->resources[p_id] >= total_cost) {
                        gs->resources[p_id] -= total_cost;
                        // Natychmiastowe dodanie (uproszczenie, bo w kliencie nie ma kolejki czasu)
                        // Normalnie tu powinno być dodanie do kolejki produkcji.
                        gs->units[p_id][u_idx] += count;
                        response.liczba = 1; // OK
                    } else {
                        response.liczba = 0; // Brak kasy
                    }
                    msgsnd(msg_id, &response, sizeof(response) - sizeof(long), 0);
                }

                // --- ATAK ---
                else if (msg.mtype == MSG_ATAK) {
                    int p_id = msg.id_gracza;
                    int pid_dest = gs->player_pids[p_id];
                    response.mtype = pid_dest;
                    
                    // Sprawdzenie czy ma tyle jednostek
                    int possible = 1;
                    for(int i=0; i<3; i++) {
                        if (gs->units[p_id][i] < msg.jednostki[i]) possible = 0;
                    }

                    if (possible) {
                        // Odejmij jednostki
                        for(int i=0; i<3; i++) gs->units[p_id][i] -= msg.jednostki[i];
                        
                        resolve_combat(p_id, msg.jednostki);
                        response.liczba = 1;
                    } else {
                        response.liczba = 0;
                    }
                    msgsnd(msg_id, &response, sizeof(response) - sizeof(long), 0);
                }

                // --- WYNIK ---
                else if (msg.mtype == MSG_WYNIK) {
                    int p_id = msg.id_gracza;
                    int opp_id = (p_id == 1) ? 2 : 1;
                    int pid_dest = gs->player_pids[p_id];
                    response.mtype = pid_dest;
                    
                    if (gs->score[p_id] >= 5) response.liczba = 1;      // Wygrana
                    else if (gs->score[opp_id] >= 5) response.liczba = 2; // Przegrana
                    else response.liczba = 0;                             // Gra trwa
                    
                    msgsnd(msg_id, &response, sizeof(response) - sizeof(long), 0);
                }

                sem_unlock();
            }
        }
    }
    return 0;
}