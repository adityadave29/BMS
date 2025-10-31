#include "server.h"

// add transaction
void log_transaction(int from_acc, int to_acc, int amount, int before_balance, int after_balance, int transaction_id)
{
    FILE *fp = fopen(TRANSACTIONS, "a");
    if (!fp)
        return;

    time_t t = time(NULL);
    struct tm tm_info = *localtime(&t);

    char timestamp[50];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_info);

    fprintf(fp, "%d %d %d %d %d %d %s\n",
            transaction_id, from_acc, to_acc, amount, before_balance, after_balance, timestamp);

    fclose(fp);
}

// deposite money
void deposit_money(int client_sock, int acc_no)
{
    char msg[BUF_SIZE];

    snprintf(msg, BUF_SIZE, "Enter amount to deposit: ");
    send(client_sock, msg, strlen(msg), 0);

    memset(msg, 0, BUF_SIZE);
    int n = recv(client_sock, msg, BUF_SIZE, 0);
    if (n <= 0)
        return;

    msg[strcspn(msg, "\n")] = 0;
    int deposit_amount = atoi(msg);
    if (deposit_amount <= 0)
    {
        snprintf(msg, BUF_SIZE, "Invalid deposit amount!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    // Read balances
    FILE *fp = fopen(BALANCE, "r");
    if (!fp)
    {
        snprintf(msg, BUF_SIZE, "Error opening balance file!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    char lines[1000][BUF_SIZE];
    int line_count = 0, found = 0;
    int file_acc_no, balance, before_balance;

    while (fgets(msg, sizeof(msg), fp))
    {
        sscanf(msg, "%d %d", &file_acc_no, &balance);
        if (file_acc_no == acc_no)
        {
            before_balance = balance;
            balance += deposit_amount;
            found = 1;
        }
        snprintf(lines[line_count], BUF_SIZE, "%d %d\n", file_acc_no, balance);
        line_count++;
    }
    fclose(fp);

    if (!found)
    {
        snprintf(msg, BUF_SIZE, "Account number not found!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    int fd = open(ACCOUNT_FILE, O_RDWR);
    if (fd < 0)
    {
        snprintf(msg, BUF_SIZE, "Error opening account number file!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    flock(fd, LOCK_EX);

    FILE *fp_acc = fdopen(fd, "r+");
    int account_count, loan_id, feedback_id, transaction_id, employee_id;
    fscanf(fp_acc, "%d %d %d %d %d", &account_count, &loan_id, &feedback_id, &transaction_id, &employee_id);
    transaction_id++;
    rewind(fp_acc);
    fprintf(fp_acc, "%d\n%d\n%d\n%d\n%d\n", account_count, loan_id, feedback_id, transaction_id, employee_id);
    fflush(fp_acc);

    flock(fd, LOCK_UN);
    fclose(fp_acc);

    fp = fopen(BALANCE, "w");
    for (int i = 0; i < line_count; i++)
        fputs(lines[i], fp);
    fclose(fp);

    log_transaction(acc_no, acc_no, deposit_amount, before_balance, before_balance + deposit_amount, transaction_id);

    snprintf(msg, BUF_SIZE, "Deposit successful! Your balance has been updated.\n");
    send(client_sock, msg, strlen(msg), 0);
}

// withdraw money
void withdraw_money(int client_sock, int acc_no)
{
    char send_msg[BUF_SIZE], recv_msg[BUF_SIZE];

    snprintf(send_msg, BUF_SIZE, "Enter amount to withdraw: ");
    send(client_sock, send_msg, strlen(send_msg), 0);

    memset(recv_msg, 0, BUF_SIZE);
    int n = recv(client_sock, recv_msg, BUF_SIZE, 0);
    if (n <= 0)
        return;

    recv_msg[strcspn(recv_msg, "\n")] = 0;
    int withdraw_amount = atoi(recv_msg);
    if (withdraw_amount <= 0)
    {
        snprintf(send_msg, BUF_SIZE, "Invalid withdrawal amount!\n");
        send(client_sock, send_msg, strlen(send_msg), 0);
        return;
    }

    FILE *fp = fopen(BALANCE, "r");
    if (!fp)
    {
        snprintf(send_msg, BUF_SIZE, "Error opening balance file!\n");
        send(client_sock, send_msg, strlen(send_msg), 0);
        return;
    }

    char lines[1000][BUF_SIZE];
    int line_count = 0, found = 0;
    int file_acc_no, balance, before_balance;

    while (fgets(recv_msg, sizeof(recv_msg), fp))
    {
        sscanf(recv_msg, "%d %d", &file_acc_no, &balance);
        if (file_acc_no == acc_no)
        {
            if (balance < withdraw_amount)
            {
                snprintf(send_msg, BUF_SIZE, "Insufficient balance!\n");
                send(client_sock, send_msg, strlen(send_msg), 0);
                fclose(fp);
                return;
            }
            before_balance = balance;
            balance -= withdraw_amount;
            found = 1;
        }
        snprintf(lines[line_count], BUF_SIZE, "%d %d\n", file_acc_no, balance);
        line_count++;
    }
    fclose(fp);

    if (!found)
    {
        snprintf(send_msg, BUF_SIZE, "Account number not found!\n");
        send(client_sock, send_msg, strlen(send_msg), 0);
        return;
    }

    int fd = open(ACCOUNT_FILE, O_RDWR);
    if (fd < 0)
        return;

    flock(fd, LOCK_EX);

    FILE *fp_acc = fdopen(fd, "r+");
    int account_count, loan_id, feedback_id, transaction_id, employee_id;
    fscanf(fp_acc, "%d %d %d %d %d", &account_count, &loan_id, &feedback_id, &transaction_id, &employee_id);
    transaction_id++;
    rewind(fp_acc);
    fprintf(fp_acc, "%d\n%d\n%d\n%d\n%d\n", account_count, loan_id, feedback_id, transaction_id, employee_id);
    fflush(fp_acc);

    flock(fd, LOCK_UN); // 🔓
    fclose(fp_acc);

    fp = fopen(BALANCE, "w");
    for (int i = 0; i < line_count; i++)
        fputs(lines[i], fp);
    fclose(fp);

    log_transaction(acc_no, acc_no, withdraw_amount, before_balance, before_balance - withdraw_amount, transaction_id);

    snprintf(send_msg, BUF_SIZE, "Withdrawal successful! Your balance has been updated.\n");
    send(client_sock, send_msg, strlen(send_msg), 0);
}

// transfer money from one account to another
void transfer_funds(int client_sock, int acc_no)
{
    char msg[BUF_SIZE], recv_buf[BUF_SIZE];

    snprintf(msg, BUF_SIZE, "Enter recipient account number: ");
    send(client_sock, msg, strlen(msg), 0);

    memset(recv_buf, 0, sizeof(recv_buf));
    int n = recv(client_sock, recv_buf, sizeof(recv_buf), 0);
    if (n <= 0)
        return;
    recv_buf[strcspn(recv_buf, "\n")] = 0;
    int recipient_acc = atoi(recv_buf);

    snprintf(msg, BUF_SIZE, "Enter amount to transfer: ");
    send(client_sock, msg, strlen(msg), 0);

    memset(recv_buf, 0, sizeof(recv_buf));
    n = recv(client_sock, recv_buf, sizeof(recv_buf), 0);
    if (n <= 0)
        return;
    recv_buf[strcspn(recv_buf, "\n")] = 0;
    int transfer_amount = atoi(recv_buf);
    if (transfer_amount <= 0)
    {
        snprintf(msg, BUF_SIZE, "Invalid transfer amount!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    FILE *fp = fopen(BALANCE, "r");
    if (!fp)
    {
        snprintf(msg, BUF_SIZE, "Error opening balance file!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    char lines[1000][BUF_SIZE];
    int line_count = 0;
    int sender_found = 0, recipient_found = 0;
    int sender_before = 0, recipient_before = 0;
    int file_acc_no, balance;

    while (fgets(recv_buf, sizeof(recv_buf), fp))
    {
        if (strlen(recv_buf) < 3)
            continue;

        if (sscanf(recv_buf, "%d %d", &file_acc_no, &balance) != 2)
            continue;

        if (file_acc_no == acc_no)
        {
            if (balance < transfer_amount)
            {
                snprintf(msg, BUF_SIZE, "Insufficient balance!\n");
                send(client_sock, msg, strlen(msg), 0);
                fclose(fp);
                return;
            }
            sender_before = balance;
            balance -= transfer_amount;
            sender_found = 1;
        }
        else if (file_acc_no == recipient_acc)
        {
            recipient_before = balance;
            balance += transfer_amount;
            recipient_found = 1;
        }

        snprintf(lines[line_count], BUF_SIZE, "%d %d\n", file_acc_no, balance);
        line_count++;
    }
    fclose(fp);

    if (!sender_found || !recipient_found)
    {
        snprintf(msg, BUF_SIZE, "Account not found!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    int fd = open(ACCOUNT_FILE, O_RDWR);
    if (fd < 0)
        return;

    flock(fd, LOCK_EX);

    FILE *fp_acc = fdopen(fd, "r+");
    int account_count, loan_id, feedback_id, transaction_id, employee_id;
    fscanf(fp_acc, "%d %d %d %d %d", &account_count, &loan_id, &feedback_id, &transaction_id, &employee_id);
    transaction_id++;
    rewind(fp_acc);
    fprintf(fp_acc, "%d\n%d\n%d\n%d\n%d\n", account_count, loan_id, feedback_id, transaction_id, employee_id);
    fflush(fp_acc);

    flock(fd, LOCK_UN);
    fclose(fp_acc);

    fp = fopen(BALANCE, "w");
    for (int i = 0; i < line_count; i++)
        fputs(lines[i], fp);
    fclose(fp);

    log_transaction(acc_no, recipient_acc, transfer_amount, sender_before, sender_before - transfer_amount, transaction_id);

    snprintf(msg, BUF_SIZE, "Transfer successful!\n");
    send(client_sock, msg, strlen(msg), 0);
}

// view transactions of an account -> logged_in user
void view_transactions(int client_sock, int acc_no)
{
    char line[BUF_SIZE];
    FILE *fp = fopen("d_transaction.txt", "r");
    if (!fp)
    {
        char msg[] = "No transaction history found!\n";
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    int found = 0;
    while (fgets(line, sizeof(line), fp))
    {
        int transaction_id, from, to, amount, before, after;
        char timestamp[50];

        if (sscanf(line, "%d %d %d %d %d %d %49[^\n]", &transaction_id, &from, &to, &amount, &before, &after, timestamp) != 7)
            continue;

        if (from == acc_no || to == acc_no)
        {
            found = 1;
            char send_buf[BUF_SIZE];
            snprintf(send_buf, BUF_SIZE, "ID:%d FROM:%d TO:%d AMOUNT:%d BEFORE:%d AFTER:%d TIME:%s\n",
                     transaction_id, from, to, amount, before, after, timestamp);
            send(client_sock, send_buf, strlen(send_buf), 0);
        }
    }

    fclose(fp);

    if (!found)
    {
        char msg[] = "No transactions found for your account!\n";
        send(client_sock, msg, strlen(msg), 0);
    }
}
