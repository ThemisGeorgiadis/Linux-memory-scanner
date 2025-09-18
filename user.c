/********************************************/
/*                                          */
/*    Linux Memory Scanner User Program     */
/*    @author Themistoklis Georgiadis       */
/*                                          */
/********************************************/


#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#define DEVICE_PATH "/dev/ReadProcessVAs"
#define MAGIC 'x'
#define SINGLE_READWRITE _IOWR(MAGIC, 1, struct instructionPkg *)
#define EMPTY_LIST _IOWR(MAGIC, 2, int)
#define INIT_SEARCH _IOWR(MAGIC, 3, struct search_struct *)
#define SEARCH_CONT _IOWR(MAGIC, 4, struct search_struct *)
#define WRITE_LIST _IOWR(MAGIC, 5, struct search_struct *)
#define EXTRACT _IOWR(MAGIC, 6, struct list_transfer_struct *)
#define TRANSFER _IOWR(MAGIC, 7, struct list_transfer_struct *)
#define REFRESH_VAL _IOWR(MAGIC, 8, int)
#define LOCK_DRIVER _IOWR(MAGIC, 9, int)
#define UNLOCK_DRIVER _IOWR(MAGIC, 10, int)
#define BYTE 8;


typedef struct addr_struct{
    unsigned long address;
    void* value;
    char type;
}addr_struct;

typedef struct transfer_node{
    unsigned long address;
    char buffer[256];
    char type;
}transfer_node;

typedef struct list_transfer_struct{
    int transfersLeft , currentTransfers;
    int size;
    int callerPid;
    transfer_node T_node[100];
}list_transfer_struct;

typedef struct list{
    addr_struct *add_str;
    struct list *next;
}list;

typedef struct instructionPkg{
    char processName[256];
    unsigned long vaddr;
    int size;
    int callerPid;
    int read;
    int write;

    int state; //1 for successful operation and 0 for unsuccessful
    int pid;
    void* value;
} instructionPkg;

typedef struct search_struct{
    char processName[256];
    char string[256];
    void* value;
    int size;
    char type;
    int callerPid;
    addr_struct addrSt;

    int state; //1 for successful operation and 0 for unsuccessful

}search_struct;

void ProgressBar(long start , long end , long current , char* msg){

    long total = end - start;
    long progress = current - start;
    static int last = -1;

    if (total > 0) {
        double percentage = ((double)progress / (double)total) * 100.0;
        if( (int)percentage != last){
            printf("\r%s %i%%",msg, (int)percentage);  
            fflush(stdout);
            last = (int)percentage;
        }

    }
}

void handle_sig(int sig){
    system("clear");
    int pid = getpid();
    exit(0);
}

list *MainListRoot;
int listLength =0;
int String_size;
int PID;
 

int lockDriver(){
    int deviceFile = open(DEVICE_PATH, O_RDWR);
    if (deviceFile < 0) {
        printf("Failed to open device file\n");
        return 0;
    }

    if (ioctl(deviceFile, LOCK_DRIVER, &PID) == -1) {
            printf("ERROR\n");
            close(deviceFile);
            return 0;
        }

}

int unlockDriver(){
    int deviceFile = open(DEVICE_PATH, O_RDWR);
    if (deviceFile < 0) {
        printf("Failed to open device file\n");
        return 0;
    }

    if (ioctl(deviceFile, UNLOCK_DRIVER, &PID) == -1) {
            printf("ERROR\n");
            close(deviceFile);
            return 0;
        }

}


void emptylist(list** root){
    list* curr = *root;
    list* tmpNext;
    while (curr) {
        tmpNext = curr->next;

        if (curr->add_str) {
            if (curr->add_str->value) {
                free(curr->add_str->value);
                curr->add_str->value = NULL;
            }
            free(curr->add_str);
            curr->add_str = NULL;
        }
        free(curr);
        curr = tmpNext;
    }
    *root = NULL;
    listLength = 0;
}

void addtolist(list** root , addr_struct* addr){
   
    if(*root != NULL){
     list* newnode = malloc(sizeof(list));
     newnode->next = *root;
     newnode->add_str = addr;
     *root = newnode;
    }else{
     
     *root = malloc(sizeof(list));
     (*root)->add_str = addr;
     (*root)->next = NULL;
     
    }
    listLength++;
}

