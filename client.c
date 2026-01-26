#include "common.h"

int msg_id;
int moje_id;
pid_t pid_dziecka; 

void czyszczenie(int sig) {
    if (pid_dziecka > 0) kill(pid_dziecka, SIGKILL);
    printf("\033[?25h");
    exit(0);
}

void aktualizuj_statystyki(Komunikat *msg) {
    printf("\0337\033[?25l");

    printf("\033[1;1H=== GRACZ %d ===\033[K", moje_id);
    
    if (msg->akcja == 1) printf(" \033[1;32m[ ZWYCIESTWO ]\033[0m");
    else if (msg->akcja == 2) printf(" \033[1;31m[ PRZEGRANA ]\033[0m");

    printf("\033[2;1HZloto: %d          ", msg->wartosc);

    printf("\033[3;1HArmia: [L: %d] [C: %d] [J: %d] [R: %d]          ",
           msg->dane[0], msg->dane[1], msg->dane[2], msg->dane[3]);

    printf("\0338\033[?25h");
    fflush(stdout);
}

void rysuj_menu_glowne() {
    system("clear");
    printf("\n\n\n"); 
    printf("------------------------------------------------\n");
    printf("ROZKAZY: 1. Buduj  2. Atak\n");
    printf("Twoj wybor > ");
    fflush(stdout);
}

int main() {
    signal(SIGINT, czyszczenie);
    key_t klucz = KLUCZ;
    msg_id = msgget(klucz, 0600);
    if (msg_id == -1) { printf("Brak serwera! Uruchom ./serwer\n"); return 1; }

    Komunikat msg;
    msg.mtype = 10;
    msg.akcja = MSG_LOGIN;
    msg.dane[0] = getpid();
    msgsnd(msg_id, &msg, sizeof(msg)-sizeof(long), 0);

    msgrcv(msg_id, &msg, sizeof(msg)-sizeof(long), getpid(), 0);
    if (msg.wartosc == 0) { printf("Serwer pelny!\n"); return 0; }
    
    moje_id = msg.id_nadawcy;
    
    rysuj_menu_glowne();

    pid_dziecka = fork();

    if (pid_dziecka == 0) {
        while(1) {
            msg.mtype = 10;
            msg.akcja = MSG_STAN;
            msg.id_nadawcy = moje_id;
            msgsnd(msg_id, &msg, sizeof(msg)-sizeof(long), 0);

            if (msgrcv(msg_id, &msg, sizeof(msg)-sizeof(long), moje_id + 20, 0) != -1) {
                if (msg.akcja == 1 || msg.akcja == 2) {
                    kill(getppid(), SIGKILL);
                    system("clear");
                    if(msg.akcja == 1) printf("\n\n   !!! ZWYCIESTWO !!!   \n\n");
                    else printf("\n\n   !!! PRZEGRANA !!!   \n\n");
                    exit(0);
                }
                aktualizuj_statystyki(&msg);
            }
            sleep(1);
        }
    } else {
        int opcja;
        while(1) {
            if (scanf("%d", &opcja) != 1) {
                while(getchar() != '\n'); 
                continue;
            }

            if (opcja == 1 || opcja == 2) {
                kill(pid_dziecka, SIGSTOP); 

                if (opcja == 1) {
                    int typ, ilosc;
                    printf("\033[7;1H\033[J"); 
                    printf("--- BUDOWANIE ---\n");
                    printf("Typ (0-Lekka, 1-Ciezka, 2-Jazda, 3-Rob): ");
                    scanf("%d", &typ);
                    printf("Ilosc: ");
                    scanf("%d", &ilosc);
                    
                    msg.mtype = 10;
                    msg.akcja = MSG_BUDUJ;
                    msg.id_nadawcy = moje_id;
                    msg.dane[0] = typ;
                    msg.dane[1] = ilosc;
                    msgsnd(msg_id, &msg, sizeof(msg)-sizeof(long), 0);
                    printf("-> Wyslano rozkaz budowy.");
                }
                else if (opcja == 2) {
                    int l, c, j;
                    printf("\033[7;1H\033[J"); 
                    printf("--- ATAK ---\n");
                    printf("Ilosc (Lekka Ciezka Jazda): ");
                    scanf("%d %d %d", &l, &c, &j);

                    msg.mtype = 10;
                    msg.akcja = MSG_ATAK;
                    msg.id_nadawcy = moje_id;
                    msg.dane[0] = l; msg.dane[1] = c; msg.dane[2] = j; msg.dane[3] = 0;
                    msgsnd(msg_id, &msg, sizeof(msg)-sizeof(long), 0);
                    printf("-> Wyslano rozkaz ataku.");
                }
                
                sleep(1); 
                
                rysuj_menu_glowne();
                
                kill(pid_dziecka, SIGCONT);
            }
        }
    }
    return 0;
}