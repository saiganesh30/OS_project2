#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHM_KEY 12345  // Change this key as needed

// Structure to store shared data
struct SharedData {
    int termination_flag;  // 'Y' for termination, 'N' for normal operation
} AdminData;

int main() {
    // Create or open the shared memory segment
    int shm_id = shmget(SHM_KEY, sizeof(struct SharedData), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("Error creating/opening shared memory");
        exit(EXIT_FAILURE);
    }

    // Attach the shared memory segment to the process
    struct SharedData *shared_data = (struct SharedData *)shmat(shm_id, NULL, 0);
    if ((void *)shared_data == (void *)(-1)) {
        perror("Error attaching shared memory");
        exit(EXIT_FAILURE);
    }
    shared_data->termination_flag = 0;
    // Admin process main loop
    while (1) {
        // Display menu and get user input
        printf("Do you want to close the hotel? Enter Y for Yes and N for No: ");
        char choice;
        scanf(" %c", &choice);

        // Update shared memory based on user input
        if (choice == 'Y' || choice == 'y') {
            shared_data->termination_flag = 1;
            printf("Hotel closure request sent to the hotel manager.\n");
            break;  // Exit the loop and terminate the admin process
        } else if (choice == 'N' || choice == 'n') {
            shared_data->termination_flag = 0;
            printf("Hotel will continue to run.\n");//if "n" is chosen then it is not terminated
        } else {
            printf("Invalid input. Please enter Y or N.\n");//wrong inputs recevied  
        }

        sleep(1);  // Optional: Add a short delay between menu prompts
    }
	sleep(5);
    // Detach the shared memory segment from the process
    if (shmdt((void *)shared_data) == -1) {
        perror("Error detaching shared memory");
        exit(EXIT_FAILURE);
    }
    shmctl(shm_id,IPC_RMID,NULL);//delelting the shared memory

    return 0;
}
