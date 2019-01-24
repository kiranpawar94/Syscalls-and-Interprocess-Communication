/*CS1550 Introduction to Operating Systems
Project 2: Trafficsim
*
*Author: Kiran Pawar
*
*Usage: Simulate traffic using synchronization while avoiding deadlocks
*
*/
#include <linux/unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct proc_list{               //Linkedlist to store procs
        struct proc_list *next;         //Points to next
        struct task_struct *proc_task;  //Task struct of the current proc
};

//Structure to store proc semaphore
struct cs1550_sem{
        int value;              //Locking mechanism
        //Queue
        struct proc_list *head;
        struct proc_list *tail;
};

//Structure to store car queue information
struct cs1550_carsem{
	int val;
	//Queue
	int car_count;		//Number of cars in the queue
	int car_queue[10];	//Array of cars
	char direction;		//'N' for north and 'S' for south	
	int car_num;
};

//Global Variables
//Pointers for the north and south queues
struct cs1550_carsem *north_ptr;	//Points to the north queue
struct cs1550_carsem *south_ptr;	//Points to the south queue
struct cs1550_carsem *ptr;		//Temporary pointer
struct cs1550_sem *sem_ptr;		//Points to proc list semaphore
void *map_ptr;				//Map pointer for mmap
int i;				//Iterator variable

//Up and down
void up(struct cs1550_sem *sem){
	syscall( __NR_cs1550_up, sem);
}

void down(struct cs1550_sem *sem){
	syscall( __NR_cs1550_down, sem);
}

//Queue car functions (add and remove)

void insert_car(struct cs1550_carsem *ptr){
	while( ptr->val > 0); 	//Check if consumer is working
	ptr->val++;		//Lock section
        if(ptr->car_count < 10){	//Check if any more cars can fit in the queue
		time_t t= time(NULL);	//Time variable to tell arrival/leave times of cars
		struct tm tm = *localtime(&t);	//Adjusts time variable to local time
		printf("Car %d coming in from the %c direction arrived in queue at time %d:%d\n", ptr->car_num, ptr->direction, tm.tm_min, tm.tm_sec);
                ptr->car_queue[ptr->car_count] = ptr->car_num;   //Insert car at offset of car_count
                ptr->car_count++;	//Increment car count in the queue
		ptr->car_num++;		//Increase car number
        }
	ptr->val--;			//Release lock

}

void remove_car(struct cs1550_carsem *ptr){
	while(ptr->val > 0);		//Check if producer is working
	ptr->val++;			//Lock section
  
	if (ptr->car_count > 0){	//Check if there are cars in queue to be moved
                for(i = 0; i < ptr->car_count; i++){	//Move as many cars as there are in the queue
			time_t t= time(NULL);		//Time variable
			struct tm tm = *localtime(&t);	//Get local time
	        	printf("Car %d from %c direction left the construction zone at time %d:%d\n", ptr->car_queue[i], ptr->direction, tm.tm_min, tm.tm_sec);
           	     	sleep(2);	//Time to pass a car: 2 seconds
                }
        ptr->car_count = 0;             //All car removed from queue
        }
	ptr->val--;			//Release lock
}


//Initilization
struct cs1550_carsem *init_list(char direction){
        map_ptr = mmap(NULL, sizeof(struct cs1550_carsem)*10 , PROT_READ | PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0); //Shared memory
        ptr = (struct cs1550_carsem *)map_ptr;	//Set pointer to the beginning of th$
        ptr->direction = direction;		//Pass either 'N' or 'S' only
        ptr->val = 0;				//Semaphore value (0 by default)
        ptr->car_count = 0;			//Number of cars in a queue
	ptr->car_num = 1;			//Start with car #1
        return map_ptr;				//Return beginning of shared memory
}

//Initialize proc list semaphore
struct cs1550_sem *init_sem(){

        map_ptr = mmap(NULL, sizeof(struct cs1550_sem), PROT_READ | PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);	//Shared memory for semaphores
        sem_ptr = (struct cs1550_sem *)map_ptr;	//Set pointer to the beginning of shared memory
	sem_ptr->head = NULL;			//Head of list
	sem_ptr->tail = NULL;			//Tail of list
        sem_ptr->value = 0;			//Semaphore value initialized to 0
        return map_ptr;				//Return map pointer
}


//Main
int main(){

	north_ptr = init_list('N');	//Initialize north car queue
	south_ptr = init_list('S');	//Initialize south car queue
	sem_ptr = init_sem();		//Initialize proc list semaphore


	//Procceses for the north and south queues
	pid_t north_proc = fork();	//Fork the north process
	if(north_proc < 0){		//Error forking north proc
		printf("Error forking north process\n");
		return 0;
	}

	pid_t south_proc = fork();	//Fork the south process
	if(south_proc < 0){		//Error forking south proc
		printf("Error forking north process\n");
		return 0;
	}


	//Child processes (Producers)

//---------------NORTH QUEUE
	if(north_proc == 0){	//Begin north proc operations
		//printf("In north process\n");
		while(1){
			//Keep inserting cars
			up(sem_ptr);	//Protect crit area
			insert_car(north_ptr);
			down(sem_ptr);	//End crit section

			if(rand()%10<8){}		//Coin flip to determine if there's a car next in line
			else{
				sleep(20);	//No more cars, so sleep for 20s
			}
		}
	}

//---------------SOUTH QUEUE
	if(south_proc == 0){	//Begin south proc operations
		//printf("In south process\n");
		while(1){
			up(sem_ptr);	//Protect crit section
			insert_car(south_ptr);
			down(sem_ptr);	//Release crit section
			if(rand()%10<8){}		//Coin flip to determine if there's a car next in line

			//The following runs if no car in next in queue
			else{
				sleep(20);	//No more cars, so sleep for 20s
			}
		}
	} 

	//Parent process (Consumer)

	while(1){

		if(north_ptr->car_count == 0 && south_ptr->car_count == 0){	//If there are no cars in either queue
			printf("The flagman is now asleep\n");
                   	while (north_ptr->car_count == 0 && south_ptr->car_count == 0);	//Don't consumer while there is nothing in either queue
			up(sem_ptr);

			if(north_ptr->car_count > 0){	//When the north car is present but flagman is asleep
				printf("Car %d from the North direction blew their horn at time %d\n", north_ptr->car_queue[0], 0);
				printf("The flagman is now awake\n");
			}

			else if(south_ptr->car_count > 0){	//When south car is present but flagman is asleep
				printf("Car %d from the South direction blew their horn at time %d\n", south_ptr->car_queue[0], 0);
				printf("The flagman is now awake\n");
			}
			down(sem_ptr);


		}

		while(1){		//Allow north traffic to pass through
                        up(sem_ptr);
                        remove_car(north_ptr);
                        down(sem_ptr);
                        if(south_ptr->car_count >= 10){	//If south car count is >=10
				break;			//Switch to south queue
			}
		}

		while(1){		//Allow south traffic to pass through
   	                up(sem_ptr);
                        remove_car(south_ptr);
                        down(sem_ptr);
			if(north_ptr->car_count >= 10){	//If north car count is >=10
				break;			//Switch to north queue
			}
                }
	}


	//End of main
	return 0;
}
