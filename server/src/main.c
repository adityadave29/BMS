#include "server.h"

/*
make
make run
make clean
*/

// Multithreaded TCP Server using POSIX threads (pthread)
int main()
{
    int server_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0)
    {
        perror("Socket error");
        exit(1);
    }

    // Set SO_REUSEADDR to avoid address in use errors on rapid restart
    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt failed");
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        exit(1);
    }

    if (listen(server_sock, 50) < 0)
    {
        perror("Listen failed");
        exit(1);
    }

    // Ignore SIGPIPE to prevent server process termination when a client abruptly disconnects
    signal(SIGPIPE, SIG_IGN);

    // Initialize binary database and reset active session flags
    init_database();

    printf("Server running on port %d (multithreaded with pthread & binary database)...\n", PORT);

    while (1)
    {
        // Directly allocate heap memory for client socket pointer
        int *client_sock = malloc(sizeof(int));
        if (!client_sock)
        {
            perror("malloc failed for client socket");
            continue;
        }

        *client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
        if (*client_sock < 0)
        {
            perror("Accept failed");
            free(client_sock);
            continue;
        }

        printf("Client connected [fd: %d].\n", *client_sock);

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client_thread, (void *)client_sock) != 0)
        {
            perror("pthread_create failed");
            close(*client_sock);
            free(client_sock);
            continue;
        }

        // Detach thread to automatically reclaim resources on termination
        pthread_detach(tid);
    }

    close(server_sock);
    return 0;
}
