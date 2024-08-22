#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/shm.h>

#define MAX_TABLES 10
#define EARNINGS_FILE "earnings.txt"
#define ADMIN_SHM_KEY 12345
#define WAITER_SHM_KEY_BASE 5678

//struct for shared memory between waiter and hotelmanager processes.
typedef struct {
    int just_now_updated;
    int is_active;
    int bill_amount;
} BillData;

//struct for shared memory between hotelmanager and admin processes.
typedef struct{ 
    int termination_flag;
} AdminData;

int main() {
    //initialising some variables
    int total_tables;
    int total_earnings = 0;
    int total_wages = 0;
    int total_profit = 0;
    int admin_shm_id;
    AdminData *admin_data;
    int waiter_shm_ids[MAX_TABLES+1];
    BillData *table_data[MAX_TABLES+1];
    key_t waiter_shm_key;

    // Prompt user for the total number of tables
    printf("Enter the Total Number of Tables at the Hotel: ");
    scanf("%d", &total_tables);

    FILE *earnings_fd = fopen(EARNINGS_FILE, "w"); //intialising a write type file pointer just to clear the earnings.txt file
    if (earnings_fd == NULL) {
        perror("Error opening earnings file");
        exit(EXIT_FAILURE);
    }
    fclose(earnings_fd); //closing the write type file pointer
    // Open earnings file for writing but with append operation here.
    earnings_fd = fopen(EARNINGS_FILE, "a");
    if (earnings_fd == NULL) {
        perror("Error opening earnings file");
        exit(EXIT_FAILURE);
    }

    // Attach to admin shared memory
    admin_shm_id = shmget(ADMIN_SHM_KEY, sizeof(AdminData), 0666);
    if (admin_shm_id == -1) {
        perror("Error attaching to admin shared memory");
        exit(EXIT_FAILURE);
    }
    admin_data = (AdminData *) shmat(admin_shm_id, NULL, 0);
    if (admin_data == (AdminData *) -1) {
        perror("Error attaching to admin shared memory");
        exit(EXIT_FAILURE);
    }

    // Attach to waiter shared memories
    for (int i = 1; i <=total_tables; i++) {
        waiter_shm_key = WAITER_SHM_KEY_BASE + i;
        waiter_shm_ids[i] = shmget(waiter_shm_key, sizeof(BillData), 0666|IPC_CREAT);
        if (waiter_shm_ids[i] == -1) {
            perror("Error attaching to waiter shared memory");
            exit(EXIT_FAILURE);
        }
        table_data[i] = (BillData *) shmat(waiter_shm_ids[i], NULL, 0);
        table_data[i]->just_now_updated = 0;
        table_data[i]->is_active = 0;
        if (table_data[i] == (BillData *) -1) {
            perror("Error attaching to waiter shared memory");
            exit(EXIT_FAILURE);
        }
    }
    while(1)
    {
        int all_notactive=0; //creating a variable for checking if all the waiter processes are inactive... to ckeck if hotelmanager can go through with the termination or not.
        for(int i=1;i<=total_tables;i++){
            if(table_data[i]->is_active!=0){
                all_notactive=1;
            }
        }
        if((admin_data->termination_flag == 1)&&(all_notactive==0)) //if admin gives termination order.
        {
            break;
        }
        for(int i = 1;i<=total_tables;i++)
        {
            if(table_data[i]->just_now_updated) //carrying out the next operation only when the table data (order) is just updated
            {
                fprintf(earnings_fd, "Earning from Table %d: %d INR", i, table_data[i]->bill_amount); //adding a line in this format to show earnings from each table for each set of customers in earnings.txt file
                fprintf(earnings_fd, "\n");
                table_data[i]->just_now_updated = 0; //making just_now_updated variable 0 right after printing once to avoid the print statement from occuring multiple times during the sleep time of other processes
            }
        }
    }

    fclose(earnings_fd); //closing the append type file pointer
    FILE *file = fopen("earnings.txt", "r"); //initialising a read type file pointer
    if (file == NULL) {
        printf("Error opening file.\n");
        return 1;
    }

    int totalEarnings = 0;
    char line[100]; // Assuming each line has a maximum of 100 characters
    // Read each line and extract earnings
    while (fgets(line, sizeof(line), file)) {
        int earnings;
        // Extract earnings from the line
        if (sscanf(line, "Earning from Table %*d: %d INR", &earnings) == 1) {
            // Add earnings to total
            totalEarnings += earnings;
        }
    }

    fclose(file); //closing the read type file pointer

    FILE *fd = fopen("earnings.txt","a"); //initialising another append type file pointer
    if (fd == NULL) {
        printf("Error opening file.\n");
        return 1;
    }
    // Print total earnings
    printf("Total Earnings of Hotel: %d INR\n", totalEarnings);
    int wages = 40*totalEarnings/100;
    int profit = totalEarnings - wages;
    printf("Total Wages of Waiters: %d INR\n", wages);
    printf("Total Profit: %d INR\n", profit);
    // Write total earnings, total wages, and total profit to earnings file
    fprintf(earnings_fd, "Total Earnings of Hotel: %d INR\n", totalEarnings);//may change to dprintf
    fprintf(earnings_fd, "Total Wages of Waiters: %d INR\n", wages);
    fprintf(earnings_fd, "Total Profit: %d INR\n", profit);

    printf("Thank you for visiting the hotel...\n");
    // Close earnings file
    fclose(fd); //closing the append type file pointer
    sleep(5);
    // Detach from shared memories
    for (int i = 1; i <= total_tables; ++i) {
        shmdt((void *) table_data[i]);
        shmctl(waiter_shm_ids[i],IPC_RMID,NULL); //delete the shared memory
    }
    shmdt((void *) admin_data);
    return 0;
}