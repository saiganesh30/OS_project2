#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHARED_MEMORY_KEY_BASE 2000
#define MANAGER_SHM_KEY_BASE 5678

//struct for shared memory between table and waiter processes.
typedef struct {
    int table_number;
    int all_orders[10];
    int num_customers;
    int totalbill;
    int valid_order;
    int max_item_no;
    int terminate;
    int is_picked;
} TableData;

//struct for shared memory between waiter and hotelmanager processes.
typedef struct {
    int just_now_updated;
    int is_active;
    int bill_amount;
} BillData;

int isValidOrder(TableData *shared_order) //function to check whether the order received from table process is valid or not.
{
    if(shared_order->all_orders[0] != 0) //if 0th element of the all_orders array is not 0, the order is invalid, we updated this in the shared memory from table process.
    {
        shared_order->valid_order = 0;
        return 0;
    }
	for(int i = shared_order->max_item_no+1;i<=9;i++) //when the user gives an order number that is not there in the menu, it is shown as an invalid error.
	{
		if(shared_order->all_orders[i] != 0)
		{
			shared_order->valid_order = 0;
			return 0;
		}
	}
	shared_order->valid_order = 1; //if none of the above invalidity conditions are satisfied, the order is valid.
	return 1;
}

int main() {
    int waiter_id;
    int table_number;

    printf("Enter Waiter ID: ");
    scanf("%d", &waiter_id);

    printf("Waiter ID: %d\n", waiter_id);

    table_number = waiter_id;

    int shm_key = SHARED_MEMORY_KEY_BASE + table_number;
    int manager_shm_key = MANAGER_SHM_KEY_BASE + table_number;

    int shm_id = shmget(shm_key, sizeof(TableData), 0666);
    if (shm_id == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    TableData *shared_order = (TableData *)shmat(shm_id, NULL, 0);
    if (shared_order == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    FILE *file = fopen("menu.txt", "r"); //intialising a read type file pointer.
    if (file == NULL) {
        printf("Error opening file.\n");
        return 1;
    }
    int prices[10];
    // Read the menu items and prices
    char line[100]; // Assuming each line has a maximum of 100 characters
    int index = 1;
    int totalBill = 0; //initialising the totalbill to 0
    while (fgets(line, sizeof(line), file)) {
            // Find the position of INR in the line
            char *ptr = strstr(line, " INR");
            if (ptr != NULL) {
                // Extract the price substring
                *ptr = '\0'; // Terminate the string at the position of INR
                char *priceStr = strrchr(line, ' '); // Find the last space to get the price
                if (priceStr != NULL) {
                    // Convert price string to integer
                    int price = atoi(priceStr + 1); // Skip the space character
                    prices[index] = price;
                    index++;
                }
            }
        }

    int manager_shm_id = shmget(manager_shm_key, sizeof(int), 0666);
    if (manager_shm_id == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    BillData *manager_shared_data= (BillData *)shmat(manager_shm_id, NULL, 0);
    if (manager_shared_data == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    manager_shared_data->is_active=1; //telling hotelmanager process that waiter process is active.
    while(1)
    {
        sleep(20);
        //If we take more than 20 seconds to give our order, we will get an extra output line in the waiter process and in earnings.txt like this "Bill amount for table X: 0 INR". But it won't be displayed in the output of table process, so it won't actually show any critical effect.
    
    while(shared_order->is_picked==1){
        sleep(1);
        if(shared_order->terminate == 1)
    {
        //printf("Terminate request from the table is processed.\n");
        break;
    }
    }
    if(shared_order->terminate == 1)
    {
        printf("Terminate request from the table is processed.\n");
        break;
    }
    if(!isValidOrder(shared_order))
    {
    	printf("\nWaiting for a valid order from the table. Please wait for 30 sec\n");
    	sleep(30);
    	continue;
    }

    FILE *file = fopen("menu.txt", "r");
    if (file == NULL) {
        printf("Error opening file.\n");
        return 1;
    }
    // Read the menu items and prices
    char line[100]; // Assuming each line has a maximum of 100 characters
    int index = 1;
    int totalBill = 0;
    //multiplying the frequency of order with price of each item and then adding them to calculate the total bill.
    for(int i = 1;i<=shared_order->max_item_no;i++)
    {
        if(shared_order->all_orders[i] != 0)
        {
            totalBill += prices[i]*shared_order->all_orders[i];
        }
    }
    
    printf("Bill Amount for Table %d: %d INR\n", waiter_id, totalBill);
    shared_order->totalbill = totalBill;
    manager_shared_data->bill_amount = totalBill;
    manager_shared_data->just_now_updated = 1;
    shared_order->is_picked = 1;
    
    fclose(file); //close the file pointer
    }
    manager_shared_data->is_active=0;
    shmdt(manager_shared_data); //detach the shared memory between waiter and hotelmanager processes.
    shmdt(shared_order); //detach the shared memory between table and waiter processes.

    return 0;
}