list* removefromlist(list** root, unsigned long addr) {
    if (*root == NULL){
        listLength = 0;
        return NULL;
    }

    list* currenT = *root;
    list* prev = NULL;

    while (currenT != NULL && currenT->add_str->address != addr) {
        prev = currenT;
        currenT = currenT->next;
    }

    if (currenT == NULL){
        listLength = 0;
        return NULL;
    }

    if (prev == NULL) {
        
        *root = currenT->next;
    } else {
        
        prev->next = currenT->next;
    }
    listLength--;
    free(currenT);
    return *root;
}

int sendListToKernel(char type){
    list_transfer_struct *lts = malloc(sizeof(list_transfer_struct));
    lts->callerPid = getpid();
    int deviceFile = open(DEVICE_PATH, O_RDWR);
    if (deviceFile < 0) {
        printf("Failed to open device file\n");
        return 0;
    }
    if(type == 'i')lts->size = 4;
    if(type == 'l')lts->size = 8;
    if(type == 's')lts->size = String_size;

    lts->transfersLeft = listLength;
    while(listLength > 0){
        lts->currentTransfers=0;
        
        for(int i=0; i<100 && lts->transfersLeft > 0; i++){
            lts->transfersLeft--;
            lts->T_node[i].address = MainListRoot->add_str->address;
            lts->T_node[i].type = type; 

            switch (type)
            {
            case 'i':
                *(int*)lts->T_node[i].buffer = *(int*)MainListRoot->add_str->value;
                break;
            
            case 'l':
                *(long*)lts->T_node[i].buffer = *(long*)MainListRoot->add_str->value;
                break;

            case 's':
                strcpy(lts->T_node[i].buffer , (char*)MainListRoot->add_str->value);
                break;
            }
            //printf("transfers %i\n",lts->transfersLeft);
            removefromlist(&MainListRoot,lts->T_node[i].address);
            lts->currentTransfers++;

        }
        
        
        if (ioctl(deviceFile, TRANSFER, lts) == -1) {
            printf("ERROR\n");
            close(deviceFile);
            return 0;
        }
    }

}

int extractList(char type){

    list_transfer_struct *lts = malloc(sizeof(list_transfer_struct));
    if(type == 'i')lts->size = 4;
    if(type == 'l')lts->size = 8;
    if(type == 's')lts->size = String_size;
    lts->transfersLeft = 1;
    lts->callerPid = getpid();
    int deviceFile = open(DEVICE_PATH, O_RDWR);
    if (deviceFile < 0) {
        printf("Failed to open device file\n");
        return 0;
    }

    while(lts->transfersLeft > 0){

        if (ioctl(deviceFile, EXTRACT, lts) == -1) {
            printf("ERROR\n");
            close(deviceFile);
            return 0;
        }
        //printf("transfers left %i , current transfers %i\n",lts->transfersLeft , lts->currentTransfers);
        for(int i=0; i<lts->currentTransfers; i++){

            addr_struct *TA = malloc(sizeof(addr_struct));
            TA->address = lts->T_node[i].address;
            TA->value = malloc(lts->size);
            switch (type)
            {
            case 'i':
                TA->value = malloc(sizeof(int));
                memcpy(TA->value, lts->T_node[i].buffer, sizeof(int));
                break;
            case 'l':
                TA->value = malloc(sizeof(long));
                memcpy(TA->value, lts->T_node[i].buffer, sizeof(long));
                break;
            case 's':
                
                TA->value = malloc(String_size);
                strcpy((char*)TA->value,(char*)lts->T_node[i].buffer);
                break;
            }
            addtolist(&MainListRoot,TA);
            //printf("Address %lx value %i\n",TA->address , (int)TA->value);
            
        }
    }
    
    free(lts);

    close(deviceFile);
}


int readProcessMemory(char* targetPname , unsigned long targetVAddr ,int size ,void* rtrnPtr){

    int deviceFile = open(DEVICE_PATH, O_RDWR);
    if (deviceFile < 0) {
        printf("Failed to open device file\n");
        return 0;
    }

    instructionPkg data;
    strncpy(data.processName, targetPname, sizeof(data.processName) - 1);
    data.processName[sizeof(data.processName) - 1] = '\0';
    data.vaddr = targetVAddr;
    data.size = size;
    data.value = rtrnPtr;
    data.callerPid = getpid();
    data.read = 1;
    data.write = 0;

    if (ioctl(deviceFile, SINGLE_READWRITE, &data) == -1) {
        printf("ERROR\n");
        close(deviceFile);
        return 0;
    }

    close(deviceFile);

    if(data.state){
        //printf("SUCCESS\n");
        return 1;
    }else{
        //printf("FAIL\n");
        return 0;
    }
    

}

