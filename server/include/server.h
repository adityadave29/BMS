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

#define PORT 9090
#define BUF_SIZE 1024
#define MAX_LINE 512

// Fixed-size string length constants
#define EMAIL_LEN 64
#define PASS_LEN 64
#define ROLE_LEN 20
#define PHONE_LEN 20
#define ADDR_LEN 128
#define TYPE_LEN 20
#define NAME_LEN 64
#define POS_LEN 32
#define DEPT_LEN 32
#define TIME_LEN 32
#define REASON_LEN 64
#define FEEDBACK_LEN 256

// Binary database file paths
#define DATA_DIR "data/"
#define USERS_DB DATA_DIR "users.dat"
#define CUSTOMERS_DB DATA_DIR "customers.dat"
#define EMPLOYEES_DB DATA_DIR "employees.dat"
#define BALANCE_DB DATA_DIR "balance.dat"
#define LOANS_DB DATA_DIR "loans.dat"
#define TRANSACTIONS_DB DATA_DIR "transactions.dat"
#define FEEDBACKS_DB DATA_DIR "feedbacks.dat"
#define COUNTERS_DB DATA_DIR "counters.dat"

// ============================================================================
// Fixed-Size Binary Record Structures
// ============================================================================

// 1. User Authentication & Session Record
typedef struct {
    int id;
    char email[EMAIL_LEN];
    char password[PASS_LEN];
    char role[ROLE_LEN];        // "user", "admin", "employee", "manager"
    int is_active;              // 1 = active, 0 = deactivated
    int is_logged_in;           // 1 = currently logged in, 0 = logged out
} User;

// 2. Customer Record
typedef struct {
    int account_no;
    char phone[PHONE_LEN];
    char email[EMAIL_LEN];
    char password[PASS_LEN];
    char address[ADDR_LEN];
    char account_type[TYPE_LEN]; // "saving", "current"
} Customer;

// 3. Employee Record
typedef struct {
    int emp_id;
    char name[NAME_LEN];
    char email[EMAIL_LEN];
    char password[PASS_LEN];
    char phone[PHONE_LEN];
    char address[ADDR_LEN];
    char position[POS_LEN];
    char department[DEPT_LEN];
} Employee;

// 4. Account Balance Record
typedef struct {
    int account_no;
    double balance;
} AccountBalance;

// 5. Transaction Record
typedef struct {
    int transaction_id;
    int from_acc;
    int to_acc;
    double amount;
    double before_bal;
    double after_bal;
    char timestamp[TIME_LEN];
} Transaction;

// 6. Loan Record
typedef struct {
    int account_no;
    int loan_id;
    double amount;
    char loan_type[POS_LEN];     // "personal", "home", "car"
    char status[POS_LEN];        // "pending", "approved", "rejected"
    int assigned_to;             // Employee ID (0 = unassigned)
    char reason[REASON_LEN];
    char timestamp[TIME_LEN];
} Loan;

// 7. Feedback Record
typedef struct {
    int feedback_id;
    int account_no;
    char feedback[FEEDBACK_LEN];
    char timestamp[TIME_LEN];
} Feedback;

// 8. System Counters Record (for atomic ID generation)
typedef struct {
    int account_no;
    int loan_id;
    int feedback_id;
    int transaction_id;
    int employee_id;
} Counters;

// ============================================================================
// Database & Record Locking API
// ============================================================================
extern pthread_mutex_t counters_mutex;
extern pthread_mutex_t users_mutex;
extern pthread_mutex_t customers_mutex;
extern pthread_mutex_t employees_mutex;
extern pthread_mutex_t balance_mutex;
extern pthread_mutex_t loans_mutex;
extern pthread_mutex_t transactions_mutex;
extern pthread_mutex_t feedbacks_mutex;

int lock_record(int fd, off_t offset, size_t size, short lock_type);
void init_database(void);

// Atomic counter helpers
int get_next_account_number(void);
int get_next_loan_id(void);
int get_next_feedback_id(void);
int get_next_transaction_id(void);
int get_next_employee_id(void);

// User / Account helpers
int get_account_number_by_email(const char *email);
double get_balance(int acc_no);
void update_balance(int acc_no, double new_balance);
int set_user_login_status(const char *email, int is_logged_in);
void remove_active_user(const char *email);

// ============================================================================
// Core & Role Handlers
// ============================================================================
void *handle_client_thread(void *arg);
void handle_client(int client_sock);
void login_user(int client_sock, char *email, char *password, char *role);
void exit_session(int client_sock);
void exit_application(int client_sock, const char *email);

// Customer operations
void client_menu(int client_sock, char *email);
void view_balance(int client_sock, int acc_no);
void deposit_money(int client_sock, int acc_no);
void withdraw_money(int client_sock, int acc_no);
void transfer_funds(int client_sock, int acc_no);
void apply_loan(int client_sock, int acc_no);
void change_password(int client_sock, int acc_no);
void add_feedback(int client_sock, int acc_no);
void view_transactions(int client_sock, int acc_no);
void record_transaction(int from, int to, double amount, double before, double after);

// Admin operations
void admin_menu(int client_sock, char *email);
void add_new_employee(int client_sock);
void modify_customer_details(int client_sock, char *email);
void modify_employee_details(int client_sock, char *email);

// Employee operations
void employee_menu(int client_sock, char *email);
void add_new_customer(int client_sock);
void modify_customer_details_emp(int client_sock, char *email);
void view_assigned_loans(int client_sock, char *email);
void approve_reject_loans(int client_sock);

// Manager operations
void manager_menu(int client_sock, char *email);
void activate_deactivate_customer(int client_sock);
void list_all_employees(int client_sock);
void list_unassigned_loans(int client_sock);
void assign_loan_to_employee(int client_sock);
void review_customer_feedback(int client_sock);
void change_manager_own_password(int client_sock, char *email);

#endif