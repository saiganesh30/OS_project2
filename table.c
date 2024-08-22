#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_CUSTOMERS 5
#define SHARED_MEMORY_KEY_BASE 2000

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

int main() {
        int table_number;
        int num_customers;

        printf("Enter Table Number: "); //taking table number 
        scanf("%d", &table_number);

        int shm_key = SHARED_MEMORY_KEY_BASE + table_number;  // generating key for shared memory

        int shm_id = shmget(shm_key, sizeof(TableData), 0666 | IPC_CREAT); // generating shared memory id
        if (shm_id == -1) {
            perror("shmget");
            exit(EXIT_FAILURE);
        }

        TableData *shared_data = (TableData *)shmat(shm_id, NULL, 0); //attaching pointer to shared memory
        if (shared_data == (void *)-1) {
            perror("shmat");
            exit(EXIT_FAILURE);
        }
	
        shared_data->table_number = table_number; //saving table number in the shared memory
        memset(shared_data->all_orders,0,10*sizeof(int));
        shared_data->valid_order = 1; // assume all orders are initially valid
    while(1)
    {
        
        printf("Enter Number of Customers at Table (maximum no. of customers can be 5): ");//taking number of custmer that are going to be seated
        scanf("%d", &num_customers);

        shared_data->num_customers = num_customers; //saving the number of coustomers in the shared memory
        
        if(num_customers == -1) //if the number of customers is -1 terminate this process
        {
        printf("\nCommunication sent to waiter to terminate and Terminating this table\n");
        shared_data->terminate = 1;
        break;
        }

        if (num_customers < 1 || num_customers > MAX_CUSTOMERS) { //if the num of customers is in the valid range, contiue...
            printf("Invalid number of customers. Must be between 1 and 5. Please re-enter\n");
            continue;
        }
        FILE *menu_file = fopen("menu.txt", "r"); //intialising a read type file pointer.

    	char item[100];
    	int max_item_no=0;
    	while (fgets(item, sizeof(item), menu_file) != NULL) //iterating through all the items in the menu file.
    	{
		printf("%s", item);
		max_item_no++; //to calculate the total number of items/max item number.
    	}
    	shared_data->max_item_no = max_item_no; //putting this max item no into the shared memory with waiter process as it needs to check if the order is valid.
    	fclose(menu_file); //closing the file pointer.
	
        int pipes[num_customers][2]; //descriptor to be used in pipes.
        //initialising two arrays to store the collective order in terms of frequency, i.e., element of the array with index 1 is the number of item number 1 ordered, and so on...
        int cust_order[10]; //cust_order is the array storing at the ith index the number or frequency of the ith item ordered by each customer at that table.
        int all_orders[10]; //all_orders is the array storing at the ith index the number or frequency of the ith item ordered collectively by all the customers at that table.
        int num = 1;
        memset(all_orders,0,10*sizeof(int)); //initialising all elemets of the array all_orders to 0.
        for (int i = 0; i < num_customers; ++i) {
            int ind_orders[10] = {0}; //ind_orders meaning individual orders storing individual customer's order temporarily inside the pipe.
            //creation of pipe.
            if (pipe(pipes[i]) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
            pid_t pid = fork();
            if (pid == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
            } else if (pid == 0) { // Child process
                int temp;
                int order = 0;
                printf("Please enter the orders for customer no. %d:\n", num);
                while(1)
                {
                    scanf(" %d", &temp);
                    if(temp<-1 || temp>max_item_no) //putting the number of invalid orders in 0th index element of the orders array to be checked in the waiter process.
                    {
                        ind_orders[0]++;
                    continue;
                    }
                    else if(temp==-1) //when we enter -1 in the orders it should go to next customer.
                    {
                        break;	
                    }
                    ind_orders[temp]++; //increment of the frequency of temp'th item in the order.
                }
                close(pipes[i][0]); // Close read end of the pipe
                // Send order to the customer process
                write(pipes[i][1], &ind_orders, sizeof(ind_orders));
                close(pipes[i][1]); // Close write end of the pipe
                exit(EXIT_SUCCESS);
            } else { // Parent process
                num++;
                close(pipes[i][1]); // Close write end of the pipe
                wait(NULL); // Wait for child process to finish
                read(pipes[i][0],&cust_order,sizeof(cust_order));
                for(int i = 0;i<10;i++) 
                {
                    all_orders[i] += cust_order[i]; //accumulating the individual order arrays.
                }
                close(pipes[i][0]); // Close read end of the pipe
            }
        }
        for(int i = 0;i<10;i++)
        {
            shared_data->all_orders[i] = all_orders[i]; //updating the collective order array with accumulated individual order array.
        }
        shared_data->is_picked = 0; //this variable is initialised to 0, it is going to be used by waiter process to check if an order is already picked or not.
        sleep(20);
        if(shared_data->valid_order == 0) //case when waiter process tells the table process that the given order is invalid and now taking the order again.
        {
            printf("Looks like some of the items that were placed turned out be invalid\n");
            printf("Starting the program again...\n");
        	continue;
        }
        printf("Now displaying the total amount for the table..."); //displaying the total bill amount calculated by waiter process
        printf("The total bill amount is : %d INR\n", shared_data->totalbill);
    }
    shmdt(shared_data); //detach the shared memory between table and waiter processes.
    shmctl(shm_id,IPC_RMID,NULL); //delete the shared memory.
        return 0;
}