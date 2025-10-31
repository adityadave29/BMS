#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

#define PORT 8080
#define BUF_SIZE 1024
#define MAX_LINE 512
#define USERS "d_login.txt"
#define TRANSACTIONS "d_transaction.txt"
#define LOAN "d_loan.txt"
#define FEEDBACK "d_feedback.txt"
#define EMPLOYEE "d_employee.txt"
#define CUSTOMERS "d_customers.txt"
#define ADMIN "d_admin.txt"
#define BALANCE "d_balance.txt"
#define ACCOUNT_FILE "d_account_number.txt"
#define ACTIVE_USERS "d_active.txt"

// Function prototypes
// ------------------ User Authentication ------------------
void login_user(int client_sock, char *email, char *password, char *role);
void signup_user(int client_sock, char *phone_number, char *email, char *password, char *address, char *account_type);
void change_password(int client_sock, int acc_no);
void exit_session(int client_sock);

// ------------------ Core Handlers ------------------
void handle_client(int client_sock);
void client_menu(int client_sock, char *email);

// ------------------ Account Helpers ------------------
int get_next_account_number();
int get_account_number_by_email(char *email);
int get_balance(int acc_no);
void update_balance(int acc_no, int new_balance);

// ------------------ Banking Operations ------------------
void view_balance(int client_sock, int acc_no);
void deposit_money(int client_sock, int acc_no);
void withdraw_money(int client_sock, int acc_no);
void transfer_funds(int client_sock, int acc_no);

// ------------------ Loan & Feedback ------------------
void apply_loan(int client_sock, int acc_no);
void add_feedback(int client_sock, int acc_no);

// ------------------ Transactions ------------------
void record_transaction(int from, int to, int amount, int before, int after);
void view_transactions(int client_sock, int acc_no);

void admin_menu(int client_sock, char *email);
void employee_menu(int client_sock, char *email);
void manager_menu(int client_sock, char *email);

// Function prototypes
void add_new_employee(int client_sock);
void modify_customer_details(int client_sock, char *email);
void modify_employee_details(int client_sock, char *email);

// manager
void activate_deactivate_customer(int client_sock);
void assign_loan_to_employee(int client_sock);
void review_customer_feedback(int client_sock);
void change_employee_password(int client_sock);
void change_client_password(int client_sock);

void exit_application(int client_sock, const char *email);
void remove_active_user(const char *email);
#endif