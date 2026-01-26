#include "common.h"

int msg_id;
int moje_id;
pid_t pid_dziecka; 

void czyszczenie(int sig) {
    if (pid_dziecka > 0) kill(pid_dziecka, SIGKILL);
    // Przywracamy kursor na widoczny przy wyjściu (dla porządku)
    printf("\033[?25h");
    exit(0);
}

// Funkcja pomocnicza do rysowania interfejsu bez czyszczenia wpisywanego tekstu
void rysuj_interfejs(Komunikat *msg) {
    // \033[s - Zapisz pozycję kursora (tam gdzie użytkownik pisze)
    // \033[1;1H - Przesuń kursor na górę ekranu (linia 1, kolumna 1)
    // \033[K - Wyczyść linię do końca (żeby stare śmieci nie zostały)
    
    printf("\033[s"); 
    
    printf("\033[1;1H=== GRACZ %d ===\033[K\n", moje_id);
    
    if (msg->akcja == 1) printf("\033[1;32m!!! ZWYCIESTWO !!!\033[0m\033[K\n");
    else if (msg->akcja == 2) printf("\033[1;31m!!! PRZEGRANA !!!\033[0m\033[K\n");
    else printf("Stan gry: W toku...\033[K\n");

    printf("Zloto: %d\033[K\n", msg->wartosc);
    printf("Armia: [0]Lekka: %d  [1]Ciezka: %d  [2]Jazda: %d  [3]Robotnicy: %d\033[K\n",
           msg->dane[0], msg->dane[1], msg->dane[2], msg->dane[3]);
    printf("------------------------------------------------\033[K\n");
    printf("ROZKAZY: 1. Buduj  2. Atak\033[K\n");
    printf("Twoj wybor > \033[K"); // Nie dajemy \n, żeby kursor nie uciekał
    
    // \033[u - Przywróć kursor tam, gdzie był (do wpisywania)
    printf("\033[u");
    fflush(stdout);
}

int main() {
    signal(SIGINT, czyszczenie);
    key_t klucz = KLUCZ;
    msg_id = msgget(klucz, 0600);
    if (msg_id == -1) { printf("Brak serwera! Uruchom ./serwer\n"); return 1; }

    // Logowanie
    Komunikat msg;
    msg.mtype = 10;
    msg.akcja = MSG_LOGIN;
    msg.dane[0] = getpid();
    msgsnd(msg_id, &msg, sizeof(msg)-sizeof(long), 0);

    msgrcv(msg_id, &msg, sizeof(msg)-sizeof(long), getpid(), 0);
    if (msg.wartosc == 0) { printf("Serwer pelny!\n"); return 0; }
    
    moje_id = msg.id_nadawcy;
    
    // Czyścimy ekran RAZ na początku
    system("clear");
    // Robimy miejsce na interfejs (6 linii pustych), żeby kursor wpisywania był niżej
    printf("\n\n\n\n\n\n"); 

    pid_dziecka = fork();

    if (pid_dziecka == 0) {
        // --- DZIECKO: Odświeżanie stanu (BEZ system("clear")) ---
        while(1) {
            msg.mtype = 10;
            msg.akcja = MSG_STAN;
            msg.id_nadawcy = moje_id;
            msgsnd(msg_id, &msg, sizeof(msg)-sizeof(long), 0);

            if (msgrcv(msg_id, &msg, sizeof(msg)-sizeof(long), moje_id + 20, 0) != -1) {
                if (msg.akcja == 1 || msg.akcja == 2) {
                    // W przypadku końca gry, wymuszamy wyczyszczenie i wypisanie wyniku
                    kill(getppid(), SIGKILL); // Zabijamy proces rodzica (wpisywanie)
                    system("clear");
                    if(msg.akcja == 1) printf("\n\n   !!! ZWYCIESTWO !!!   \n\n");
                    else printf("\n\n   !!! PRZEGRANA !!!   \n\n");
                    exit(0);
                }
                rysuj_interfejs(&msg);
            }
            sleep(1);
        }
    } else {
        // --- RODZIC: Wczytywanie klawiszy ---
        int opcja;
        while(1) {
            // scanf blokuje czekając na Enter. Dzięki kodom ANSI w dziecku,
            // tekst wpisywany tutaj nie będzie znikał.
            if (scanf("%d", &opcja) != 1) {
                while(getchar() != '\n'); // Czyszczenie bufora w razie błędu
                continue;
            }

            if (opcja == 1 || opcja == 2) {
                // Zatrzymujemy odświeżanie na czas wpisywania szczegółów
                kill(pid_dziecka, SIGSTOP); 
                
                // Czyścimy dół ekranu pod interfejsem dla czytelności
                // Przesuwamy kursor pod interfejs statystyk
                printf("\033[8;1H\033[J"); // Skok do linii 8 i czyszczenie w dół

                if (opcja == 1) {
                    int typ, ilosc;
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
                    printf("-> Rozkaz budowy wyslany.\n");
                }
                else if (opcja == 2) {
                    int l, c, j;
                    printf("--- ATAK ---\n");
                    printf("Podaj ilosc (Lekka Ciezka Jazda): ");
                    scanf("%d %d %d", &l, &c, &j);

                    msg.mtype = 10;
                    msg.akcja = MSG_ATAK;
                    msg.id_nadawcy = moje_id;
                    msg.dane[0] = l; msg.dane[1] = c; msg.dane[2] = j; msg.dane[3] = 0;
                    msgsnd(msg_id, &msg, sizeof(msg)-sizeof(long), 0);
                    printf("-> Rozkaz ataku wyslany.\n");
                }
                
                sleep(1); // Czas na przeczytanie komunikatu
                
                // Wznawiamy odświeżanie
                // Dziecko zaraz nadpisze ekran statystykami, co jest OK
                kill(pid_dziecka, SIGCONT);
                
                // Opcjonalnie: Przesuwamy kursor w dół, żeby scanf był gotowy na nowo
                // (choć funkcja rysuj_interfejs i tak ustawi kursor, to bezpiecznik)
            }
        }
    }
    return 0;
}