int writeProcessMemory(char* targetPname , unsigned long targetVAddr ,int size ,void* dataToWrite){

    int deviceFile = open(DEVICE_PATH, O_RDWR);
    if (deviceFile < 0) {
        printf("Failed to open device file\n");
        return 0;
    }

    instructionPkg data;
    strncpy(data.processName, targetPname, sizeof(data.processName) - 1);
    data.processName[sizeof(data.processName) - 1] = '\0';
    data.vaddr = targetVAddr;
    data.size = size;
    data.value = malloc(size);
    memcpy(&data.value, dataToWrite, size);
    data.callerPid = getpid();
    data.read = 0;
    data.write = 1;

    if (ioctl(deviceFile, SINGLE_READWRITE, &data) == -1) {
        printf("ERROR\n");
        close(deviceFile);
        return 0;
    }

    close(deviceFile);

    if(data.state){
        //printf("SUCCESS\n");
        return 1;
    }else{
        //printf("FAIL\n");
        return 0;
    }

}


void emptyKernList(){
    int deviceFile = open(DEVICE_PATH, O_RDWR);
    if (deviceFile < 0) {
        printf("Failed to open device file\n");
        return ;
    }
    
    int state = 4;

    if (ioctl(deviceFile, EMPTY_LIST, &state) == -1) {
        printf("ERROR\n");
        close(deviceFile);
        return ;
    }
    close(deviceFile);
}

int Kernel_Init_search(char* name , void* value , char type){
    search_struct sr;
    int deviceFile = open(DEVICE_PATH, O_RDWR);
    if (deviceFile < 0) {
        printf("Failed to open device file\n");
        return 0;
    }


    switch(type){

        case 'i':
        memcpy(&sr.value , value , sizeof(int));
        sr.type = 'i';
        sr.size = sizeof(int);
        break;

        case 'l':
        memcpy(&sr.value , value , sizeof(long));
        sr.type = 'l';
        sr.size = sizeof(long);
        break;

        case 's':
        sr.size = strlen((char*)value) + 1;
        String_size = sr.size;
        sr.value = malloc(sr.size);
        sr.string[sr.size-1] = '\0';
        strcpy(sr.string , value);
        sr.type = 's';
        break;

    }

    sr.callerPid = getpid();
   
    strncpy(sr.processName, name, sizeof(sr.processName) - 1);
    sr.processName[sizeof(sr.processName) - 1] = '\0';

    
    if (ioctl(deviceFile, INIT_SEARCH, &sr) == -1) {
        printf("ERROR\n");
        close(deviceFile);
        return 0;
    }
    

    close(deviceFile);

    if(sr.state){
        //printf("SUCCESS\n");
        return 1;
    }else{
        //printf("FAIL\n");
        return 0;
    }

}

int Kernel_Cont_search(char* name , void* value, char type){
    search_struct sr;
    int deviceFile = open(DEVICE_PATH, O_RDWR);
    if (deviceFile < 0) {
        printf("Failed to open device file\n");
        return 0;
    }

    switch(type){

        case 'i':
        memcpy(&sr.value , value , sizeof(int));
        sr.type = 'i';
        sr.size = sizeof(int);
        break;

        case 'l':
        memcpy(&sr.value , value , sizeof(long));
        sr.type = 'l';
        sr.size = sizeof(long);
        break;

        case 's':
        sr.size = strlen((char*)value) + 1;
        sr.value = malloc(sr.size);
        sr.string[sr.size-1] = '\0';
        strcpy(sr.string , value);
        sr.type = 's';
        break;

    }

    sr.callerPid = getpid();
    
    strncpy(sr.processName, name, sizeof(sr.processName) - 1);
    sr.processName[sizeof(sr.processName) - 1] = '\0';
    


    if (ioctl(deviceFile, SEARCH_CONT, &sr) == -1) {
        printf("ERROR\n");
        close(deviceFile);
        return 0;
    }

    close(deviceFile);

    if(sr.state){
        //printf("SUCCESS\n");
        return 1;
    }else{
        //printf("FAIL\n");
        return 0;
    }
}

