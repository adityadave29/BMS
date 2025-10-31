#include "server.h"

/*
make
make run
make clean
*/

// Here we are just making connection with client and forking process for each client (We can use threads also they are light weight but thread management is not easy in macos so for simplicity we are using fork.)
int main()
{
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0)
    {
        perror("Socket error");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        exit(1);
    }

    if (listen(server_sock, 5) < 0)
    {
        perror("Listen failed");
        exit(1);
    }

    printf("Server running on port %d...\n", PORT);

    while (1)
    {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
        if (client_sock < 0)
        {
            perror("Accept failed");
            continue;
        }

        printf("Client connected.\n");

        if (fork() == 0)
        { // child process
            close(server_sock);
            handle_client(client_sock);
            exit(0);
        }
        else
        {
            close(client_sock);
        }
    }

    close(server_sock);
    return 0;
}
