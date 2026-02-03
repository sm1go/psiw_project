#include "common.h"

int mid, sid, semid;
GameData *game;

// Funkcja zwalniająca zasoby systemowe
void cleanup_server(int s) {
    msgctl(mid, IPC_RMID, NULL); // Usuwa kolejkę komunikatów
    shmctl(sid, IPC_RMID, NULL); // Usuwa pamięć współdzieloną
    semctl(semid, 0, IPC_RMID);  // Usuwa semafor
    print_s("[SERWER] Zasoby zwolnione. Koniec pracy.\n");
    exit(0);
}

int main() {
    signal(SIGINT, cleanup_server);
    signal(SIGCHLD, SIG_IGN);

    // Inicjalizacja zasobów IPC
    mid = msgget(PROJECT_KEY, IPC_CREAT | 0600);
    sid = shmget(PROJECT_KEY, sizeof(GameData), IPC_CREAT | 0600);
    semid = semget(PROJECT_KEY, 1, IPC_CREAT | 0600);
    
    union semun a; a.val = 1;
    semctl(semid, 0, SETVAL, a);

    game = (GameData*)shmat(sid, NULL, 0);
    memset(game, 0, sizeof(GameData));
    game->gold[0] = 300; game->gold[1] = 300;

    print_s("[SERWER] Gra strategiczna 2025 uruchomiona.\n");

    // Proces przyrostu surowców (wykonuje się w tle)
    if (fork() == 0) { 
        while(1) {
            sleep(1);
            lock(semid);
            if (game->running) {
                for(int i=0; i<2; i++) 
                    game->gold[i] += 50 + (game->army[i][3] * 5);
            }
            unlock(semid);
        }
    }

    int players_count = 0;
    Msg m;
    while(1) {
        // Oczekiwanie na komunikat od klienta
        if (msgrcv(mid, &m, sizeof(Msg)-sizeof(long), CHAN_SERVER, 0) == -1) continue;

        if (m.action == ACT_LOGIN) {
            lock(semid);
            long return_pid = m.sender_pid;
            if (players_count < 2) {
                m.player_id = ++players_count;
                if (players_count == 2) game->running = 1;
                print_s("Gracz "); print_i(m.player_id); print_s(" zalogowany.\n");
            } else m.player_id = -1;
            m.mtype = return_pid;
            msgsnd(mid, &m, sizeof(Msg)-sizeof(long), 0);
            unlock(semid);
        } 
       // ... (początek pliku bez zmian) ...

        else if (m.action == ACT_STATE) {
            int p = m.player_id - 1;
            int enemy = (p == 0) ? 1 : 0;
            
            lock(semid);
            m.mtype = (m.player_id == 1) ? CHAN_P1 : CHAN_P2;
            m.gold = game->gold[p];
            m.score = game->score[p];
            m.active = game->running;
            for(int i=0; i<4; i++) m.army[i] = game->army[p][i];
            
            // Jeśli przeciwnik wygrał, ustawiamy -1 (znacznik porażki)
            if (game->score[enemy] >= WIN_THRESHOLD) m.score = -1; 
            
            // NAJPIERW WYŚLIJ, POTEM CZEKAJ NA ZAMKNIĘCIE
            msgsnd(mid, &m, sizeof(Msg)-sizeof(long), 0);

            // Sprawdzamy czy gra się skończyła
            if (game->score[p] >= WIN_THRESHOLD || game->score[enemy] >= WIN_THRESHOLD) {
                print_s("[SERWER] Wykryto koniec gry. Zamykanie za 3 sekundy...\n");
                unlock(semid);
                sleep(3); // Dajemy czas OBU klientom na odebranie ostatniej wiadomości
                cleanup_server(0);
            }
            unlock(semid);
        }
       
        else if (m.action == ACT_BUILD) {
            int p = m.player_id - 1;
            int total_cost = STATS[m.u_type].cost * m.u_count;
            lock(semid);
            if (p >= 0 && game->gold[p] >= total_cost) {
                game->gold[p] -= total_cost;
                unlock(semid);
                if (fork() == 0) { // Proces budowy jednostek
                    for(int i=0; i<m.u_count; i++) {
                        sleep(STATS[m.u_type].time);
                        lock(semid); game->army[p][m.u_type]++; unlock(semid);
                    }
                    exit(0);
                }
            } else unlock(semid);
        }
        else if (m.action == ACT_ATTACK) {
            int p = m.player_id - 1;
            int enemy = (p == 0) ? 1 : 0;
            lock(semid);
            int can_atk = 1;
            for(int i=0; i<3; i++) if(game->army[p][i] < m.army[i]) can_atk = 0;
            if (can_atk) {
                for(int i=0; i<3; i++) game->army[p][i] -= m.army[i];
                unlock(semid);
                if (fork() == 0) { // Proces bitwy
                    int squad[4]; for(int i=0; i<4; i++) squad[i] = m.army[i];
                    sleep(5);
                    lock(semid);
                    float sa=0, sb=0, esa=0, esb=0;
                    for(int i=0; i<3; i++) {
                        sa += squad[i] * STATS[i].atk; 
                        sb += game->army[enemy][i] * STATS[i].def;
                        esa += game->army[enemy][i] * STATS[i].atk; 
                        esb += squad[i] * STATS[i].def;
                    }
                    if (sa > sb) { 
                        game->score[p]++; // Aktualizacja wyniku w pamięci współdzielonej
                        for(int i=0; i<3; i++) game->army[enemy][i] = 0; 
                    } else { 
                        float r = (sb > 0) ? sa/sb : 0; 
                        for(int i=0; i<3; i++) game->army[enemy][i] -= (int)(game->army[enemy][i]*r); 
                    }
                    if (esa > esb) { for(int i=0; i<3; i++) squad[i] = 0; }
                    else { 
                        float r = (esb > 0) ? esa/esb : 0; 
                        for(int i=0; i<3; i++) squad[i] -= (int)(squad[i]*r); 
                    }
                    for(int i=0; i<3; i++) game->army[p][i] += squad[i];
                    unlock(semid); exit(0);
                }
            } else unlock(semid);
        }
    }
}