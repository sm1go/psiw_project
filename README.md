INSTUKCJA KOMPILACJI:
1. Otwórz terminal w folderze z projektem.
2. Wpisz polecenie: make
   To utworzy pliki binarne 'server' i 'client'.

INSTRUKCJA URUCHOMIENIA:
1. Najpierw uruchom serwer: ./server
2. Otwórz dwa nowe okna terminala i w każdym uruchom klienta: ./client
3. Gra rozpocznie się automatycznie, gdy zaloguje się dwóch graczy.

OPIS PLIKÓW:
- server.c: Główna logika gry, obsługa bitew, przyrostu złota oraz zarządzanie zasobami IPC.
- client.c: Interfejs użytkownika, obsługa wejścia/wyjścia oraz wizualizacja stanu gry.
- common.h: Wspólne definicje struktur (Msg, GameData), stałych oraz funkcji pomocniczych (semaphores, I/O).