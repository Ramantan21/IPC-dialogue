#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <poll.h>
#include <defines.h>
#include <dialogue.h>
#include <shared_memory.h>
#include <message.h>
#include <sys/mman.h> //for shm open etc..
volatile __sig_atomic_t terminate = 0;
int my_idx = 0;//global for thread access
int my_dialogid = 1; //same
void* reader(void* args);
void* writer(void* arg); 
int create_new_dialog(shared_memory *shm, int max_participants);
int join_dialog(shared_memory *shm, int dialog_id);
void show_available_dialogs(shared_memory *shm);
int get_participant_count(shared_memory *shm, int dialog_id);
void cleanup(shared_memory *shm, int idx, int dialogid);

int main(){
    shared_memory *shm_ptr;
    int shm_fd;
    bool rest_processes = false;
    bool first_process = false;
    pthread_t reader_id;//thread id
    pthread_t writer_id;

    /*
        logic around this is that i keep the first process flag and the rest_processes flag so 
        the rest_processes won't try to create the shared memory while it's already created by the first one
        and also so the first process can lock the mutex without any other processes try to unlock it 

    */
    if((shm_fd = shm_open(SHM_SEMS,O_RDWR | O_CREAT | O_EXCL,0666)) == -1){
        if(errno != EEXIST) return -1;
        rest_processes = true;
    }else{
        first_process = true;
    }
    if(rest_processes){
        if((shm_fd = shm_open(SHM_SEMS,O_RDWR,0666)) == -1){
            perror("shm_open failed on other process");
            return -1;
        }
    }
    if(first_process){
        if(ftruncate(shm_fd,sizeof(shared_memory)) == -1){
        perror("ftruncate failed\n");
        close(shm_fd);
        shm_unlink(SHM_SEMS);
        return -1;
    }
    }
    //ptr now will be mapped in the shared memory
    if((shm_ptr = mmap(NULL,sizeof(shared_memory),PROT_READ |  PROT_WRITE, MAP_SHARED, shm_fd,0)) == MAP_FAILED){
        perror("Mmap failed check line 29..\n");
        return -1;
    }
    /*
    initialize the semaphore with pshared = 1 to be shared between processes
    with value 1 to be unclocked
    */
    if(first_process){
        sem_init(&shm_ptr->sem1,1,1);
        shm_ptr->active_dialog_count = 0;
        shm_ptr->process_count = 0;
        shm_ptr->message_count = 0;
        shm_ptr->message_head = 0; 
        shm_ptr->next_dialog_id = 1;
        memset(shm_ptr->messages, 0, sizeof(shm_ptr->messages));
        memset(shm_ptr->process_pids, 0, sizeof(shm_ptr->process_pids));
        memset(shm_ptr->dialogs, 0, sizeof(shm_ptr->dialogs));
    }
    sem_wait(&shm_ptr->sem1);
    for(int i = 0; i < MAX_PROCS; i++){
        if(shm_ptr->process_pids[i] == 0){
            shm_ptr->process_pids[i] = getpid();
            my_idx = i;
            shm_ptr->process_count++;
            break;
        }
    }  
    sem_post(&shm_ptr->sem1);
    printf("Dialog selection: \n");
    printf("Enter dialog Id or 0 to create a new one\n");
    show_available_dialogs(shm_ptr);
    int choise;
    scanf("%d",&choise);
    getchar();
    if(choise == 0){
        printf("Max participants:\n"); //enter max participants that the dialog will hold
        int max;
        scanf("%d",&max);
        getchar();
        my_dialogid = create_new_dialog(shm_ptr,max);
        if(my_dialogid == -1){
            printf("Error in creating new dialog..\n");
            return -1;
        }
        printf("Created a new dialog\n"); //debug
    }else{
        int result = join_dialog(shm_ptr,choise);
        if(result == 0){
            my_dialogid = choise;
            printf("Joined dialog : %d\n",my_dialogid);
        }else if(result == -2){
            printf("Dialog %d is full\n",choise);
            return -1;
        }else{
            printf("Dialog not found\n");
            return -1;
        }
    }
    pthread_create(&writer_id, NULL, writer,(void*)shm_ptr);
    pthread_create(&reader_id,NULL,reader,(void *)shm_ptr);
    pthread_join(writer_id,NULL);
    pthread_join(reader_id,NULL);
    
    cleanup(shm_ptr, my_idx, my_dialogid);

    munmap(shm_ptr,sizeof(shared_memory));
    close(shm_fd);
    return 0;
}
/*
    writer thread gets the input from the user stores it in shm so all the processes that are contained in the dialog
    can fetch it from it.Also initialises the msg struct to be active,get's the dialog id from the global my_dialogid
    and treats the message[pos] like a circullar buffer.When the user sends "Terminate" the writer thread first stores
    the message in the buffer so the other processes also read it and then they all terminate..
    use of pollfd so the fgets doesn't stay blocked while waiting input from the user
*/
void* writer(void* arg){
    char user_input[TEXT_SZ];
    shared_memory *shm = (shared_memory*)arg;
    while(!terminate){
        fflush(stdout);
        struct pollfd pfd = { .fd = STDIN_FILENO, .events =POLLIN}; 
        int ready = poll(&pfd,1,250);
        if(ready == -1){
            if(errno == EINTR) continue;
            perror("poll");
            break;
        }
        if(ready == 0) continue;
        if(!(pfd.revents & POLLIN)) continue;

        if(!fgets(user_input,TEXT_SZ,stdin)) break;
        user_input[strcspn(user_input,"\n")] = '\0';
        
        bool is_terminate = (strcmp(user_input,"terminate") == 0);
        Message new_msg;
        memset(&new_msg, 0, sizeof(Message));
        new_msg.is_active = true;
        new_msg.dialog_id = my_dialogid;
        new_msg.sender_id = getpid();
        strcpy(new_msg.payload,user_input);
        sem_wait(&shm->sem1); // lock the buffer then write
        int pos = shm->message_head;
        shm->messages[pos] = new_msg;
        shm->message_head = (shm->message_head + 1) % MAX_MESSAGES;
        shm->message_count++;
        sem_post(&shm->sem1); //now unlock
        if(is_terminate){
            terminate=1;
            break;
        }
        fflush(stdout);   
    }
    return NULL;
}


