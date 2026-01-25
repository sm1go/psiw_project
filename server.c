#include "common.h"

void sem_lock()

void sem_unlock()





void build(int player_id, int unit_type, int count){
    if (fork() == 0){
        for (int i = 0; i < count; i++){
            sleep(PRODUCTION_TIME[unit_type]);
            sem_lock();
            game->players[player_id].units[units_type]++;
            sem_unlock;
        }
        exit(0);
    }
}




void battle_result(){
    //symulacja walki, logika walki
}


void attack(){

}



void updates


void gold 

void cleanup

int main