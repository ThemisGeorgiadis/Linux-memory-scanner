#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define DEVICE_PATH "/dev/ReadProcessVAs"
#define MAGIC 'x'
#define SINGLE_READWRITE _IOR(MAGIC, 1, struct instructionPkg *)
#define EMPTY_LIST _IOR(MAGIC, 2, int)
#define INIT_SEARCH _IOR(MAGIC, 3, struct search_struct *)
#define SEARCH_CONT _IOR(MAGIC, 4, struct search_struct *)
#define COMMAND5 _IOR(MAGIC, 5, struct search_struct *)
#define BYTE 8;



typedef struct addr_struct{
    unsigned long address;
    void* value;
}addr_struct;

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
    int value;
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




int Kernel_Init_search(char* name , int value){
    search_struct sr;
    int deviceFile = open(DEVICE_PATH, O_RDWR);
    if (deviceFile < 0) {
        printf("Failed to open device file\n");
        return 0;
    }

    sr.callerPid = getpid();
    sr.value = value;
    sr.type = 'i';
    strncpy(sr.processName, name, sizeof(sr.processName) - 1);
    sr.processName[sizeof(sr.processName) - 1] = '\0';
    sr.size = sizeof(int);
    
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

int Kernel_Cont_search(char* name , int value){
    search_struct sr;
    int deviceFile = open(DEVICE_PATH, O_RDWR);
    if (deviceFile < 0) {
        printf("Failed to open device file\n");
        return 0;
    }

    sr.callerPid = getpid();
    sr.value = value;
    sr.type = 'i';
    strncpy(sr.processName, name, sizeof(sr.processName) - 1);
    sr.processName[sizeof(sr.processName) - 1] = '\0';
    sr.size = sizeof(int);


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

int writeKernelList(char* name , int value){
    search_struct sr;
    strncpy(sr.processName, name, sizeof(sr.processName) - 1);
    sr.processName[sizeof(sr.processName) - 1] = '\0';
    sr.value = value;
    sr.size = sizeof(int);

    int deviceFile = open(DEVICE_PATH, O_RDWR);
    if (deviceFile < 0) {
        printf("Failed to open device file\n");
        return 0;
    }

    if (ioctl(deviceFile, COMMAND5, &sr) == -1) {
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
enum state {CLEAR , INIT_SR , CONT_SR , WRITE_LIST , DEFAULT};

void menu(){
    enum state State = DEFAULT;
    int input;
    system("clear");
    printf("Enter process name to attach\n\n");
    char processName[256];
    scanf("%s",processName);
    while(1){
        system("clear");
        switch(State){

            case CLEAR:
                emptyList();
                State = DEFAULT;
            break;

            case INIT_SR:
                printf("-----------------------------------------\n\n");
                printf("Enter value to search\n\n");
                printf("-----------------------------------------\n");

                scanf("%i",&input);
                Kernel_Init_search(processName, input);
                State = DEFAULT;
            break;

            case CONT_SR:
                printf("-----------------------------------------\n\n");
                printf("Enter value to search\n\n");
                printf("-----------------------------------------\n");

                scanf("%i",&input);
                Kernel_Cont_search(processName,input);
                State = DEFAULT;
            break;

            case WRITE_LIST:
                printf("-----------------------------------------\n\n");
                printf("Enter value to Write\n\n");
                printf("-----------------------------------------\n");

                int datatowrite;
                scanf("%i",&datatowrite);
                writeKernelList(processName, datatowrite);
                State = DEFAULT;
            break;

            case DEFAULT:
                printf("-----------------------------------------\n");
                printf("Select option\n");
                printf("1. Clear kernel list\n");
                printf("2. Initialise kernel search\n");
                printf("3. Continue kernel search\n");
                printf("4. Write the addresses in the kernel list\n");
                printf("-----------------------------------------\n");
                while(!(scanf("%i",&input) == 1 && input>0 && input <5));
                switch(input){
                    case 1: State = CLEAR; break;
                    case 2: State = INIT_SR; break;
                    case 3: State = CONT_SR; break;
                    case 4: State = WRITE_LIST; break;
                }
            break;
        }
    }
}


int main() {
   

    menu();


    return 0;
}