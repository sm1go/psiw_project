#include "common.h"

int msg_id, shm_id, sem_id;
SharedGameState *game_state;

void shutdown_server(int sig) {
    msgctl(msg_id, IPC_RMID, NULL);
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    write_str(STDOUT_FILENO, "\nSerwer wylaczony.\n");
    exit(0);
}

int main() {
    signal(SIGINT, shutdown_server);
    signal(SIGCHLD, SIG_IGN);

    // Czyszczenie starych zasobów o tym samym kluczu przed startem
    msg_id = msgget(PROJECT_KEY, 0600);
    if (msg_id != -1) msgctl(msg_id, IPC_RMID, NULL);

    msg_id = msgget(PROJECT_KEY, IPC_CREAT | 0600);
    shm_id = shmget(PROJECT_KEY, sizeof(SharedGameState), IPC_CREAT | 0600);
    sem_id = semget(PROJECT_KEY, 1, IPC_CREAT | 0600);

    union semun arg;
    arg.val = 1;
    semctl(sem_id, 0, SETVAL, arg);

    game_state = (SharedGameState*)shmat(shm_id, NULL, 0);
    memset(game_state, 0, sizeof(SharedGameState));
    game_state->player_resources[0] = 300;
    game_state->player_resources[1] = 300;

    write_str(STDOUT_FILENO, "Serwer uruchomiony. Czekam na graczy...\n");

    if (fork() == 0) { // Produkcja złota
        while(1) {
            sleep(1);
            lock_shm(sem_id);
            if (game_state->is_game_started) {
                for(int i=0; i<2; i++) {
                    game_state->player_resources[i] += 50 + (game_state->player_army[i][3] * 5);
                }
            }
            unlock_shm(sem_id);
        }
    }

    int players_ready = 0;
    GameMessage msg;

    while(1) {
        // Serwer słucha na mtype = 100
        if (msgrcv(msg_id, &msg, sizeof(GameMessage)-sizeof(long), 100, 0) == -1) continue;

        if (msg.action == ACTION_LOGIN) {
            lock_shm(sem_id);
            long return_type = msg.player_id; // PID klienta
            if (players_ready < 2) {
                msg.player_id = ++players_ready;
                if (players_ready == 2) game_state->is_game_started = 1;
                write_str(STDOUT_FILENO, "Gracz zalogowany. ID: ");
                write_int(STDOUT_FILENO, msg.player_id); write_str(STDOUT_FILENO, "\n");
            } else msg.player_id = -1;
            
            msg.mtype = return_type;
            msgsnd(msg_id, &msg, sizeof(GameMessage)-sizeof(long), 0);
            unlock_shm(sem_id);
        } 
        else if (msg.action == ACTION_STATE) {
            int p = msg.player_id - 1;
            lock_shm(sem_id);
            msg.mtype = (long)msg.player_id + 200; // Unikalny kanał zwrotny dla stanu
            msg.resource_info = game_state->player_resources[p];
            msg.score_info = game_state->player_scores[p];
            msg.game_status = game_state->is_game_started;
            for(int i=0; i<4; i++) msg.army_info[i] = game_state->player_army[p][i];
            
            int enemy = (p == 0) ? 1 : 0;
            if (game_state->player_scores[enemy] >= WIN_THRESHOLD) msg.score_info = -1; 

            msgsnd(msg_id, &msg, sizeof(GameMessage)-sizeof(long), 0);
            unlock_shm(sem_id);
        }
        else if (msg.action == ACTION_BUILD) {
            int p = msg.player_id - 1;
            int total_cost = UNIT_STATS[msg.unit_type].cost * msg.quantity;
            lock_shm(sem_id);
            if (game_state->player_resources[p] >= total_cost && msg.unit_type >= 0 && msg.unit_type <= 3) {
                game_state->player_resources[p] -= total_cost;
                unlock_shm(sem_id);
                if (fork() == 0) {
                    for(int i=0; i<msg.quantity; i++) {
                        sleep(UNIT_STATS[msg.unit_type].build_time);
                        lock_shm(sem_id);
                        game_state->player_army[p][msg.unit_type]++;
                        unlock_shm(sem_id);
                    }
                    exit(0);
                }
            } else unlock_shm(sem_id);
        }
        else if (msg.action == ACTION_ATTACK) {
            int p = msg.player_id - 1;
            int enemy = (p == 0) ? 1 : 0;
            lock_shm(sem_id);
            int can_go = 1;
            for(int i=0; i<3; i++) if(game_state->player_army[p][i] < msg.army_info[i]) can_go = 0;

            if(can_go) {
                for(int i=0; i<3; i++) game_state->player_army[p][i] -= msg.army_info[i];
                unlock_shm(sem_id);
                if(fork() == 0) {
                    int attackers[4];
                    for(int i=0; i<4; i++) attackers[i] = msg.army_info[i];
                    sleep(5); 
                    lock_shm(sem_id);
                    float SA = 0, SB = 0, ESA = 0, ESB = 0;
                    for(int i=0; i<3; i++) {
                        SA += attackers[i] * UNIT_STATS[i].attack_val;
                        SB += game_state->player_army[enemy][i] * UNIT_STATS[i].defense_val;
                        ESA += game_state->player_army[enemy][i] * UNIT_STATS[i].attack_val;
                        ESB += attackers[i] * UNIT_STATS[i].defense_val;
                    }
                    if (SA > SB) {
                        game_state->player_scores[p]++;
                        for(int i=0; i<3; i++) game_state->player_army[enemy][i] = 0;
                    } else {
                        float r = (SB > 0) ? SA/SB : 0;
                        for(int i=0; i<3; i++) game_state->player_army[enemy][i] -= (int)(game_state->player_army[enemy][i] * r);
                    }
                    if (ESA > ESB) { for(int i=0; i<3; i++) attackers[i] = 0; }
                    else {
                        float r = (ESB > 0) ? ESA/ESB : 0;
                        for(int i=0; i<3; i++) attackers[i] -= (int)(attackers[i] * r);
                    }
                    for(int i=0; i<3; i++) game_state->player_army[p][i] += attackers[i];
                    unlock_shm(sem_id);
                    exit(0);
                }
            } else unlock_shm(sem_id);
        }
    }
}