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

/* --- DEFINICJE --- */
#define MSG_LOGOWANIE 10
#define MSG_BUDUJ     11
#define MSG_ATAK      12
#define MSG_STAN      13
#define MSG_WYNIK     14
#define MSG_START     15

#define KLUCZ 12345

// Struktura wiadomosci
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

// Stan gry (Shared Memory)
typedef struct {
    int resources[3];      // resources[1]=Gracz1, [2]=Gracz2
    int units[3][4];       
    int score[3];          
    int players_count;     
    int player_pids[3];    
} GameState;

// Statystyki
int COST[] = {100, 250, 550, 150};
double ATTACK_VAL[] = {1.0, 1.5, 3.5, 0.0};
double DEFENSE_VAL[] = {1.2, 3.0, 1.2, 0.0};

int sem_id;
int shm_id;
int msg_id;
GameState *gs;

// --- FUNKCJE POMOCNICZE (zamiast printf) ---
void print_log(const char *s) {
    write(1, s, strlen(s));
}

void sem_lock() {
    struct sembuf sb = {0, -1, 0};
    semop(sem_id, &sb, 1);
}

void sem_unlock() {
    struct sembuf sb = {0, 1, 0};
    semop(sem_id, &sb, 1);
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
    for(int i=1; i<=2; i++) {
        gs->resources[i] = 300; 
        gs->score[i] = 0;
        gs->player_pids[i] = 0;
        for(int j=0; j<4; j++) gs->units[i][j] = 0;
    }
    sem_unlock();
}

void resolve_combat(int attacker_id, int *attack_units) {
    int defender_id = (attacker_id == 1) ? 2 : 1;
    
    double SA = 0; 
    for(int i=0; i<3; i++) SA += attack_units[i] * ATTACK_VAL[i];

    double SB = 0; 
    for(int i=0; i<3; i++) SB += gs->units[defender_id][i] * DEFENSE_VAL[i];

    if (SA > SB) {
        gs->score[attacker_id]++;
        for(int i=0; i<3; i++) gs->units[defender_id][i] = 0;
    } else {
        if (SB > 0) {
            for(int i=0; i<3; i++) {
                int lost = (int)(gs->units[defender_id][i] * (SA/SB));
                gs->units[defender_id][i] -= lost;
                if (gs->units[defender_id][i] < 0) gs->units[defender_id][i] = 0;
            }
        }
    }
}

int main() {
    signal(SIGINT, cleanup);

    // Tworzenie IPC
    msg_id = msgget(KLUCZ, IPC_CREAT | 0600);
    shm_id = shmget(KLUCZ, sizeof(GameState), IPC_CREAT | 0600);
    sem_id = semget(KLUCZ, 1, IPC_CREAT | 0600);

    semctl(sem_id, 0, SETVAL, 1); // Semafor otwarty

    gs = (GameState*)shmat(shm_id, NULL, 0);
    init_game();

    print_log("Serwer gotowy. Czekam na graczy...\n");

    if (fork() == 0) {
        // --- PROCES DZIECKO (Surowce) ---
        while(1) {
            sleep(1);
            sem_lock();
            if (gs->players_count > 0) {
                for(int i=1; i<=2; i++) {
                    int gain = 50 + (gs->units[i][3] * 5);
                    gs->resources[i] += gain;
                }
            }
            sem_unlock();
        }
    } else {
        // --- PROCES RODZIC (Komunikaty) ---
        struct msgbuf msg, response;
        
        while(1) {
            // Odbieramy komunikaty typu -20 (czyli priorytetowe i zwykłe, byle < 20)
            // Dzięki temu nie odbierzemy komunikatów PID (duże liczby) jeśli jakieś są.
            if (msgrcv(msg_id, &msg, sizeof(msg) - sizeof(long), -20, 0) != -1) {
                
                sem_lock();
                
                // 1. LOGOWANIE (Tu odsyłamy na PID!)
                if (msg.mtype == MSG_LOGOWANIE) {
                    response.mtype = msg.liczba; // Odsyłamy na PID procesu klienta
                    if (gs->players_count < 2) {
                        gs->players_count++;
                        int new_id = gs->players_count;
                        gs->player_pids[new_id] = msg.liczba;
                        response.id_gracza = new_id;
                        print_log("Nowy gracz dolaczyl.\n");
                    } else {
                        response.id_gracza = -1; 
                    }
                    msgsnd(msg_id, &response, sizeof(response) - sizeof(long), 0);
                }
                
                // 2. START GRY (Tu odsyłamy na ID GRACZA - 1 lub 2!)
                else if (msg.mtype == MSG_START) {
                    response.mtype = msg.id_gracza; // Klient czeka na mtype=1 lub 2
                    response.liczba = (gs->players_count == 2) ? 1 : 0;
                    msgsnd(msg_id, &response, sizeof(response) - sizeof(long), 0);
                }

                // 3. STAN GRY
                else if (msg.mtype == MSG_STAN) {
                    response.mtype = msg.id_gracza;
                    response.punkty = gs->score[msg.id_gracza];
                    response.zasoby = gs->resources[msg.id_gracza];
                    for(int i=0; i<4; i++) response.jednostki[i] = gs->units[msg.id_gracza][i];
                    msgsnd(msg_id, &response, sizeof(response) - sizeof(long), 0);
                }

                // 4. BUDOWANIE
                else if (msg.mtype == MSG_BUDUJ) {
                    int p_id = msg.id_gracza;
                    int u_idx = msg.indeks_jednostki;
                    int count = msg.liczba;
                    int total_cost = COST[u_idx] * count;
                    
                    response.mtype = p_id; // Odsyłamy na 1 lub 2

                    if (gs->resources[p_id] >= total_cost) {
                        gs->resources[p_id] -= total_cost;
                        gs->units[p_id][u_idx] += count;
                        response.liczba = 1; 
                    } else {
                        response.liczba = 0; 
                    }
                    msgsnd(msg_id, &response, sizeof(response) - sizeof(long), 0);
                }

                // 5. ATAK
                else if (msg.mtype == MSG_ATAK) {
                    int p_id = msg.id_gracza;
                    response.mtype = p_id;
                    
                    int possible = 1;
                    for(int i=0; i<3; i++) {
                        if (gs->units[p_id][i] < msg.jednostki[i]) possible = 0;
                    }

                    if (possible) {
                        for(int i=0; i<3; i++) gs->units[p_id][i] -= msg.jednostki[i];
                        resolve_combat(p_id, msg.jednostki);
                        response.liczba = 1;
                    } else {
                        response.liczba = 0;
                    }
                    msgsnd(msg_id, &response, sizeof(response) - sizeof(long), 0);
                }

                // 6. WYNIK
                else if (msg.mtype == MSG_WYNIK) {
                    int p_id = msg.id_gracza;
                    int opp_id = (p_id == 1) ? 2 : 1;
                    response.mtype = p_id;
                    
                    if (gs->score[p_id] >= 5) response.liczba = 1;     
                    else if (gs->score[opp_id] >= 5) response.liczba = 2; 
                    else response.liczba = 0;                             
                    
                    msgsnd(msg_id, &response, sizeof(response) - sizeof(long), 0);
                }

                sem_unlock();
            }
        }
    }
    return 0;
}