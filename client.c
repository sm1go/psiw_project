#include "common.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        my_print("Uzycie: ./client <id_gracza 1 lub 2>\n");
        return 1;
    }

    int my_id = my_atoi(argv[1]);
    int msg_id = msgget(KEY_MSG, 0666);
    if (msg_id == -1) {
        my_print("Blad polaczenia z kolejka (uruchom serwer).\n");
        return 1;
    }

    my_print("Witaj w grze! Komendy:\n");
    my_print("  l <ilosc> - trenuj Lekka\n");
    my_print("  c <ilosc> - trenuj Ciezka\n");
    my_print("  r <ilosc> - trenuj Robotnika\n");
    my_print("  a <typ 0-2> <ilosc> - Atakuj\n");

    if (fork() == 0) {
        // --- PROCES ODBIERANIA STANU GRY (WYŚWIETLANIE) ---
        msg_buf msg;
        GameState gs;
        while(1) {
            // Odbieramy wiadomości typu 10 + my_id
            if (msgrcv(msg_id, &msg, sizeof(msg) - sizeof(long), 10 + my_id, 0) != -1) {
                // Odkodowanie stanu gry z pola text
                memcpy(&gs, msg.text, sizeof(GameState));

                // Ekran "czyszczony" nowymi liniami
                write(1, "\n\n\n\n\n\n\n\n\n\n", 10); 
                write(1, "=== STAN GRY ===\n", 17);
                
                write(1, "Zloto: ", 7); 
                my_print_int(gs.resources[my_id]);
                write(1, "\n", 1);

                write(1, "Jednostki [L C J R]: ", 21);
                my_print_int(gs.units[my_id][0]); write(1, " ", 1);
                my_print_int(gs.units[my_id][1]); write(1, " ", 1);
                my_print_int(gs.units[my_id][2]); write(1, " ", 1);
                my_print_int(gs.units[my_id][3]); write(1, "\n", 1);

                write(1, "Produkcja [L C J R]: ", 21);
                my_print_int(gs.production_queue[my_id][0]); write(1, " ", 1);
                my_print_int(gs.production_queue[my_id][1]); write(1, " ", 1);
                my_print_int(gs.production_queue[my_id][2]); write(1, " ", 1);
                my_print_int(gs.production_queue[my_id][3]); write(1, "\n", 1);

                write(1, "WYNIK (TY vs WROG): ", 20);
                my_print_int(gs.score[my_id]);
                write(1, " : ", 3);
                my_print_int(gs.score[ (my_id==1 ? 2 : 1) ]);
                write(1, "\n> ", 3); // Znak zachęty
            }
        }
    } else {
        // --- PROCES WYSYŁANIA KOMEND (KLAWIATURA) ---
        char input[50];
        msg_buf cmd_msg;
        cmd_msg.mtype = MSG_CMD;
        cmd_msg.client_id = my_id;

        while(1) {
            int n = read(0, input, 49);
            if (n > 0) {
                input[n] = '\0';
                
                // Bardzo proste parsowanie pierwszej litery
                char c = input[0];
                int val1 = 0;
                int val2 = 0;
                
                // Pomijamy pierwsza litere i spacje, czytamy liczbe
                // W prawdziwym C bez scanf trzeba to zrobić pętlą, tutaj uproszczenie:
                // Zakładamy format "x 10" - trzeci znak to początek liczby
                if (n > 2) {
                    val1 = my_atoi(&input[2]);
                }

                if (c == 'l') {
                    strcpy(cmd_msg.cmd, "TRAIN");
                    cmd_msg.unit_type = UNIT_L;
                    cmd_msg.count = val1;
                    msgsnd(msg_id, &cmd_msg, sizeof(cmd_msg) - sizeof(long), 0);
                }
                else if (c == 'c') {
                    strcpy(cmd_msg.cmd, "TRAIN");
                    cmd_msg.unit_type = UNIT_H;
                    cmd_msg.count = val1;
                    msgsnd(msg_id, &cmd_msg, sizeof(cmd_msg) - sizeof(long), 0);
                }
                else if (c == 'r') {
                    strcpy(cmd_msg.cmd, "TRAIN");
                    cmd_msg.unit_type = UNIT_R;
                    cmd_msg.count = val1;
                    msgsnd(msg_id, &cmd_msg, sizeof(cmd_msg) - sizeof(long), 0);
                }
                else if (c == 'a') {
                    // format a <typ> <ilosc>, np a 0 10
                    // musimy znaleźć drugą spację
                    int second_space = 2;
                    while(input[second_space] != ' ' && input[second_space] != '\0') second_space++;
                    
                    val1 = my_atoi(&input[2]); // typ
                    if (input[second_space] == ' ') {
                        val2 = my_atoi(&input[second_space+1]); // ilosc
                    }

                    strcpy(cmd_msg.cmd, "ATTACK");
                    cmd_msg.unit_type = val1;
                    cmd_msg.count = val2;
                    msgsnd(msg_id, &cmd_msg, sizeof(cmd_msg) - sizeof(long), 0);
                }
            }
        }
    }

    return 0;
}