#include "server.h"

// for primary key retrieval
int get_account_number_by_email(char *email)
{
    FILE *fp = fopen(CUSTOMERS, "r");
    if (!fp)
        return -1;

    char line[512];
    int acc_no;
    char phone[50], file_email[100], password[50], address[200], account_type[50];

    while (fgets(line, sizeof(line), fp))
    {
        if (sscanf(line, "%d %s %s %s %s %s", &acc_no, phone, file_email, password, address, account_type) == 6)
        {
            if (strcmp(file_email, email) == 0)
            {
                fclose(fp);
                return acc_no;
            }
        }
    }

    fclose(fp);
    return -1;
}

// check current balance from balance db
void view_balance(int client_sock, int acc_no)
{
    FILE *fp = fopen(BALANCE, "r");
    if (!fp)
    {
        char err_msg[] = "Error: Could not open balance file.\n";
        send(client_sock, err_msg, strlen(err_msg), 0);
        return;
    }

    int file_acc_no, balance = -1;
    char line[BUF_SIZE];

    while (fgets(line, sizeof(line), fp))
    {
        if (sscanf(line, "%d %d", &file_acc_no, &balance) == 2)
        {
            if (file_acc_no == acc_no)
            {
                break;
            }
        }
    }

    fclose(fp);

    char msg[BUF_SIZE];
    if (balance != -1)
    {
        snprintf(msg, BUF_SIZE, "Your current balance is: %d\n", balance);
    }
    else
    {
        snprintf(msg, BUF_SIZE, "Account number %d not found!\n", acc_no);
    }

    send(client_sock, msg, strlen(msg), 0);
}