int writeKernelList(char* name , void* value, char type){
    search_struct sr;
    strncpy(sr.processName, name, sizeof(sr.processName) - 1);
    sr.processName[sizeof(sr.processName) - 1] = '\0';

    switch(type){

        case 'i':
        memcpy(&sr.value , value , sizeof(int));
        sr.type = 'i';
        sr.size = sizeof(int);
        break;

        case 'l':
        memcpy(&sr.value , value , sizeof(long));
        sr.type = 'l';
        sr.size = sizeof(long);
        break;

        case 's':
        sr.size = strlen((char*)value) + 1;
        sr.value = malloc(sr.size);
        sr.string[sr.size-1] = '\0';
        strcpy(sr.string , value);
        sr.type = 's';
        break;

    }
    sr.callerPid = getpid();
    int deviceFile = open(DEVICE_PATH, O_RDWR);
    if (deviceFile < 0) {
        printf("Failed to open device file\n");
        return 0;
    }

    if (ioctl(deviceFile, WRITE_LIST, &sr) == -1) {
        printf("ERROR\n");
        close(deviceFile);
        return 0;
    }

    close(deviceFile);

    if(sr.state){
        //printf("SUCCESS\n");
        return 1;
    }else{
        //printf("FAIL\n");
        return 0;
    }
}
enum state {CLEAR , INIT_SR , CONT_SR , WRITE_LIST_ALL ,READONCE, WRITEONCE, DEFAULT, PRINTLIST, SEND_TO_KERNEL, REFRESH_VALUES,LOCKDR,UNLOCKDR};

