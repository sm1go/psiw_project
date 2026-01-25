#include "common.h"

int msg_id;
int moje_id;
pid_t pid_dziecka;

void czyszczenie(int sig) {
    if (pid_dziecka > 0) kill(pid_dziecka, SIGKILL);
    exit(0);
}

int main() {
    signal(SIGINT, czyszczenie);
    key_t klucz = KLUCZ;
    msg_id = msgget(klucz, 0600);
    if (msg_id == -1) { printf("Brak serwera!\n"); return 1; }

    Komunikat msg;
    msg.mtype = 10;
    msg.akcja = MSG_LOGIN;
    msg.dane[0] = getpid();
    msgsnd(msg_id, &msg, sizeof(msg)-sizeof(long), 0);

    msgrcv(msg_id, &msg, sizeof(msg)-sizeof(long), getpid(), 0);
    if (msg.wartosc == 0) { printf("Serwer pelny!\n"); return 0; }
    
    moje_id = msg.id_nadawcy;
    printf("Zalogowano jako gracz %d. Start gry...\n", moje_id);

    pid_dziecka = fork();

    if (pid_dziecka == 0) {
        while(1) {
            msg.mtype = 10;
            msg.akcja = MSG_STAN;
            msg.id_nadawcy = moje_id;
            msgsnd(msg_id, &msg, sizeof(msg)-sizeof(long), 0);

            if (msgrcv(msg_id, &msg, sizeof(msg)-sizeof(long), moje_id + 20, 0) != -1) {
                system("clear"); 
                printf("=== GRACZ %d ===\n", moje_id);
                if (msg.akcja == 1) { printf("\n!!! ZWYCIESTWO !!!\n"); kill(getppid(), SIGKILL); exit(0); }
                if (msg.akcja == 2) { printf("\n!!! PRZEGRANA !!!\n"); kill(getppid(), SIGKILL); exit(0); }

                printf("Złoto: %d\n", msg.wartosc);
                printf("Armia: [0]Lekka: %d  [1]Ciezka: %d  [2]Jazda: %d  [3]Robotnicy: %d\n",
                       msg.dane[0], msg.dane[1], msg.dane[2], msg.dane[3]);
                printf("------------------------------------------------\n");
                printf("ROZKAZY: 1. Buduj  2. Atak\n");
                printf("Twoj wybor: ");
                fflush(stdout);
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
                system("clear"); 

                if (opcja == 1) {
                    int typ, ilosc;
                    printf("=== BUDOWANIE ===\n");
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
                    printf("Wysłano rozkaz budowy.\n");
                }
                else if (opcja == 2) {
                    int l, c, j;
                    printf("=== ATAK ===\n");
                    printf("Podaj ilosc (Lekka Ciezka Jazda): ");
                    scanf("%d %d %d", &l, &c, &j);

                    msg.mtype = 10;
                    msg.akcja = MSG_ATAK;
                    msg.id_nadawcy = moje_id;
                    msg.dane[0] = l; msg.dane[1] = c; msg.dane[2] = j; msg.dane[3] = 0;
                    msgsnd(msg_id, &msg, sizeof(msg)-sizeof(long), 0);
                    printf("Wysłano rozkaz ataku.\n");
                }
                
                sleep(1); 
                kill(pid_dziecka, SIGCONT);
            }
        }
    }
    return 0;
}