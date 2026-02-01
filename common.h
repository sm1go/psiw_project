#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>

// Stałe IPC
#define KLUCZ_MSG 12345
#define KLUCZ_SHM 12346
#define KLUCZ_SEM 12347

// Typy komunikatów
#define MSG_LOGIN 1
#define MSG_AKCJA 2
#define MSG_UPDATE 3

// Typy jednostek
#define PIECHOTA_LEKKA 0
#define PIECHOTA_CIEZKA 1
#define JAZDA 2
#define ROBOTNICY 3

// Statystyki jednostek (koszt, czas)
// Zgodnie z tabelą w PDF, ale zdefiniowane jako stałe w kodzie dla wygody
static const int KOSZT[] = {100, 250, 550, 150};
static const int CZAS_PROD[] = {2, 3, 5, 2};
// Atak i obrona jako mnożniki x10 dla operacji na int (unikamy float w kernelu/IPC dla prostoty, lub rzutujemy)
// PDF podaje np. 1.2. Użyjmy double w logice, ale przesyłamy inty w strukturach gdzie się da.
static const double ATAK[] = {1.0, 1.5, 3.5, 0.0};
static const double OBRONA[] = {1.2, 3.0, 1.2, 0.0};

// Struktura gracza w pamięci współdzielonej
struct GraczInfo {
    int id; // PID
    int aktywny;
    int surowce;
    int jednostki[4]; // Ilość posiadanych jednostek
    int w_produkcji[4]; // Ilość zlecana
    int czas_produkcji[4]; // Czas do końca produkcji
    int punkty;
};

// Pamięć współdzielona
struct StanGry {
    struct GraczInfo gracze[2];
    int licznik_graczy;
    char ostatni_komunikat[256]; // Wiadomość tekstowa o wyniku walki
};

// Struktura kolejki komunikatów
struct msgbuf {
    long mtype;
    int id_nadawcy; // PID
    int typ_akcji; // 1=Buduj, 2=Atak
    int parametry[4]; // [0]=typ, [1]=ilosc
};

// Struktura aktualizacji dla klienta
struct msgupdate {
    long mtype;
    struct GraczInfo ja;
    struct GraczInfo wrog;
    char wiadomosc[256];
};

#endif