/*
    Reader thread fetches the message from the shared memory.First it locks the semaphore contained in the shm struct
    is the message is not active it skips it,if is already read by the process then it's skipped and also
    if the msg dialog != than my_dialogid also skips it,When it reads terminate it first reads the msg gets all the info it needs
    then it sets the flag to 1.
*/
void* reader(void* args){
    shared_memory *shm = (shared_memory*)args;
    while(!terminate){
        sem_wait(&shm->sem1);
        for(int i = 0; i < MAX_MESSAGES; i++){
            Message *msg = &shm->messages[i];
                if(!msg->is_active) continue; // skip if it's inactive
                if(msg->read_by[my_idx]) continue;// skip if u already read it
                if(msg->sender_id == getpid()) continue;
                if (msg->dialog_id != my_dialogid) continue; 
                fflush(stdout);
                bool is_terminate = (strcmp(msg->payload,"terminate") == 0);
                printf("arrived from %d: %s\n",msg->sender_id, msg->payload);
                msg->read_count++;
                msg->read_by[my_idx] = true;
            int participants = get_participant_count(shm, msg->dialog_id);
            if(msg->read_count >= participants) msg->is_active = false;
            if(is_terminate){
                terminate = 1;
                sem_post(&shm->sem1);
                return NULL;
                break;
            }
        }

        sem_post(&shm->sem1);
        usleep(100000);
        
    }
    return NULL;
}

/*
    Here i first find an empty slot in the dialog then initialise the new dialog by the new slot i found.
    and return the new id of the dialog

*/

