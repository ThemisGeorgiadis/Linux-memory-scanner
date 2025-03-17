#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define DEVICE_PATH "/dev/ReadProcessVAs"
#define MAGIC 'x'
#define SINGLE_READWRITE _IOR(MAGIC, 1, struct instructionPkg *)
#define EMPTY_LIST _IOR(MAGIC, 2, int)
#define INIT_SEARCH _IOR(MAGIC, 3, struct search_struct *)
#define SEARCH_CONT _IOR(MAGIC, 4, struct search_struct *)
#define WRITE_LIST _IOR(MAGIC, 5, struct search_struct *)
#define TRANSFER _IOR(MAGIC, 6, struct list_transfer_struct *)
#define BYTE 8;


typedef struct addr_struct{
    unsigned long address;
    void* value;
}addr_struct;

typedef struct list_transfer_struct{
    int transfersLeft , listLenght , currentTransfers;
    addr_struct* add_str;
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
}

list* removefromlist(list** root, unsigned long addr) {
    if (*root == NULL) return NULL;

    list* currenT = *root;
    list* prev = NULL;

    while (currenT != NULL && currenT->add_str->address != addr) {
        prev = currenT;
        currenT = currenT->next;
    }

    if (currenT == NULL) return NULL; 

    if (prev == NULL) {
        
        *root = currenT->next;
    } else {
        
        prev->next = currenT->next;
    }

    free(currenT);
    return *root;
}

int extractList(){

    list_transfer_struct *lts = malloc(sizeof(list_transfer_struct));
    lts->transfersLeft = 1;
    lts->add_str = malloc(sizeof(addr_struct)*1024);
    int deviceFile = open(DEVICE_PATH, O_RDWR);
    if (deviceFile < 0) {
        printf("Failed to open device file\n");
        return 0;
    }

    while(lts->transfersLeft > 0){

        if (ioctl(deviceFile, TRANSFER, lts) == -1) {
            printf("ERROR\n");
            close(deviceFile);
            return 0;
        }
        printf("transfers left %i , current transfers %i\n",lts->transfersLeft , lts->currentTransfers);
        if(!(lts->add_str))printf("tsifsa\n");
        for(int i=0; i<lts->currentTransfers; i++){

            addr_struct *TA = malloc(sizeof(addr_struct));
            TA->address = lts->add_str[i].address;
            addtolist(&MainListRoot,TA);
            printf("Address %x\n",TA->address);
            
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


void emptyList(){
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
enum state {CLEAR , INIT_SR , CONT_SR , WRITE_LIST_ALL ,READONCE, WRITEONCE, DEFAULT};

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

            case CLEAR:
                emptyList();
                State = DEFAULT;
            break;

            case INIT_SR:
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
            break;

            case CONT_SR:
                printf("------------------------------------------------------\n\n");
                printf("        Enter value to search\n\n");
                printf("------------------------------------------------------\n");
                
                switch(type){
                    case 'i':scanf(" %i",&inputInt);Kernel_Cont_search(processName, &inputInt , type);break;
                    case 'l':scanf(" %ld",&inputLong);Kernel_Cont_search(processName, &inputLong , type);break;
                    case 's':scanf(" %s",inputString);Kernel_Cont_search(processName, &inputString , type);break;
                }

                State = DEFAULT;
            break;

            case WRITE_LIST_ALL:
                printf("------------------------------------------------------\n\n");
                printf("        Enter value to Write\n\n");
                printf("------------------------------------------------------\n");

                switch(type){
                    case 'i':scanf(" %i",&inputInt);writeKernelList(processName, &inputInt , type);break;
                    case 'l':scanf(" %ld",&inputLong);writeKernelList(processName, &inputLong , type);break;
                    case 's':scanf(" %s",inputString);writeKernelList(processName, &inputString , type);break;
                }

                State = DEFAULT;
            break;

            case READONCE:
                printf("------------------------------------------------------\n\n");
                    printf("        Enter type and address to read\n");
                    printf("        sytax: (type) (value)\n");
                    printf("        types: i (int), l (long)\n\n");
                    printf("------------------------------------------------------\n");
                    scanf(" %c",&type);
                    switch(type){
                        case 'i':scanf(" %ld",&inputLong);
                            int rtrnInt;
                            readProcessMemory(processName, inputLong ,sizeof(int) ,&rtrnInt);
                            printf("value %i in hex %x\n",rtrnInt);
                            sleep(10);
                        break;

                        case 'l':scanf(" %ld",&inputLong);
                            long rtrnLong;
                            readProcessMemory(processName, inputLong ,sizeof(long) ,&rtrnLong);
                            printf("value %ld in hex %lx\n",rtrnLong);
                            sleep(10);
                        break;
                    }
                    
                    State = DEFAULT;

                break;

            case WRITEONCE:
                printf("------------------------------------------------------\n\n");
                printf("        Enter type address and value to write\n");
                printf("        sytax: (type) (value)\n");
                printf("        types: i (int), l (long)\n\n");
                printf("------------------------------------------------------\n");
                scanf(" %c",&type);
                unsigned long address;
                switch(type){
                    case 'i':scanf(" %ld %i",&address,&inputInt);writeProcessMemory(processName ,address,sizeof(int) ,&inputInt);break;
                    case 'l':scanf(" %ld %ld",&address,&inputLong);writeProcessMemory(processName ,address,sizeof(int) ,&inputLong);break;
                }
                
                State = DEFAULT;
            break;

            case DEFAULT:
                printf("------------------------------------------------------\n\n");
                printf("           Selected type : %c\n",type);
                printf("            Select option\n\n");
                printf("    1. Clear kernel list\n");
                printf("    2. Initialise kernel search\n");
                printf("    3. Continue kernel search\n");
                printf("    4. Write the addresses in the kernel list\n\n");
                printf("    5. Read single address\n\n");
                printf("    6. Write single address\n\n");
                printf("------------------------------------------------------\n");
                while(!(scanf("%i",&menuSelection) == 1 && menuSelection>0 && menuSelection <7));
                switch(menuSelection){
                    case 1: State = CLEAR; break;
                    case 2: State = INIT_SR; break;
                    case 3: if(type == 'i' || type == 'l' || type == 's')State = CONT_SR;
                        break;
                    case 4: if(type == 'i' || type == 'l' || type == 's')State = WRITE_LIST_ALL;
                        break;
                    case 5: State = READONCE; break;
                    case 6: State = WRITEONCE; break;
                }
            break;
            
            default: State = DEFAULT;
        }
    }
}


int main() {
   

    menu();


    return 0;
}