// apply for loan in loan db
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
    recv_buf[n] = '\0';
    recv_buf[strcspn(recv_buf, "\n")] = 0;
    int loan_amount = atoi(recv_buf);

    snprintf(msg, BUF_SIZE, "Enter loan type (personal/home/car): ");
    send(client_sock, msg, strlen(msg), 0);
    memset(recv_buf, 0, sizeof(recv_buf));
    n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[n] = '\0';
    recv_buf[strcspn(recv_buf, "\n")] = 0;

    char loan_type[50];
    strncpy(loan_type, recv_buf, sizeof(loan_type));

    // ---------------- LOCK ACCOUNT FILE ----------------
    int fd = open(ACCOUNT_FILE, O_RDWR);
    if (fd == -1)
    {
        snprintf(msg, BUF_SIZE, "Error: cannot open loan ID file.\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    struct flock lock;
    lock.l_type = F_WRLCK; // exclusive write lock
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    fcntl(fd, F_SETLKW, &lock); // Wait if locked

    FILE *fp_acc = fdopen(fd, "r+");
    if (!fp_acc)
    {
        snprintf(msg, BUF_SIZE, "Error: cannot access loan ID file.\n");
        send(client_sock, msg, strlen(msg), 0);
        close(fd);
        return;
    }

    // ---------------- UPDATE LOAN ID ----------------
    int account_count, loan_id, feedback_id, transaction_id, employee_id;
    fscanf(fp_acc, "%d %d %d %d %d", &account_count, &loan_id, &feedback_id, &transaction_id, &employee_id);
    int new_loan_id = loan_id + 1;
    rewind(fp_acc);
    fprintf(fp_acc, "%d %d %d %d %d\n", account_count, new_loan_id, feedback_id, transaction_id, employee_id);
    fflush(fp_acc);

    // ---------------- UNLOCK FILE ----------------
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    fclose(fp_acc);

    // ---------------- WRITE LOAN ENTRY ----------------
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char timestamp[50];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    FILE *fp = fopen(LOAN, "a");
    if (fp)
    {
        fprintf(fp, "%d %d %d %s pending 0 - %s\n",
                acc_no, new_loan_id, loan_amount, loan_type, timestamp);
        fclose(fp);
    }

    // ---------------- CONFIRMATION ----------------
    snprintf(msg, BUF_SIZE, "Loan application submitted successfully. Your Loan ID: %d\n", new_loan_id);
    send(client_sock, msg, strlen(msg), 0);
}

// change your own password
void change_password(int client_sock, int acc_no)
{
    char msg[256], recv_buf[256];
    char old_pass[50], new_pass[50], confirm_pass[50];
    char email[100];
    int found = 0;

    // Step 1: Find account in d_customers.txt
    FILE *fp = fopen(CUSTOMERS, "r");
    if (!fp)
    {
        sprintf(msg, "Error opening customer file!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    char line[512], current_pass[50];
    while (fgets(line, sizeof(line), fp))
    {
        int file_acc;
        char phone[50], acc_email[100], pass[50], address[200], acc_type[50];
        sscanf(line, "%d %49s %99s %49s %199[^\n] %49s", &file_acc, phone, acc_email, pass, address, acc_type);
        if (file_acc == acc_no)
        {
            strcpy(email, acc_email);
            strcpy(current_pass, pass);
            found = 1;
            break;
        }
    }
    fclose(fp);

    if (!found)
    {
        sprintf(msg, "Account not found!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    // Step 2: Verify old password
    sprintf(msg, "Enter old password: ");
    send(client_sock, msg, strlen(msg), 0);
    int n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[n] = '\0';
    recv_buf[strcspn(recv_buf, "\n")] = 0;
    strcpy(old_pass, recv_buf);

    if (strcmp(old_pass, current_pass) != 0)
    {
        sprintf(msg, "Old password incorrect!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    // Step 3: Ask new password
    sprintf(msg, "Enter new password: ");
    send(client_sock, msg, strlen(msg), 0);
    n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[n] = '\0';
    recv_buf[strcspn(recv_buf, "\n")] = 0;
    strcpy(new_pass, recv_buf);

    sprintf(msg, "Confirm new password: ");
    send(client_sock, msg, strlen(msg), 0);
    n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[n] = '\0';
    recv_buf[strcspn(recv_buf, "\n")] = 0;
    strcpy(confirm_pass, recv_buf);

    if (strcmp(new_pass, confirm_pass) != 0)
    {
        sprintf(msg, "Passwords do not match!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    // Step 4: Update d_customers.txt
    FILE *fc = fopen(CUSTOMERS, "r");
    FILE *fc_temp = fopen("temp_customer.txt", "w");
    while (fgets(line, sizeof(line), fc))
    {
        int file_acc;
        char phone[50], acc_email[100], pass[50], address[200], acc_type[50];
        sscanf(line, "%d %49s %99s %49s %199[^\n] %49s", &file_acc, phone, acc_email, pass, address, acc_type);
        if (file_acc == acc_no)
            fprintf(fc_temp, "%d %s %s %s %s %s\n", file_acc, phone, acc_email, new_pass, address, acc_type);
        else
            fputs(line, fc_temp);
    }
    fclose(fc);
    fclose(fc_temp);
    remove(CUSTOMERS);
    rename("temp_customer.txt", CUSTOMERS);

    // ✅ Step 5: Update d_login.txt (preserve ACTIVATION)
    FILE *fl = fopen(USERS, "r");
    FILE *fl_temp = fopen("temp_login.txt", "w");
    while (fgets(line, sizeof(line), fl))
    {
        char login_email[100], pass[50], role[20];
        int activation;

        int count = sscanf(line, "%99s %49s %19s %d", login_email, pass, role, &activation);
        if (count == 4)
        {
            if (strcmp(login_email, email) == 0)
                fprintf(fl_temp, "%s %s %s %d\n", login_email, new_pass, role, activation);
            else
                fprintf(fl_temp, "%s %s %s %d\n", login_email, pass, role, activation);
        }
        else
        {
            // handle malformed line (fallback)
            fputs(line, fl_temp);
        }
    }
    fclose(fl);
    fclose(fl_temp);
    remove(USERS);
    rename("temp_login.txt", USERS);

    sprintf(msg, "Password updated successfully!\n");
    send(client_sock, msg, strlen(msg), 0);
}

// add feedback to feedback db
void add_feedback(int client_sock, int acc_no)
{
    char msg[BUF_SIZE], feedback[BUF_SIZE];

    // --- Lock and open account file ---
    int fd = open(ACCOUNT_FILE, O_RDWR);
    if (fd < 0)
    {
        snprintf(msg, BUF_SIZE, "Error opening account number file!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    // Apply exclusive lock
    if (flock(fd, LOCK_EX) != 0)
    {
        snprintf(msg, BUF_SIZE, "Unable to acquire file lock!\n");
        send(client_sock, msg, strlen(msg), 0);
        close(fd);
        return;
    }

    FILE *fp = fdopen(fd, "r+");
    if (!fp)
    {
        snprintf(msg, BUF_SIZE, "Error accessing file pointer!\n");
        send(client_sock, msg, strlen(msg), 0);
        flock(fd, LOCK_UN);
        close(fd);
        return;
    }

    // --- Read and increment feedback ID safely ---
    int account_count, loan_id, feedback_id, transaction_id, employee_id;
    fscanf(fp, "%d %d %d %d %d", &account_count, &loan_id, &feedback_id, &transaction_id, &employee_id);

    feedback_id++; // increment feedback ID
    rewind(fp);
    fprintf(fp, "%d\n%d\n%d\n%d\n%d\n", account_count, loan_id, feedback_id, transaction_id, employee_id);
    fflush(fp);

    // Unlock and close file
    flock(fd, LOCK_UN);
    fclose(fp); // this also closes fd

    // --- Ask client for feedback ---
    snprintf(msg, BUF_SIZE, "Enter your feedback: ");
    send(client_sock, msg, strlen(msg), 0);

    memset(feedback, 0, sizeof(feedback));
    int n = recv(client_sock, feedback, sizeof(feedback) - 1, 0);
    if (n <= 0)
        return;

    feedback[n] = '\0';
    feedback[strcspn(feedback, "\n")] = 0;

    // --- Timestamp ---
    time_t t = time(NULL);
    struct tm tm_info = *localtime(&t);
    char timestamp[50];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_info);

    // --- Write feedback entry ---
    FILE *fp_feedback = fopen("d_feedback.txt", "a");
    if (!fp_feedback)
    {
        snprintf(msg, BUF_SIZE, "Error opening feedback file!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    fprintf(fp_feedback, "%d %d %s %s\n", feedback_id, acc_no, feedback, timestamp);
    fclose(fp_feedback);

    snprintf(msg, BUF_SIZE, "Feedback submitted successfully!\n");
    send(client_sock, msg, strlen(msg), 0);
}

// after exit remove user from active table
void remove_active_user(const char *email)
{
    FILE *fp = fopen(ACTIVE_USERS, "r");
    if (!fp)
        return; // File doesn't exist yet

    FILE *temp = fopen("temp_active.txt", "w");
    if (!temp)
    {
        fclose(fp);
        return;
    }

    char a_email[100], a_pass[100], a_role[50];
    while (fscanf(fp, "%s %s %s", a_email, a_pass, a_role) != EOF)
    {
        // Write all entries except the one with the same email
        if (strcmp(a_email, email) != 0)
        {
            fprintf(temp, "%s %s %s\n", a_email, a_pass, a_role);
        }
    }

    fclose(fp);
    fclose(temp);

    // Replace old file with updated one
    remove(ACTIVE_USERS);
    rename("temp_active.txt", ACTIVE_USERS);
}

void exit_application(int client_sock, const char *email)
{
    remove_active_user(email);
    char msg[BUF_SIZE];
    snprintf(msg, BUF_SIZE, "User %s exited the application.\n", email);
    send(client_sock, msg, strlen(msg), 0);
    close(client_sock);
}

// ---------------- CLIENT MENU (Here we will add all the functionalities for customers)----------------
void client_menu(int client_sock, char *email)
{
    char buffer[BUF_SIZE];
    int acc_no = get_account_number_by_email(email);

    const char *menu =
        "\n-- - Session Started-- -\n"
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

        // Read client choice
        int n = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0)
        {
            // 🧠 Client disconnected unexpectedly (Ctrl+C or network)
            printf("Client with email %s disconnected unexpectedly.\n", email);
            remove_active_user(email);
            break;
        }
        buffer[n] = '\0';
        buffer[strcspn(buffer, "\n")] = 0;

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
            handle_client(client_sock);
            exit(0);
        }
        else
        {
            char response[BUF_SIZE];
            snprintf(response, BUF_SIZE, "Invalid option!\n");
            send(client_sock, response, strlen(response), 0);
        }

        // Always send menu after handling any choice
        send(client_sock, menu, strlen(menu), 0);
    }
}
