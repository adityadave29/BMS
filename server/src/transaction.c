#include "server.h"

// Record a new transaction in transactions.dat
void record_transaction(int from, int to, double amount, double before, double after)
{
    int tid = get_next_transaction_id();

    pthread_mutex_lock(&transactions_mutex);
    int fd = open(TRANSACTIONS_DB, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
    {
        pthread_mutex_unlock(&transactions_mutex);
        return;
    }

    Transaction t;
    memset(&t, 0, sizeof(Transaction));
    t.transaction_id = tid;
    t.from_acc = from;
    t.to_acc = to;
    t.amount = amount;
    t.before_bal = before;
    t.after_bal = after;

    time_t now = time(NULL);
    struct tm tm_info = *localtime(&now);
    strftime(t.timestamp, sizeof(t.timestamp), "%Y-%m-%d %H:%M:%S", &tm_info);

    off_t offset = lseek(fd, 0, SEEK_END);
    lock_record(fd, offset, sizeof(Transaction), F_WRLCK);
    write(fd, &t, sizeof(Transaction));
    lock_record(fd, offset, sizeof(Transaction), F_UNLCK);

    close(fd);
    pthread_mutex_unlock(&transactions_mutex);
}

// Deposit money into account using in-place record locking
void deposit_money(int client_sock, int acc_no)
{
    char msg[BUF_SIZE];

    snprintf(msg, BUF_SIZE, "Enter amount to deposit: ");
    send(client_sock, msg, strlen(msg), 0);

    memset(msg, 0, BUF_SIZE);
    int n = recv(client_sock, msg, BUF_SIZE - 1, 0);
    if (n <= 0)
        return;

    msg[strcspn(msg, "\r\n")] = 0;
    double deposit_amount = atof(msg);
    if (deposit_amount <= 0.0)
    {
        snprintf(msg, BUF_SIZE, "Invalid deposit amount!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    pthread_mutex_lock(&balance_mutex);
    int fd = open(BALANCE_DB, O_RDWR);
    if (fd < 0)
    {
        pthread_mutex_unlock(&balance_mutex);
        snprintf(msg, BUF_SIZE, "Error opening balance database!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    AccountBalance ab;
    off_t offset = 0;
    int found = 0;
    double before_bal = 0.0, after_bal = 0.0;

    while (1)
    {
        if (lock_record(fd, offset, sizeof(AccountBalance), F_WRLCK) < 0)
            break;

        ssize_t r = read(fd, &ab, sizeof(AccountBalance));
        if (r < (ssize_t)sizeof(AccountBalance))
        {
            lock_record(fd, offset, sizeof(AccountBalance), F_UNLCK);
            break;
        }

        if (ab.account_no == acc_no)
        {
            before_bal = ab.balance;
            ab.balance += deposit_amount;
            after_bal = ab.balance;

            lseek(fd, offset, SEEK_SET);
            write(fd, &ab, sizeof(AccountBalance));
            lock_record(fd, offset, sizeof(AccountBalance), F_UNLCK);
            found = 1;
            break;
        }

        lock_record(fd, offset, sizeof(AccountBalance), F_UNLCK);
        offset += sizeof(AccountBalance);
    }

    close(fd);
    pthread_mutex_unlock(&balance_mutex);

    if (!found)
    {
        snprintf(msg, BUF_SIZE, "Account number not found!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    record_transaction(acc_no, acc_no, deposit_amount, before_bal, after_bal);

    snprintf(msg, BUF_SIZE, "Deposit successful! New balance: %.2f\n", after_bal);
    send(client_sock, msg, strlen(msg), 0);
}

// Withdraw money from account using in-place record locking
void withdraw_money(int client_sock, int acc_no)
{
    char send_msg[BUF_SIZE], recv_msg[BUF_SIZE];

    snprintf(send_msg, BUF_SIZE, "Enter amount to withdraw: ");
    send(client_sock, send_msg, strlen(send_msg), 0);

    memset(recv_msg, 0, BUF_SIZE);
    int n = recv(client_sock, recv_msg, BUF_SIZE - 1, 0);
    if (n <= 0)
        return;

    recv_msg[strcspn(recv_msg, "\r\n")] = 0;
    double withdraw_amount = atof(recv_msg);
    if (withdraw_amount <= 0.0)
    {
        snprintf(send_msg, BUF_SIZE, "Invalid withdrawal amount!\n");
        send(client_sock, send_msg, strlen(send_msg), 0);
        return;
    }

    pthread_mutex_lock(&balance_mutex);
    int fd = open(BALANCE_DB, O_RDWR);
    if (fd < 0)
    {
        pthread_mutex_unlock(&balance_mutex);
        snprintf(send_msg, BUF_SIZE, "Error opening balance database!\n");
        send(client_sock, send_msg, strlen(send_msg), 0);
        return;
    }

    AccountBalance ab;
    off_t offset = 0;
    int found = 0;
    int insufficient = 0;
    double before_bal = 0.0, after_bal = 0.0;

    while (1)
    {
        if (lock_record(fd, offset, sizeof(AccountBalance), F_WRLCK) < 0)
            break;

        ssize_t r = read(fd, &ab, sizeof(AccountBalance));
        if (r < (ssize_t)sizeof(AccountBalance))
        {
            lock_record(fd, offset, sizeof(AccountBalance), F_UNLCK);
            break;
        }

        if (ab.account_no == acc_no)
        {
            found = 1;
            before_bal = ab.balance;

            if (ab.balance < withdraw_amount)
            {
                insufficient = 1;
                lock_record(fd, offset, sizeof(AccountBalance), F_UNLCK);
                break;
            }

            ab.balance -= withdraw_amount;
            after_bal = ab.balance;

            lseek(fd, offset, SEEK_SET);
            write(fd, &ab, sizeof(AccountBalance));
            lock_record(fd, offset, sizeof(AccountBalance), F_UNLCK);
            break;
        }

        lock_record(fd, offset, sizeof(AccountBalance), F_UNLCK);
        offset += sizeof(AccountBalance);
    }

    close(fd);
    pthread_mutex_unlock(&balance_mutex);

    if (!found)
    {
        snprintf(send_msg, BUF_SIZE, "Account number not found!\n");
        send(client_sock, send_msg, strlen(send_msg), 0);
        return;
    }

    if (insufficient)
    {
        snprintf(send_msg, BUF_SIZE, "Insufficient balance!\n");
        send(client_sock, send_msg, strlen(send_msg), 0);
        return;
    }

    record_transaction(acc_no, acc_no, withdraw_amount, before_bal, after_bal);

    snprintf(send_msg, BUF_SIZE, "Withdrawal successful! New balance: %.2f\n", after_bal);
    send(client_sock, send_msg, strlen(send_msg), 0);
}

// Transfer funds between two accounts using record-level fcntl locking
void transfer_funds(int client_sock, int acc_no)
{
    char msg[BUF_SIZE], recv_buf[BUF_SIZE];

    snprintf(msg, BUF_SIZE, "Enter recipient account number: ");
    send(client_sock, msg, strlen(msg), 0);

    memset(recv_buf, 0, sizeof(recv_buf));
    int n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[strcspn(recv_buf, "\r\n")] = 0;
    int recipient_acc = atoi(recv_buf);

    if (recipient_acc == acc_no)
    {
        snprintf(msg, BUF_SIZE, "Cannot transfer funds to the same account!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    snprintf(msg, BUF_SIZE, "Enter amount to transfer: ");
    send(client_sock, msg, strlen(msg), 0);

    memset(recv_buf, 0, sizeof(recv_buf));
    n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[strcspn(recv_buf, "\r\n")] = 0;
    double transfer_amount = atof(recv_buf);
    if (transfer_amount <= 0.0)
    {
        snprintf(msg, BUF_SIZE, "Invalid transfer amount!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    pthread_mutex_lock(&balance_mutex);
    int fd = open(BALANCE_DB, O_RDWR);
    if (fd < 0)
    {
        pthread_mutex_unlock(&balance_mutex);
        snprintf(msg, BUF_SIZE, "Error opening balance database!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    // Find offsets for sender and recipient
    AccountBalance ab;
    off_t offset = 0;
    off_t sender_offset = -1, recipient_offset = -1;
    AccountBalance sender_ab, recipient_ab;

    while (1)
    {
        lock_record(fd, offset, sizeof(AccountBalance), F_RDLCK);
        ssize_t r = read(fd, &ab, sizeof(AccountBalance));
        lock_record(fd, offset, sizeof(AccountBalance), F_UNLCK);

        if (r < (ssize_t)sizeof(AccountBalance))
            break;

        if (ab.account_no == acc_no)
        {
            sender_offset = offset;
            sender_ab = ab;
        }
        else if (ab.account_no == recipient_acc)
        {
            recipient_offset = offset;
            recipient_ab = ab;
        }

        offset += sizeof(AccountBalance);
    }

    if (sender_offset < 0)
    {
        close(fd);
        pthread_mutex_unlock(&balance_mutex);
        snprintf(msg, BUF_SIZE, "Sender account not found!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    if (recipient_offset < 0)
    {
        close(fd);
        pthread_mutex_unlock(&balance_mutex);
        snprintf(msg, BUF_SIZE, "Recipient account not found!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    // Lock both records (in order of offset to prevent deadlock)
    off_t first_offset = (sender_offset < recipient_offset) ? sender_offset : recipient_offset;
    off_t second_offset = (sender_offset < recipient_offset) ? recipient_offset : sender_offset;

    lock_record(fd, first_offset, sizeof(AccountBalance), F_WRLCK);
    lock_record(fd, second_offset, sizeof(AccountBalance), F_WRLCK);

    // Re-read sender balance
    lseek(fd, sender_offset, SEEK_SET);
    read(fd, &sender_ab, sizeof(AccountBalance));

    if (sender_ab.balance < transfer_amount)
    {
        lock_record(fd, first_offset, sizeof(AccountBalance), F_UNLCK);
        lock_record(fd, second_offset, sizeof(AccountBalance), F_UNLCK);
        close(fd);
        pthread_mutex_unlock(&balance_mutex);

        snprintf(msg, BUF_SIZE, "Insufficient balance!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    // Re-read recipient balance
    lseek(fd, recipient_offset, SEEK_SET);
    read(fd, &recipient_ab, sizeof(AccountBalance));

    double sender_before = sender_ab.balance;
    sender_ab.balance -= transfer_amount;
    recipient_ab.balance += transfer_amount;

    // Write updated balances in-place
    lseek(fd, sender_offset, SEEK_SET);
    write(fd, &sender_ab, sizeof(AccountBalance));

    lseek(fd, recipient_offset, SEEK_SET);
    write(fd, &recipient_ab, sizeof(AccountBalance));

    // Release locks
    lock_record(fd, first_offset, sizeof(AccountBalance), F_UNLCK);
    lock_record(fd, second_offset, sizeof(AccountBalance), F_UNLCK);
    close(fd);
    pthread_mutex_unlock(&balance_mutex);

    // Record the transaction
    record_transaction(acc_no, recipient_acc, transfer_amount, sender_before, sender_ab.balance);

    snprintf(msg, BUF_SIZE, "Transfer successful! New balance: %.2f\n", sender_ab.balance);
    send(client_sock, msg, strlen(msg), 0);
}

// View transaction history of an account
void view_transactions(int client_sock, int acc_no)
{
    pthread_mutex_lock(&transactions_mutex);
    int fd = open(TRANSACTIONS_DB, O_RDONLY);
    if (fd < 0)
    {
        pthread_mutex_unlock(&transactions_mutex);
        char msg[] = "No transaction history found!\n";
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    Transaction t;
    off_t offset = 0;
    int found = 0;

    while (1)
    {
        lock_record(fd, offset, sizeof(Transaction), F_RDLCK);
        ssize_t r = read(fd, &t, sizeof(Transaction));
        lock_record(fd, offset, sizeof(Transaction), F_UNLCK);

        if (r < (ssize_t)sizeof(Transaction))
            break;

        if (t.from_acc == acc_no || t.to_acc == acc_no)
        {
            found = 1;
            char send_buf[BUF_SIZE];
            snprintf(send_buf, BUF_SIZE, "ID:%d FROM:%d TO:%d AMOUNT:%.2f BEFORE:%.2f AFTER:%.2f TIME:%s\n",
                     t.transaction_id, t.from_acc, t.to_acc, t.amount, t.before_bal, t.after_bal, t.timestamp);
            send(client_sock, send_buf, strlen(send_buf), 0);
        }

        offset += sizeof(Transaction);
    }

    close(fd);
    pthread_mutex_unlock(&transactions_mutex);

    if (!found)
    {
        char msg[] = "No transactions found for your account!\n";
        send(client_sock, msg, strlen(msg), 0);
    }
}
