#include "server.h"

// This function will take care of login and exit
void handle_client(int client_sock)
{
    char buffer[BUF_SIZE];
    char email[100], password[100], role[50];

    while (1)
    {
        memset(buffer, 0, BUF_SIZE);
        int n = recv(client_sock, buffer, BUF_SIZE, 0);
        if (n <= 0)
            break;

        if (strncmp(buffer, "exit", 4) == 0)
        {
            exit_session(client_sock);
            break;
        }
        else if (strncmp(buffer, "login", 5) == 0)
        {
            sscanf(buffer + 6, "%s %s %s", email, password, role);
            login_user(client_sock, email, password, role);
        }
    }

    close(client_sock);
}
