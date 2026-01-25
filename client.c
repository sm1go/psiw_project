#include "common.h"



void clear_screen(){

}


void print_dashboard(GameMsg *state) {
    clear_screen();
    printf("==========================================\n");
    printf("     GRA STRATEGICZNA - GRACZ %d\n", my_id + 1);
    printf("==========================================\n");
    printf(" ZŁOTO: \033[1;33m%d\033[0m  |  PUNKTY: %d (Wróg: %d)\n", 
           state->gold_now, state->score_me, state->score_enemy);
    printf("------------------------------------------\n");
    printf(" TWOJA ARMIA:\n");
    printf(" [0] Lekka Piechota: %d  (Atk: 1.0, Def: 1.2)\n", state->units_now[LIGHT]);
    printf(" [1] Ciężka Piechota:%d  (Atk: 1.5, Def: 3.0)\n", state->units_now[HEAVY]);
    printf(" [2] Jazda:          %d  (Atk: 3.5, Def: 1.2)\n", state->units_now[CAVALRY]);
    printf(" [3] Robotnicy:      %d  (+5 złoto/s)\n", state->units_now[WORKER]);
    printf("==========================================\n");
    printf(" KOMENDY:\n");
    printf(" b [typ] [ilość]  -> Buduj (np. b 0 5)\n");
    printf(" a [L] [C] [J]    -> Atakuj (np. a 5 2 1)\n");
    printf(" q                -> Wyjdź\n");
    printf("==========================================\n");
    printf(" Ostatni raport: \033[1;31m%s\033[0m\n", state->text);
    printf("> ");
    fflush(stdout);


void receiver(){ //sluchanie z serwera

}


int main(){
    // logowanie, petla do wejscia, ataki z wyslaniem do serwera, okreslenie jednostek 
}