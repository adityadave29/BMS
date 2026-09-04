#include "server.h"

// Thread worker routine for each connected client
void *handle_client_thread(void *arg)
{
    int client_sock = *(int *)arg;
    free(arg);
    handle_client(client_sock);
    pthread_exit(NULL);
}

// This function will take care of login, exit, and disconnect commands
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
        else if (strncmp(buffer, "disconnect", 10) == 0)
        {
            char disc_email[100] = "";
            sscanf(buffer + 11, "%99s", disc_email);
            if (strlen(disc_email) > 0)
                remove_active_user(disc_email);
            break;
        }
        else if (strncmp(buffer, "login", 5) == 0)
        {
            memset(email, 0, sizeof(email));
            memset(password, 0, sizeof(password));
            memset(role, 0, sizeof(role));
            sscanf(buffer + 6, "%99s %99s %49s", email, password, role);
            login_user(client_sock, email, password, role);
        }
        else
        {
            send(client_sock, "Invalid command. Please use login or exit.\n", 43, 0);
        }
    }

    close(client_sock);
}
