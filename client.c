#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

/* --- DEFINICJE ZGODNE Z TWOIM WZOREM --- */
#define MSG_LOGOWANIE 10
#define MSG_BUDUJ     11
#define MSG_ATAK      12
#define MSG_STAN      13
#define MSG_WYNIK     14
#define MSG_START     15

#define KLUCZ 12345

// Struktura identyczna jak w Twoim wzorze
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

pid_t pid_dziecka = 0;

/* --- FUNKCJE POMOCNICZE (zamiast printf/scanf) --- */

// Wypisywanie ciągu znaków
void print_str(const char *s) {
    write(1, s, strlen(s));
}

// Wypisywanie liczby całkowitej
void print_int(int n) {
    char buf[20];
    int i = 0;
    if (n == 0) {
        write(1, "0", 1);
        return;
    }
    int neg = 0;
    if (n < 0) { neg = 1; n = -n; }
    while (n > 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }
    if (neg) buf[i++] = '-';
    for (int j = 0; j < i / 2; j++) {
        char tmp = buf[j];
        buf[j] = buf[i - j - 1];
        buf[i - j - 1] = tmp;
    }
    write(1, buf, i);
}

// Pobieranie liczby od użytkownika (zamiast scanf)
int get_int() {
    char buf[32];
    int n = read(0, buf, 31);
    if (n <= 0) return 0;
    buf[n] = '\0';
    int res = 0;
    int start = 0;
    // Pomin ewentualne białe znaki
    while(buf[start] == ' ' || buf[start] == '\n') start++;
    
    for (int i = start; i < n; ++i) {
        if (buf[i] >= '0' && buf[i] <= '9') {
            res = res * 10 + (buf[i] - '0');
        } else if (buf[i] == '\n' || buf[i] == ' ') {
            break;
        }
    }
    return res;
}

// Pobieranie trzech liczb (dla ataku: L C J)
void get_three_ints(int *a, int *b, int *c) {
    char buf[64];
    int n = read(0, buf, 63);
    if (n <= 0) { *a=0; *b=0; *c=0; return; }
    buf[n] = '\0';
    
    int val = 0;
    int counter = 0;
    int reading_num = 0;
    
    for(int i=0; i<n; i++) {
        if(buf[i] >= '0' && buf[i] <= '9') {
            val = val * 10 + (buf[i] - '0');
            reading_num = 1;
        } else {
            if(reading_num) {
                if(counter == 0) *a = val;
                else if(counter == 1) *b = val;
                else if(counter == 2) *c = val;
                val = 0;
                counter++;
                reading_num = 0;
            }
        }
    }
    // Jeśli ciąg kończy się liczbą bez spacji
    if(reading_num) {
        if(counter == 0) *a = val;
        else if(counter == 1) *b = val;
        else if(counter == 2) *c = val;
    }
}

// Czyszczenie ekranu kodami ANSI (zamiast system("clear"))
void clear_screen() {
    write(1, "\033[H\033[J", 7);
}

/* --- OBSŁUGA SYGNAŁÓW --- */

void czyszczenie(int sig) {
    if (pid_dziecka > 0) kill(pid_dziecka, SIGKILL);
    exit(0);
}

/* --- MAIN --- */

