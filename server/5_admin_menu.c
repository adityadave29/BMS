#include "server.h"

// ============================================= ADMIN MENU ===========================================================>
void admin_menu(int client_sock, char *email)
{
    char buffer[BUF_SIZE];

    const char *menu =
        "\n--- Admin Session Started ---\n"
        "\nAdmin Menu:\n"
        "1. Add New Employee\n"
        "2. Modify Customer Details\n"
        "3. Modify Employee Details\n"
        "4. Logout\n";

    while (1)
    {
        // Send menu and prompt
        send(client_sock, menu, strlen(menu), 0);
        send(client_sock, "Choice: ", 8, 0);

        memset(buffer, 0, sizeof(buffer));
        int n = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0)
        {
            printf("Admin with email %s disconnected unexpectedly.\n", email);
            remove_active_user(email);
            break;
        }

        buffer[n] = '\0';
        buffer[strcspn(buffer, "\r\n")] = 0; // remove newline

        if (strcmp(buffer, "1") == 0)
        {
            add_new_employee(client_sock);
        }
        else if (strcmp(buffer, "2") == 0)
        {
            modify_customer_details(client_sock, email); // returns here after exit
        }
        else if (strcmp(buffer, "3") == 0)
        {
            modify_employee_details(client_sock, email);
        }
        else if (strcmp(buffer, "4") == 0)
        {

            remove_active_user(email);
            snprintf(buffer, BUF_SIZE, "email %s logged out successfully.\n", email);
            send(client_sock, buffer, strlen(buffer), 0);
            break;
        }
        else
        {
            send(client_sock, "Invalid option!\n", 16, 0);
        }
    }
}

