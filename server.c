#include "common.h"

// Definicje kolorów ANSI dla lepszego wyglądu
#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_CYAN    "\033[36m"
#define C_BG_BLUE "\033[44m"

int msg_id;
int moje_id;
pid_t pid_dziecka; 

void czyszczenie(int sig) {
    if (pid_dziecka > 0) kill(pid_dziecka, SIGKILL);
    printf("\033[?25h"); // Pokaż kursor
    printf(C_RESET);     // Zresetuj kolory
    exit(0);
}

// Funkcja aktualizująca TYLKO górną część ekranu (Dashboard)
void aktualizuj_statystyki(Komunikat *msg) {
    // \033[s - Zapisz pozycję kursora
    // \033[?25l - Ukryj kursor
    printf("\033[s\033[?25l");

    // Linia 1: Nagłówek
    printf("\033[1;1H" C_BG_BLUE C_BOLD "  >>> CENTRUM DOWODZENIA GRACZA %d <<<  " C_RESET "\033[K", moje_id);
    
    // Linia 2: Status i Punkty
    printf("\033[2;1H" C_BOLD "  STATUS: " C_RESET);
    if (msg->akcja == 1) printf(C_GREEN "ZWYCIESTWO! " C_RESET);
    else if (msg->akcja == 2) printf(C_RED "PRZEGRANA!  " C_RESET);
    else printf("W TOKU      " C_RESET);
    
    printf(" | " C_CYAN "PUNKTY ZWYCIESTWA: %d / 5" C_RESET "\033[K", msg->punkty);

    // Linia 3: Separator
    printf("\033[3;1H------------------------------------------\033[K");

    // Linia 4: Zasoby
    printf("\033[4;1H  " C_YELLOW C_BOLD "$$$ ZLOTO: %-6d" C_RESET "\033[K", msg->wartosc);

    // Linia 5: Nagłówek Armii
    printf("\033[5;1H  " C_CYAN "ARMIA STACJONUJACA W BAZIE:" C_RESET "\033[K");

    // Linia 6: Tabela Armii (Wiersz 1)
    printf("\033[6;1H  [0] Lekka Piechota:  %4d  |  [2] Jazda:     %4d\033[K", 
           msg->dane[0], msg->dane[2]);

    // Linia 7: Tabela Armii (Wiersz 2)
    printf("\033[7;1H  [1] Ciezka Piechota: %4d  |  [3] Robotnicy: %4d\033[K", 
           msg->dane[1], msg->dane[3]);

    // Linia 8: Separator końcowy
    printf("\033[8;1H------------------------------------------\033[K");

    // \033[u - Przywróć kursor na dół
    // \033[?25h - Pokaż kursor
    printf("\033[u\033[?25h");
    fflush(stdout);
}

void rysuj_menu_glowne() {
    system("clear");
    // Rezerwujemy 8 linii na dashboard
    printf("\n\n\n\n\n\n\n\n\n"); 
    
    printf(C_BOLD " DOSTEPNE ROZKAZY:" C_RESET "\n");
    printf(" " C_GREEN "[1]" C_RESET " Buduj jednostki\n");
    printf(" " C_RED   "[2]" C_RESET " Atakuj wroga\n");
    printf("------------------------------------------\n");
    printf(C_BOLD " Twoj wybor > " C_RESET); 
    fflush(stdout);
}

int main() {
    setbuf(stdout, NULL);
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
        // --- PROCES DZIECKO (Odświeżanie) ---
        while(1) {
            msg.mtype = 10;
            msg.akcja = MSG_STAN;
            msg.id_nadawcy = moje_id;
            msgsnd(msg_id, &msg, sizeof(msg)-sizeof(long), 0);

            if (msgrcv(msg_id, &msg, sizeof(msg)-sizeof(long), moje_id + 20, 0) != -1) {
                if (msg.akcja == 1 || msg.akcja == 2) {
                    kill(getppid(), SIGKILL);
                    system("clear");
                    printf("\n\n");
                    if(msg.akcja == 1) 
                        printf(C_GREEN C_BOLD "\n\t   !!! GRATULACJE !!!\n\t   !!! ZWYCIESTWO !!!   \n\n" C_RESET);
                    else 
                        printf(C_RED C_BOLD "\n\t   !!! BAZA ZNISZCZONA !!!\n\t   !!! PRZEGRANA !!!   \n\n" C_RESET);
                    exit(0);
                }
                
                aktualizuj_statystyki(&msg);
            }
            usleep(500000); 
        }
    } else {
        // --- PROCES RODZIC (Sterowanie) ---
        int opcja;
        while(1) {
            if (scanf("%d", &opcja) != 1) {
                while(getchar() != '\n'); 
                continue;
            }

            if (opcja == 1 || opcja == 2) {
                kill(pid_dziecka, SIGSTOP); 

                // Czyścimy obszar pod menu (od linii 14 w dół)
                printf("\033[14;1H\033[J"); 

                if (opcja == 1) {
                    int typ, ilosc;
                    printf(C_GREEN " [BUDOWA]" C_RESET " Wybierz typ:\n");
                    printf(" 0-Lekka(100)  1-Ciezka(250)\n 2-Jazda(550)  3-Robotnik(150)\n > ");
                    scanf("%d", &typ);
                    printf(" Ilosc: ");
                    scanf("%d", &ilosc);
                    
                    msg.mtype = 10;
                    msg.akcja = MSG_BUDUJ;
                    msg.id_nadawcy = moje_id;
                    msg.dane[0] = typ;
                    msg.dane[1] = ilosc;
                    msgsnd(msg_id, &msg, sizeof(msg)-sizeof(long), 0);
                    printf(C_YELLOW " -> Rozkaz przyjeto." C_RESET);
                }
                else if (opcja == 2) {
                    int l, c, j;
                    printf(C_RED " [ATAK]" C_RESET " Podaj liczebnosc wojsk:\n");
                    printf(" Lekka Ciezka Jazda > ");
                    scanf("%d %d %d", &l, &c, &j);

                    msg.mtype = 10;
                    msg.akcja = MSG_ATAK;
                    msg.id_nadawcy = moje_id;
                    msg.dane[0] = l; msg.dane[1] = c; msg.dane[2] = j; msg.dane[3] = 0;
                    msgsnd(msg_id, &msg, sizeof(msg)-sizeof(long), 0);
                    printf(C_YELLOW " -> Wojska wyslane!" C_RESET);
                }
                
                sleep(1); 
                rysuj_menu_glowne();
                kill(pid_dziecka, SIGCONT);
            }
        }
    }
    return 0;
}