void menu(){
    signal(SIGINT, handle_sig); 
    enum state State = DEFAULT;

    char type = 'x';
    int menuSelection;
    int inputInt;
    long inputLong;
    char inputString[256];
    char processName[256];

    system("clear");
    printf("Enter process name to attach\n\n");
    scanf("%s",processName);

    while(1){
        system("clear");
        switch(State){

            case REFRESH_VALUES:
                if(MainListRoot){
                    sendListToKernel(type);

                    int deviceFile = open(DEVICE_PATH, O_RDWR);
                    if (deviceFile < 0) {
                        printf("Failed to open device file\n");
                        return 0;
                    }
                    int sz;
                    switch(type)
                    {
                    case 'i':
                        sz = 4;
                        break;
                    case 'l':
                        sz = 8;
                        break;
                    case 's':
                        sz = String_size;
                        break;
                    }
                    if (ioctl(deviceFile, REFRESH_VAL, &sz) == -1) {
                        printf("ERROR\n");
                        close(deviceFile);
                        return 0;
                    }

                    extractList(type);
                    State = DEFAULT;
                }
            break;

            case SEND_TO_KERNEL:
                sendListToKernel(type);
                State = DEFAULT;
            break;

            case PRINTLIST:
                list* curr = MainListRoot;
                
                while(curr){
                    switch(type){
                        case 'i':
                        printf("Address: %lx Value %i\n",curr->add_str->address , *(int*)curr->add_str->value );
                        break;
                        case 'l':
                        printf("Address: %lx Value %ld\n",curr->add_str->address , *(long*)curr->add_str->value );
                        break;
                        case 's':
                        printf("Address: %lx Value %s\n",curr->add_str->address , (char*)curr->add_str->value );
                        break;
                    }
                    curr = curr->next;
                }
                while(getchar());

                State = DEFAULT;
            break;

            case CLEAR:
                emptyKernList();
                State = DEFAULT;
            break;

            case INIT_SR:
                emptylist(&MainListRoot);
                lockDriver();
                printf("------------------------------------------------------\n\n");
                printf("        Enter type and value to search\n");
                printf("        sytax: (type) (value)\n");
                printf("        types: i (int), l (long), s (string)\n\n");
                printf("------------------------------------------------------\n");
                scanf(" %c",&type);
                switch(type){
                    case 'i':scanf(" %i",&inputInt);Kernel_Init_search(processName, &inputInt , type);break;
                    case 'l':scanf(" %ld",&inputLong);Kernel_Init_search(processName, &inputLong , type);break;
                    case 's':scanf(" %s",inputString);Kernel_Init_search(processName, &inputString , type);break;
                }
                State = DEFAULT;
                extractList(type);
                unlockDriver();
            break;

            case CONT_SR:
                lockDriver();
                sendListToKernel(type);
                printf("------------------------------------------------------\n\n");
                printf("        Enter value to search\n\n");
                printf("------------------------------------------------------\n");
                
                switch(type){
                    case 'i':scanf(" %i",&inputInt);Kernel_Cont_search(processName, &inputInt , type);break;
                    case 'l':scanf(" %ld",&inputLong);Kernel_Cont_search(processName, &inputLong , type);break;
                    case 's':scanf(" %s",inputString);Kernel_Cont_search(processName, &inputString , type);break;
                }
                State = DEFAULT;
                extractList(type);
                unlockDriver();
            break;

            case WRITE_LIST_ALL:
            lockDriver();
            sendListToKernel(type);
                printf("------------------------------------------------------\n\n");
                printf("        Enter value to Write\n\n");
                printf("------------------------------------------------------\n");

                switch(type){
                    case 'i':scanf(" %i",&inputInt);writeKernelList(processName, &inputInt , type);break;
                    case 'l':scanf(" %ld",&inputLong);writeKernelList(processName, &inputLong , type);break;
                    case 's':scanf(" %s",inputString);writeKernelList(processName, &inputString , type);break;
                }
                State = DEFAULT;
                extractList(type);
                unlockDriver();
            break;

            case READONCE:
                
                printf("------------------------------------------------------\n\n");
                    printf("        Enter type and address to read\n");
                    printf("        sytax: (type) (value)\n");
                    printf("        types: i (int), l (long)\n\n");
                    printf("------------------------------------------------------\n");
                    scanf(" %c",&type);
                    switch(type){
                        case 'i':scanf(" %lx",&inputLong);
                            int rtrnInt;
                            readProcessMemory(processName, inputLong ,sizeof(int) ,&rtrnInt);
                            printf("value %i in hex %x\n",rtrnInt,rtrnInt);
                            sleep(10);
                        break;

                        case 'l':scanf(" %lx",&inputLong);
                            long rtrnLong;
                            readProcessMemory(processName, inputLong ,sizeof(long) ,&rtrnLong);
                            printf("value %ld in hex %lx\n",rtrnLong,rtrnLong);
                            sleep(10);
                        break;
                    }
                    
                    State = DEFAULT;

                break;

            case WRITEONCE:
                printf("------------------------------------------------------\n\n");
                printf("        Enter type address and value to write\n");
                printf("        sytax: (type) (address) (value)\n");
                printf("        types: i (int), l (long)\n\n");
                printf("------------------------------------------------------\n");
                scanf(" %c",&type);
                unsigned long address;
                switch(type){
                    case 'i':scanf(" %lx %i",&address,&inputInt);writeProcessMemory(processName ,address,sizeof(int) ,&inputInt);break;
                    case 'l':scanf(" %lx %ld",&address,&inputLong);writeProcessMemory(processName ,address,sizeof(int) ,&inputLong);break;
                }
                
                State = DEFAULT;
            break;

            case LOCKDR:
                lockDriver();
                State = DEFAULT;
            break;

            case UNLOCKDR:
                unlockDriver();
                State = DEFAULT;
            break;

            case DEFAULT:
                printf("------------------------------------------------------\n\n");
                printf("           Selected type : %c\n",type);
                printf("            Select option\n\n");
                //printf("    1. Clear kernel list\n");
                printf("    1. Initialise kernel search\n\n");
                printf("    2. Continue kernel search\n\n");
                printf("    3. Write the addresses in the kernel list\n\n");
                printf("    4. Read single address\n\n");
                printf("    5. Write single address\n\n");
                printf("    6. Print List\n\n");
                //printf("    8. Send List to kernel\n\n");
                printf("------------------------------------------------------\n");
                while(!(scanf("%i",&menuSelection) == 1 && menuSelection>0 && menuSelection <8));
                switch(menuSelection){
                    //case 1: State = CLEAR; break;
                    case 1: State = INIT_SR; break;
                    case 2: if(type == 'i' || type == 'l' || type == 's')State = CONT_SR;
                        break;
                    case 3: if(type == 'i' || type == 'l' || type == 's')State = WRITE_LIST_ALL;
                        break;
                    case 4: State = READONCE; break;
                    case 5: State = WRITEONCE; break;
                    case 6: State = PRINTLIST; break;
                    case 7: State = REFRESH_VALUES; break;
                    case 8: State = LOCKDR; break;
                    case 9: State = UNLOCKDR; break;
                }
            break;
            
            default: State = DEFAULT;
        }
    }
}


int main() {
   
    pthread_t RefreshListThread;
    //pthread_create(&RefreshListThread, NULL, do_something, NULL);
    PID = getpid();
    menu();


    return 0;
}