void add_new_employee(int client_sock)
{
    char msg[BUF_SIZE], recv_buf[BUF_SIZE];
    char name[50] = "", email[50] = "", password[50] = "", phone[20] = "", address[100] = "", position[50] = "", department[50] = "";

    const char *prompts[] = {
        "Enter employee name: ",
        "Enter employee email: ",
        "Enter employee password: ",
        "Enter phone number: ",
        "Enter address: ",
        "Enter position: ",
        "Enter department: "};
    char *fields[] = {name, email, password, phone, address, position, department};
    int field_sizes[] = {sizeof(name), sizeof(email), sizeof(password), sizeof(phone), sizeof(address), sizeof(position), sizeof(department)};
    int num_fields = 7;

    // --- Collect employee details from client ---
    for (int i = 0; i < num_fields; i++)
    {
        send(client_sock, prompts[i], strlen(prompts[i]), 0);
        memset(recv_buf, 0, sizeof(recv_buf));

        int n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
        if (n <= 0)
            return;

        recv_buf[n] = '\0';
        recv_buf[strcspn(recv_buf, "\n")] = 0; // remove newline

        // ✅ Correct safe copy per field
        strncpy(fields[i], recv_buf, field_sizes[i] - 1);
        fields[i][field_sizes[i] - 1] = '\0';
    }

    // --- Lock and update ID counter ---
    int fd = open(ACCOUNT_FILE, O_RDWR);
    if (fd == -1)
    {
        snprintf(msg, BUF_SIZE, "Error opening employee ID file.\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl(fd, F_SETLKW, &lock);

    FILE *fp_acc = fdopen(fd, "r+");
    if (!fp_acc)
    {
        snprintf(msg, BUF_SIZE, "Error accessing employee ID file.\n");
        send(client_sock, msg, strlen(msg), 0);
        close(fd);
        return;
    }

    int account_count, loan_id, feedback_id, transaction_id, employee_id;
    if (fscanf(fp_acc, "%d %d %d %d %d", &account_count, &loan_id, &feedback_id, &transaction_id, &employee_id) != 5)
    {
        snprintf(msg, BUF_SIZE, "Error reading ID file.\n");
        send(client_sock, msg, strlen(msg), 0);
        fclose(fp_acc);
        return;
    }

    int new_emp_id = employee_id + 1;
    rewind(fp_acc);
    fprintf(fp_acc, "%d %d %d %d %d\n", account_count, loan_id, feedback_id, transaction_id, new_emp_id);
    fflush(fp_acc);

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    fclose(fp_acc);

    // --- Append to EMPLOYEE file ---
    FILE *fp_emp = fopen(EMPLOYEE, "a");
    if (!fp_emp)
    {
        send(client_sock, "Error opening employee file\n", 28, 0);
        return;
    }

    fprintf(fp_emp, "%d %s %s %s %s %s %s %s\n",
            new_emp_id, name, email, password, phone, address, position, department);
    fflush(fp_emp);
    fclose(fp_emp);

    // --- Append to USERS file ---
    FILE *fp_login = fopen(USERS, "a");
    if (!fp_login)
    {
        send(client_sock, "Error opening login file\n", 25, 0);
        return;
    }

    fprintf(fp_login, "%s %s %s 1\n", email, password, position);
    fflush(fp_login);
    fclose(fp_login);

    // --- Confirmation message ---
    snprintf(msg, BUF_SIZE, "Employee added successfully! Employee ID: %d\n", new_emp_id);
    send(client_sock, msg, strlen(msg), 0);
}

void modify_employee_details(int client_sock, char *email)
{
    char buffer[BUF_SIZE], recv_buf[BUF_SIZE];
    int emp_id, found = 0;
    char old_email[100];

    send(client_sock, "Enter employee ID: ", 19, 0);
    int n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[n] = '\0';
    emp_id = atoi(recv_buf);

    FILE *fp = fopen(EMPLOYEE, "r");
    if (!fp)
    {
        send(client_sock, "Error opening employee file!\n", 29, 0);
        return;
    }

    char line[512];
    char emp_name[50], emp_email[100], emp_pass[50], emp_phone[50];
    char emp_address[200], emp_position[50], emp_dept[50];

    while (fgets(line, sizeof(line), fp))
    {
        int file_id;
        if (sscanf(line, "%d %49s %99s %49s %49s %199s %49s %49s",
                   &file_id, emp_name, emp_email, emp_pass, emp_phone,
                   emp_address, emp_position, emp_dept) == 8)
        {
            if (file_id == emp_id)
            {
                found = 1;
                strcpy(old_email, emp_email);
                break;
            }
        }
    }
    fclose(fp);

    if (!found)
    {
        send(client_sock, "Employee not found!\n", 20, 0);
        return;
    }

    while (1)
    {
        const char *menu =
            "\n1. View current details\n"
            "2. Change email\n"
            "3. Change password\n"
            "4. Change phone number\n"
            "5. Change address\n"
            "6. Change position\n"
            "7. Change department\n"
            "8. Exit to Admin Menu\n"
            "Choice: ";
        send(client_sock, menu, strlen(menu), 0);

        n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
        if (n <= 0)
            return;
        recv_buf[n] = '\0';
        recv_buf[strcspn(recv_buf, "\n")] = 0;

        int choice = atoi(recv_buf);

        if (choice == 1)
        {
            snprintf(buffer, BUF_SIZE,
                     "ID: %d\nName: %s\nEmail: %s\nPassword: %s\nPhone: %s\nAddress: %s\nPosition: %s\nDepartment: %s\n",
                     emp_id, emp_name, emp_email, emp_pass, emp_phone, emp_address, emp_position, emp_dept);
            send(client_sock, buffer, strlen(buffer), 0);
        }
        else if (choice >= 2 && choice <= 7)
        {
            const char *prompt;
            if (choice == 2)
                prompt = "Enter new email: ";
            else if (choice == 3)
                prompt = "Enter new password: ";
            else if (choice == 4)
                prompt = "Enter new phone: ";
            else if (choice == 5)
                prompt = "Enter new address: ";
            else if (choice == 6)
                prompt = "Enter new position: ";
            else
                prompt = "Enter new department: ";

            send(client_sock, prompt, strlen(prompt), 0);
            n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
            if (n <= 0)
                return;
            recv_buf[n] = '\0';
            recv_buf[strcspn(recv_buf, "\n")] = 0;

            if (choice == 2)
                strcpy(emp_email, recv_buf);
            else if (choice == 3)
                strcpy(emp_pass, recv_buf);
            else if (choice == 4)
                strcpy(emp_phone, recv_buf);
            else if (choice == 5)
                strcpy(emp_address, recv_buf);
            else if (choice == 6)
                strcpy(emp_position, recv_buf);
            else if (choice == 7)
                strcpy(emp_dept, recv_buf);

            // --- Update EMPLOYEE file ---
            FILE *fpr = fopen(EMPLOYEE, "r");
            FILE *fpw = fopen("temp_employee.txt", "w");

            while (fgets(line, sizeof(line), fpr))
            {
                int file_id;
                char name[50], email[100], pass[50], phone[50];
                char address[200], position[50], dept[50];

                if (sscanf(line, "%d %49s %99s %49s %49s %199s %49s %49s",
                           &file_id, name, email, pass, phone, address, position, dept) != 8)
                    continue;

                if (file_id == emp_id)
                {
                    strcpy(name, emp_name);
                    strcpy(email, emp_email);
                    strcpy(pass, emp_pass);
                    strcpy(phone, emp_phone);
                    strcpy(address, emp_address);
                    strcpy(position, emp_position);
                    strcpy(dept, emp_dept);
                }

                fprintf(fpw, "%d %s %s %s %s %s %s %s\n",
                        file_id, name, email, pass, phone, address, position, dept);
            }
            fclose(fpr);
            fclose(fpw);
            remove(EMPLOYEE);
            rename("temp_employee.txt", EMPLOYEE);
            // --- Update USERS file if needed ---
            if (choice == 2 || choice == 3 || choice == 6)
            {
                FILE *flog = fopen(USERS, "r");
                FILE *ftmp = fopen("temp_login.txt", "w");
                char login_line[256];

                if (!flog || !ftmp)
                {
                    perror("File open error");
                    if (flog)
                        fclose(flog);
                    if (ftmp)
                        fclose(ftmp);
                    return;
                }

                while (fgets(login_line, sizeof(login_line), flog))
                {
                    char login_email[100], login_pass[100], login_role[50];
                    int activation = 1; // default

                    // Expect 4 space-separated fields now
                    if (sscanf(login_line, "%99s %99s %49s %d", login_email, login_pass, login_role, &activation) < 4)
                        continue;

                    // --- Update email ---
                    if (choice == 2 && strcmp(login_email, old_email) == 0)
                    {
                        fprintf(ftmp, "%s %s %s %d\n", emp_email, login_pass, login_role, activation);
                    }
                    // --- Update password ---
                    else if (choice == 3 && strcmp(login_email, emp_email) == 0)
                    {
                        fprintf(ftmp, "%s %s %s %d\n", login_email, emp_pass, login_role, activation);
                    }
                    // --- Update role when position changes ---
                    else if (choice == 6 && strcmp(login_email, emp_email) == 0)
                    {
                        fprintf(ftmp, "%s %s %s %d\n", login_email, login_pass, emp_position, activation);
                    }
                    // --- Keep unchanged lines ---
                    else
                    {
                        fputs(login_line, ftmp);
                    }
                }

                fclose(flog);
                fclose(ftmp);

                remove(USERS);
                rename("temp_login.txt", USERS);
            }

            snprintf(buffer, BUF_SIZE, "Update successful!\n");
            send(client_sock, buffer, strlen(buffer), 0);
        }
        else if (choice == 8)
        {
            admin_menu(client_sock, email);
            return;
        }
        else
        {
            send(client_sock, "Invalid choice!\n", 16, 0);
        }
    }
}

void modify_customer_details(int client_sock, char *email)
{
    char buffer[BUF_SIZE], recv_buf[BUF_SIZE];
    int acc_no, found = 0;

    // Step 1: Ask for account number
    send(client_sock, "Enter customer account number: ", 32, 0);
    int n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[n] = '\0';
    acc_no = atoi(recv_buf);

    // Step 2: Search for customer
    FILE *fp = fopen(CUSTOMERS, "r");
    if (!fp)
    {
        send(client_sock, "Error opening customer file!\n", 29, 0);
        return;
    }

    char line[512], cust_phone[50], cust_email[100], cust_pass[50], cust_address[200], cust_type[50];
    while (fgets(line, sizeof(line), fp))
    {
        int file_acc;
        char temp[512];
        sscanf(line, "%d %49s %99s %49s %199[^\n]", &file_acc, cust_phone, cust_email, cust_pass, temp);
        if (file_acc == acc_no)
        {
            found = 1;
            // Extract type (last word)
            char *last_space = strrchr(temp, ' ');
            if (last_space)
            {
                strcpy(cust_type, last_space + 1);
                *last_space = '\0';
            }
            else
                cust_type[0] = '\0';
            strcpy(cust_address, temp);
            break;
        }
    }
    fclose(fp);

    if (!found)
    {
        send(client_sock, "Account not found!\n", 19, 0);
        return;
    }

    // Step 3: Show modify menu
    while (1)
    {
        const char *menu =
            "\n1. View current details\n"
            "2. Change email\n"
            "3. Change password\n"
            "4. Change phone number\n"
            "5. Change address\n"
            "6. Change account type\n"
            "7. Exit to Admin Menu\n"
            "Choice: ";
        send(client_sock, menu, strlen(menu), 0);

        n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
        if (n <= 0)
            return;
        recv_buf[n] = '\0';
        recv_buf[strcspn(recv_buf, "\n")] = 0;

        int choice = atoi(recv_buf);

        if (choice == 1)
        {
            snprintf(buffer, BUF_SIZE,
                     "Account: %d\nPhone: %s\nEmail: %s\nPassword: %s\nAddress: %s\nType: %s\n",
                     acc_no, cust_phone, cust_email, cust_pass, cust_address, cust_type);
            send(client_sock, buffer, strlen(buffer), 0);
        }
        else if (choice >= 2 && choice <= 6)
        {
            const char *prompt;
            if (choice == 2)
                prompt = "Enter new email: ";
            else if (choice == 3)
                prompt = "Enter new password: ";
            else if (choice == 4)
                prompt = "Enter new phone: ";
            else if (choice == 5)
                prompt = "Enter new address: ";
            else
                prompt = "Enter new account type: ";

            send(client_sock, prompt, strlen(prompt), 0);

            n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
            if (n <= 0)
                return;
            recv_buf[n] = '\0';
            recv_buf[strcspn(recv_buf, "\n")] = 0;

            char old_email[100];
            strcpy(old_email, cust_email); // store old email before change

            // Update local variables
            if (choice == 2)
                strcpy(cust_email, recv_buf);
            else if (choice == 3)
                strcpy(cust_pass, recv_buf);
            else if (choice == 4)
                strcpy(cust_phone, recv_buf);
            else if (choice == 5)
                strcpy(cust_address, recv_buf);
            else if (choice == 6)
                strcpy(cust_type, recv_buf);

            // Update d_customer.txt
            FILE *fpr = fopen(CUSTOMERS, "r");
            FILE *fpw = fopen("temp_customer.txt", "w");
            if (!fpr || !fpw)
            {
                send(client_sock, "File operation error!\n", 23, 0);
                if (fpr)
                    fclose(fpr);
                if (fpw)
                    fclose(fpw);
                return;
            }

            while (fgets(line, sizeof(line), fpr))
            {
                int file_acc;
                char phone[50], email[100], pass[50], address[200], type[50];
                char temp[512];
                sscanf(line, "%d %49s %99s %49s %199[^\n]", &file_acc, phone, email, pass, temp);

                char *last_space = strrchr(temp, ' ');
                if (last_space)
                {
                    strcpy(type, last_space + 1);
                    *last_space = '\0';
                }
                else
                    type[0] = '\0';
                strcpy(address, temp);

                if (file_acc == acc_no)
                {
                    if (choice == 2)
                        strcpy(email, cust_email);
                    else if (choice == 3)
                        strcpy(pass, cust_pass);
                    else if (choice == 4)
                        strcpy(phone, cust_phone);
                    else if (choice == 5)
                        strcpy(address, cust_address);
                    else if (choice == 6)
                        strcpy(type, cust_type);
                }

                fprintf(fpw, "%d %s %s %s %s %s\n", file_acc, phone, email, pass, address, type);
            }
            fclose(fpr);
            fclose(fpw);
            remove(CUSTOMERS);
            rename("temp_customer.txt", CUSTOMERS);

            // --- Update d_login.txt (USERS) ---
            if (choice == 2 || choice == 3)
            {
                FILE *flog = fopen(USERS, "r");
                FILE *ftmp = fopen("temp_login.txt", "w");
                if (!flog || !ftmp)
                {
                    send(client_sock, "Login file update error!\n", 26, 0);
                    if (flog)
                        fclose(flog);
                    if (ftmp)
                        fclose(ftmp);
                    return;
                }

                char login_line[256];
                while (fgets(login_line, sizeof(login_line), flog))
                {
                    char login_email[100], login_pass[100], login_role[50];
                    int activation = 1; // Default if missing

                    // Expecting: EMAIL PASSWORD ROLE ACTIVATION
                    if (sscanf(login_line, "%99s %99s %49s %d", login_email, login_pass, login_role, &activation) < 4)
                        continue;

                    // --- If email is changed ---
                    if (choice == 2)
                    {
                        if (strcmp(login_email, old_email) == 0)
                            fprintf(ftmp, "%s %s %s %d\n", cust_email, login_pass, login_role, activation);
                        else
                            fputs(login_line, ftmp);
                    }
                    // --- If password is changed ---
                    else if (choice == 3)
                    {
                        if (strcmp(login_email, cust_email) == 0)
                            fprintf(ftmp, "%s %s %s %d\n", login_email, cust_pass, login_role, activation);
                        else
                            fputs(login_line, ftmp);
                    }
                }

                fclose(flog);
                fclose(ftmp);

                remove(USERS);
                rename("temp_login.txt", USERS);
            }

            snprintf(buffer, BUF_SIZE, "Update successful!\n");
            send(client_sock, buffer, strlen(buffer), 0);
        }
        else if (choice == 7)
        {
            admin_menu(client_sock, email);
            return;
        }
        else
        {
            send(client_sock, "Invalid choice!\n", 16, 0);
        }
    }
}