int create_new_dialog(shared_memory *shm,int max_participants){
    sem_wait(&shm->sem1);

    int slot = -1;
    for(int i = 0; i < MAX_DIALOGS; i++){
        if(!shm->dialogs[i].is_active){
            slot = i;
            break;
        }
    }
    if(slot == -1){
        sem_post(&shm->sem1);
        return -1;
    }
    int new_id = shm->next_dialog_id++;

    shm->dialogs[slot].dialog_id = new_id;
    shm->dialogs[slot].is_active = true;
    shm->dialogs[slot].max_participants = max_participants;
    shm->dialogs[slot].participant_count = 1;
    shm->dialogs[slot].participant_ids[0] = getpid();
    shm->active_dialog_count++;

    sem_post(&shm->sem1);

    return new_id;
}
/*
    process here joins an existin dialog,depeneds from 3 things.
    If user types 0 then it created a new dialog.
    If user types the id of the process that is visible to him by the other terminal it then joins his "choise"
    If the dialog that he's trying to enter is full then it returns
*/
int join_dialog(shared_memory *shm,int dialog_id){
    sem_wait(&shm->sem1);
    int slot = -1;
    for(int i = 0; i < MAX_DIALOGS; i++){
        if(shm->dialogs[i].is_active && shm->dialogs[i].dialog_id == dialog_id){
            slot = i;
            break;
        }
    }
    if(slot == -1){
        sem_post(&shm->sem1);
        return -1;
    }
    if(shm->dialogs[slot].participant_count >= shm->dialogs[slot].max_participants){
        sem_post(&shm->sem1);
        return -2;
    }
    int pos = shm->dialogs[slot].participant_count;
    shm->dialogs[slot].participant_ids[pos] = getpid();
    shm->dialogs[slot].participant_count++;

    sem_post(&shm->sem1);

    return 0;
}
/*
    helper function so the "other processes" can see when they get to choose if they want to create or join an existance dialog
    show_avaiable dialogs does what it's name says it does
*/
void show_available_dialogs(shared_memory *shm){
    sem_wait(&shm->sem1);
    printf("Available dialogs:\n");

    bool found = false;
    for(int i = 0; i < MAX_DIALOGS; i++){
        if(shm->dialogs[i].is_active){
            printf("[%d] Dialog: %d (%d/%d participants)\n",
                    shm->dialogs[i].dialog_id,
                    shm->dialogs[i].dialog_id,
                    shm->dialogs[i].participant_count,
                    shm->dialogs[i].max_participants);
            found = true;
        }
    }
    if(!found) printf("No active dialogs found\n");

    sem_post(&shm->sem1);
}
/*
    gets participant_count of the dialog so the rest processes can actually check if they fit in it
*/
int get_participant_count(shared_memory *shm,int dialog_id){
    for(int i = 0; i < MAX_DIALOGS; i++){
        if(shm->dialogs[i].is_active && shm->dialogs[i].dialog_id == dialog_id){
            return shm->dialogs[i].participant_count;
        }
    }
    return 0;
}
/*
    a cleanup process that i call in main after threads_are joined
*/
void cleanup(shared_memory *shm, int idx, int dialogid){
    sem_wait(&shm->sem1);
    shm->process_pids[idx] = 0;
    shm->process_count--;

        //remove from dialog participants
    for (int i = 0; i < MAX_DIALOGS; i++) {
        if (shm->dialogs[i].is_active && 
            shm->dialogs[i].dialog_id == dialogid) {
            
            //find my PID in participants
            pid_t my_pid = getpid();
            for (int j = 0; j < shm->dialogs[i].participant_count; j++) {
                if (shm->dialogs[i].participant_ids[j] == my_pid) {
                    //shift remaining participants left
                    for (int k = j; k < shm->dialogs[i].participant_count - 1; k++) {
                        shm->dialogs[i].participant_ids[k] = 
                            shm->dialogs[i].participant_ids[k + 1];
                    }
                    shm->dialogs[i].participant_count--;
                    
                    //if dialog empty, deactivate it
                    if (shm->dialogs[i].participant_count == 0) {
                        shm->dialogs[i].is_active = false;
                        shm->active_dialog_count--;
                    }
                    break;
                }
            }
            break;
        }
    }
    //check if last process
    int remaining = shm->process_count;
    
    if (remaining == 0) {
        sem_post(&shm->sem1);
        sem_destroy(&shm->sem1);
        shm_unlink(SHM_SEMS);
    } else {
        sem_post(&shm->sem1);
    }
}