int main() {
    signal(SIGINT, czyszczenie);
    
    int msg = msgget(KLUCZ, 0600);
    if (msg == -1) { 
        print_str("Brak polaczenia z serwerem (uruchom serwer).\n"); 
        return 1; 
    }

    pid_t pid = getpid();
    struct msgbuf komunikat;
    
    // --- LOGOWANIE ---
    komunikat.liczba = pid;
    komunikat.wybor = MSG_LOGOWANIE;
    komunikat.mtype = MSG_LOGOWANIE;
    
    msgsnd(msg, &komunikat, sizeof(komunikat) - sizeof(long), 0);
    msgrcv(msg, &komunikat, sizeof(komunikat) - sizeof(long), pid, 0);
    
    int id = komunikat.id_gracza;
    if (id == -1) {
        print_str("Serwer pelny! Zamykanie.\n");
        return 0;
    }
    
    print_str("Zalogowano! Twoje ID to: ");
    print_int(id);
    print_str("\n");

    // --- OCZEKIWANIE NA GRACZA ---
    print_str("Oczekiwanie na drugiego gracza...\n");
    while(1) {
        komunikat.mtype = MSG_START;
        komunikat.wybor = MSG_START;
        komunikat.id_gracza = id;
        
        msgsnd(msg, &komunikat, sizeof(komunikat) - sizeof(long), 0);
        // Odbiór
        msgrcv(msg, &komunikat, sizeof(komunikat) - sizeof(long), id, 0);
        
        if (komunikat.liczba == 1) {
            print_str("Drugi gracz dolaczyl! START GRY!\n");
            sleep(1);
            break;
        }
        sleep(1);
    }

    // --- ROZDZIELENIE PROCESÓW ---
    if ((pid_dziecka = fork()) == 0) {
        // === PROCES DZIECKO (WYŚWIETLANIE) ===
        struct msgbuf zapytanie, odpowiedz;
        
        while(1) {
            // Pytanie o stan
            zapytanie.mtype = MSG_STAN;
            zapytanie.wybor = MSG_STAN;
            zapytanie.id_gracza = id;
            msgsnd(msg, &zapytanie, sizeof(zapytanie) - sizeof(long), 0);
            
            // Odbiór stanu
            if (msgrcv(msg, &odpowiedz, sizeof(odpowiedz) - sizeof(long), id, 0) != -1) {
                clear_screen();
                print_str("=== GRACZ "); print_int(id); print_str(" ===\n");
                
                print_str("PUNKTY ZWYCIESTWA: "); 
                print_int(odpowiedz.punkty); 
                print_str(" / 5\n");
                
                print_str("Zasoby (Zloto): ");
                print_int(odpowiedz.zasoby);
                print_str("\n----------------------------\n");
                print_str("ARMIA:\n");
                print_str(" [0] Lekka Piechota:  "); print_int(odpowiedz.jednostki[0]); print_str("\n");
                print_str(" [1] Ciezka Piechota: "); print_int(odpowiedz.jednostki[1]); print_str("\n");
                print_str(" [2] Jazda:           "); print_int(odpowiedz.jednostki[2]); print_str("\n");
                print_str(" [3] Robotnicy:       "); print_int(odpowiedz.jednostki[3]); print_str("\n");
                print_str("----------------------------\n");
                print_str("AKCJE: 1. Buduj   2. Atak\n");
                print_str("Twoj wybor: ");
            }

            // Sprawdzanie wyniku (czy ktoś wygrał)
            zapytanie.mtype = MSG_WYNIK;
            zapytanie.wybor = MSG_WYNIK;
            zapytanie.id_gracza = id;
            msgsnd(msg, &zapytanie, sizeof(zapytanie) - sizeof(long), 0);
            msgrcv(msg, &odpowiedz, sizeof(odpowiedz) - sizeof(long), id, 0);

            if (odpowiedz.liczba == 1) {
                print_str("\n\n!!! GRATULACJE! WYGRALES !!!\n");
                kill(getppid(), SIGKILL);
                exit(0);
            } 
            else if (odpowiedz.liczba == 2) {
                print_str("\n\n!!! PRZEGRALES !!!\n");
                kill(getppid(), SIGKILL);
                exit(0);
            }

            sleep(1);
        }
    } else {
        // === PROCES RODZIC (KLAWIATURA) ===
        struct msgbuf rozkaz;
        
        while(1) {
            // Czekamy na input (blokująco)
            int opcja = get_int();
            
            if (opcja == 1) {
                // ZATRZYMUJEMY ODŚWIEŻANIE (DZIECKO)
                kill(pid_dziecka, SIGSTOP);
                
                clear_screen();
                print_str("=== BUDOWANIE ===\n");
                print_str("Typ (0-Lekka, 1-Ciezka, 2-Jazda, 3-Rob): ");
                int typ = get_int();
                
                print_str("Ilosc: ");
                int ilosc = get_int();
                
                rozkaz.mtype = MSG_BUDUJ;
                rozkaz.wybor = MSG_BUDUJ;
                rozkaz.id_gracza = id;
                rozkaz.indeks_jednostki = typ;
                rozkaz.liczba = ilosc;
                
                msgsnd(msg, &rozkaz, sizeof(rozkaz) - sizeof(long), 0);
                
                // Czekamy na odpowiedź czy starczyło złota
                msgrcv(msg, &rozkaz, sizeof(rozkaz) - sizeof(long), id, 0);
                
                if (rozkaz.liczba == 0) {
                    print_str("\n[!!!] BLAD: ZA MALO SRODKOW! [!!!]\n");
                    sleep(2);
                } else {
                    print_str("\nRozpoczeto budowe.\n");
                    sleep(1);
                }
                
                // WZNAWIAMY ODŚWIEŻANIE
                kill(pid_dziecka, SIGCONT);
            }
            else if (opcja == 2) {
                // ZATRZYMUJEMY ODŚWIEŻANIE
                kill(pid_dziecka, SIGSTOP);
                
                clear_screen();
                print_str("=== ATAK ===\n");
                print_str("Podaj ilosc wojsk (Lekka Ciezka Jazda) oddzielone spacja:\n");
                
                int l, c, j;
                get_three_ints(&l, &c, &j);
                
                rozkaz.mtype = MSG_ATAK;
                rozkaz.wybor = MSG_ATAK;
                rozkaz.id_gracza = id;
                rozkaz.jednostki[0] = l;
                rozkaz.jednostki[1] = c;
                rozkaz.jednostki[2] = j;
                rozkaz.jednostki[3] = 0; // Robotnicy nie walczą
                
                msgsnd(msg, &rozkaz, sizeof(rozkaz) - sizeof(long), 0);
                
                // Czekamy na potwierdzenie ataku
                msgrcv(msg, &rozkaz, sizeof(rozkaz) - sizeof(long), id, 0);
                
                if (rozkaz.liczba == 0) {
                    print_str("\n[!!!] BLAD: NIE MASZ TYLE WOJSKA! [!!!]\n");
                    sleep(2);
                } else {
                    print_str("\nATAK WYSLANY!\n");
                    sleep(1);
                }
                
                // WZNAWIAMY ODŚWIEŻANIE
                kill(pid_dziecka, SIGCONT);
            }
        }
    }
    return 0;
}