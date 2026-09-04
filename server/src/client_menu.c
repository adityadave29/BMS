#include "server.h"

// Check current balance from balance database
void view_balance(int client_sock, int acc_no)
{
    double balance = get_balance(acc_no);
    char msg[BUF_SIZE];

    if (balance >= 0.0)
    {
        snprintf(msg, BUF_SIZE, "Your current balance is: %.2f\n", balance);
    }
    else
    {
        snprintf(msg, BUF_SIZE, "Account number %d not found!\n", acc_no);
    }

    send(client_sock, msg, strlen(msg), 0);
}

// Apply for a loan in loans.dat
void apply_loan(int client_sock, int acc_no)
{
    char msg[BUF_SIZE], recv_buf[BUF_SIZE];

    // ---------------- ASK FOR LOAN DETAILS ----------------
    snprintf(msg, BUF_SIZE, "Enter loan amount: ");
    send(client_sock, msg, strlen(msg), 0);
    memset(recv_buf, 0, sizeof(recv_buf));
    int n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[strcspn(recv_buf, "\r\n")] = 0;
    double loan_amount = atof(recv_buf);
    if (loan_amount <= 0.0)
    {
        snprintf(msg, BUF_SIZE, "Invalid loan amount!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    snprintf(msg, BUF_SIZE, "Enter loan type (personal/home/car): ");
    send(client_sock, msg, strlen(msg), 0);
    memset(recv_buf, 0, sizeof(recv_buf));
    n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[strcspn(recv_buf, "\r\n")] = 0;

    int new_loan_id = get_next_loan_id();

    // ---------------- WRITE LOAN ENTRY ----------------
    pthread_mutex_lock(&loans_mutex);
    int fd = open(LOANS_DB, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd >= 0)
    {
        Loan l;
        memset(&l, 0, sizeof(Loan));
        l.account_no = acc_no;
        l.loan_id = new_loan_id;
        l.amount = loan_amount;
        strncpy(l.loan_type, recv_buf, sizeof(l.loan_type) - 1);
        strncpy(l.status, "pending", sizeof(l.status) - 1);
        l.assigned_to = 0;
        strncpy(l.reason, "-", sizeof(l.reason) - 1);

        time_t t = time(NULL);
        struct tm tm_info = *localtime(&t);
        strftime(l.timestamp, sizeof(l.timestamp), "%Y-%m-%d %H:%M:%S", &tm_info);

        off_t offset = lseek(fd, 0, SEEK_END);
        lock_record(fd, offset, sizeof(Loan), F_WRLCK);
        write(fd, &l, sizeof(Loan));
        lock_record(fd, offset, sizeof(Loan), F_UNLCK);
        close(fd);
    }
    pthread_mutex_unlock(&loans_mutex);

    snprintf(msg, BUF_SIZE, "Loan application submitted successfully. Your Loan ID: %d\n", new_loan_id);
    send(client_sock, msg, strlen(msg), 0);
}

// Change customer password with in-place record locking
void change_password(int client_sock, int acc_no)
{
    char msg[256], recv_buf[256];
    char old_pass[PASS_LEN], new_pass[PASS_LEN], confirm_pass[PASS_LEN];
    char email[EMAIL_LEN];
    int found = 0;
    off_t cust_offset = 0;
    Customer c;

    // Step 1: Find account in customers.dat
    pthread_mutex_lock(&customers_mutex);
    int fd_cust = open(CUSTOMERS_DB, O_RDWR);
    if (fd_cust < 0)
    {
        pthread_mutex_unlock(&customers_mutex);
        snprintf(msg, sizeof(msg), "Error opening customer database!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    while (1)
    {
        lock_record(fd_cust, cust_offset, sizeof(Customer), F_RDLCK);
        ssize_t r = read(fd_cust, &c, sizeof(Customer));
        lock_record(fd_cust, cust_offset, sizeof(Customer), F_UNLCK);

        if (r < (ssize_t)sizeof(Customer))
            break;

        if (c.account_no == acc_no)
        {
            strncpy(email, c.email, sizeof(email) - 1);
            found = 1;
            break;
        }
        cust_offset += sizeof(Customer);
    }
    close(fd_cust);
    pthread_mutex_unlock(&customers_mutex);

    if (!found)
    {
        snprintf(msg, sizeof(msg), "Account not found!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    // Step 2: Verify old password
    snprintf(msg, sizeof(msg), "Enter old password: ");
    send(client_sock, msg, strlen(msg), 0);
    int n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[strcspn(recv_buf, "\r\n")] = 0;
    strncpy(old_pass, recv_buf, sizeof(old_pass) - 1);

    if (strcmp(old_pass, c.password) != 0)
    {
        snprintf(msg, sizeof(msg), "Old password incorrect!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    // Step 3: Ask new password
    snprintf(msg, sizeof(msg), "Enter new password: ");
    send(client_sock, msg, strlen(msg), 0);
    n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[strcspn(recv_buf, "\r\n")] = 0;
    strncpy(new_pass, recv_buf, sizeof(new_pass) - 1);

    snprintf(msg, sizeof(msg), "Confirm new password: ");
    send(client_sock, msg, strlen(msg), 0);
    n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[strcspn(recv_buf, "\r\n")] = 0;
    strncpy(confirm_pass, recv_buf, sizeof(confirm_pass) - 1);

    if (strcmp(new_pass, confirm_pass) != 0)
    {
        snprintf(msg, sizeof(msg), "Passwords do not match!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    // Step 4: Update Customer record in-place
    pthread_mutex_lock(&customers_mutex);
    fd_cust = open(CUSTOMERS_DB, O_RDWR);
    if (fd_cust >= 0)
    {
        lock_record(fd_cust, cust_offset, sizeof(Customer), F_WRLCK);
        strncpy(c.password, new_pass, sizeof(c.password) - 1);
        lseek(fd_cust, cust_offset, SEEK_SET);
        write(fd_cust, &c, sizeof(Customer));
        lock_record(fd_cust, cust_offset, sizeof(Customer), F_UNLCK);
        close(fd_cust);
    }
    pthread_mutex_unlock(&customers_mutex);

    // Step 5: Update User login record in-place
    pthread_mutex_lock(&users_mutex);
    int fd_user = open(USERS_DB, O_RDWR);
    if (fd_user >= 0)
    {
        User u;
        off_t u_offset = 0;
        while (1)
        {
            lock_record(fd_user, u_offset, sizeof(User), F_WRLCK);
            ssize_t r = read(fd_user, &u, sizeof(User));
            if (r < (ssize_t)sizeof(User))
            {
                lock_record(fd_user, u_offset, sizeof(User), F_UNLCK);
                break;
            }

            if (strcmp(u.email, email) == 0)
            {
                strncpy(u.password, new_pass, sizeof(u.password) - 1);
                lseek(fd_user, u_offset, SEEK_SET);
                write(fd_user, &u, sizeof(User));
                lock_record(fd_user, u_offset, sizeof(User), F_UNLCK);
                break;
            }

            lock_record(fd_user, u_offset, sizeof(User), F_UNLCK);
            u_offset += sizeof(User);
        }
        close(fd_user);
    }
    pthread_mutex_unlock(&users_mutex);

    snprintf(msg, sizeof(msg), "Password updated successfully!\n");
    send(client_sock, msg, strlen(msg), 0);
}

// Add feedback to feedbacks.dat
void add_feedback(int client_sock, int acc_no)
{
    char msg[BUF_SIZE], feedback[FEEDBACK_LEN];

    int new_feedback_id = get_next_feedback_id();

    // --- Ask client for feedback ---
    snprintf(msg, BUF_SIZE, "Enter your feedback: ");
    send(client_sock, msg, strlen(msg), 0);

    memset(feedback, 0, sizeof(feedback));
    int n = recv(client_sock, feedback, sizeof(feedback) - 1, 0);
    if (n <= 0)
        return;

    feedback[strcspn(feedback, "\r\n")] = 0;

    pthread_mutex_lock(&feedbacks_mutex);
    int fd = open(FEEDBACKS_DB, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
    {
        pthread_mutex_unlock(&feedbacks_mutex);
        snprintf(msg, BUF_SIZE, "Error opening feedback database!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    Feedback fb;
    memset(&fb, 0, sizeof(Feedback));
    fb.feedback_id = new_feedback_id;
    fb.account_no = acc_no;
    strncpy(fb.feedback, feedback, sizeof(fb.feedback) - 1);

    time_t t = time(NULL);
    struct tm tm_info = *localtime(&t);
    strftime(fb.timestamp, sizeof(fb.timestamp), "%Y-%m-%d %H:%M:%S", &tm_info);

    off_t offset = lseek(fd, 0, SEEK_END);
    lock_record(fd, offset, sizeof(Feedback), F_WRLCK);
    write(fd, &fb, sizeof(Feedback));
    lock_record(fd, offset, sizeof(Feedback), F_UNLCK);
    close(fd);
    pthread_mutex_unlock(&feedbacks_mutex);

    snprintf(msg, BUF_SIZE, "Feedback submitted successfully!\n");
    send(client_sock, msg, strlen(msg), 0);
}

void exit_application(int client_sock, const char *email)
{
    remove_active_user(email);
    char msg[BUF_SIZE];
    snprintf(msg, BUF_SIZE, "User %s exited the application.\n", email);
    send(client_sock, msg, strlen(msg), 0);
    close(client_sock);
}

// ---------------- CLIENT MENU ----------------
void client_menu(int client_sock, char *email)
{
    char buffer[BUF_SIZE];
    int acc_no = get_account_number_by_email(email);

    const char *menu =
        "\n--- Customer Session Started ---\n"
        "\nMenu:\n"
        "1. My account number\n"
        "2. View Account Balance\n"
        "3. Deposit Money\n"
        "4. Withdraw Money\n"
        "5. Transfer Funds\n"
        "6. Apply for a Loan\n"
        "7. Change Password\n"
        "8. Adding Feedback\n"
        "9. View Transaction History\n"
        "10. Logout\n"
        "Choice: ";

    send(client_sock, menu, strlen(menu), 0);

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));

        int n = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0)
        {
            printf("Client with email %s disconnected unexpectedly.\n", email);
            remove_active_user(email);
            break;
        }
        buffer[strcspn(buffer, "\r\n")] = 0;

        if (strcmp(buffer, "1") == 0)
        {
            char response[BUF_SIZE];
            snprintf(response, BUF_SIZE, "Your account number is: %d\n", acc_no);
            send(client_sock, response, strlen(response), 0);
        }
        else if (strcmp(buffer, "2") == 0)
            view_balance(client_sock, acc_no);
        else if (strcmp(buffer, "3") == 0)
            deposit_money(client_sock, acc_no);
        else if (strcmp(buffer, "4") == 0)
            withdraw_money(client_sock, acc_no);
        else if (strcmp(buffer, "5") == 0)
            transfer_funds(client_sock, acc_no);
        else if (strcmp(buffer, "6") == 0)
            apply_loan(client_sock, acc_no);
        else if (strcmp(buffer, "7") == 0)
            change_password(client_sock, acc_no);
        else if (strcmp(buffer, "8") == 0)
            add_feedback(client_sock, acc_no);
        else if (strcmp(buffer, "9") == 0)
            view_transactions(client_sock, acc_no);
        else if (strcmp(buffer, "10") == 0)
        {
            remove_active_user(email);
            char response[BUF_SIZE];
            snprintf(response, BUF_SIZE, "User %s logged out successfully.\n", email);
            send(client_sock, response, strlen(response), 0);
            break;
        }
        else
        {
            char response[BUF_SIZE];
            snprintf(response, BUF_SIZE, "Invalid option!\n");
            send(client_sock, response, strlen(response), 0);
        }

        send(client_sock, menu, strlen(menu), 0);
    }
}
