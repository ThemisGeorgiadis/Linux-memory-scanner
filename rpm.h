

#define DEVICE_NAME "ReadProcessVAs"
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
#define BYTE 8

/*
    Struct that holds an address and a pointer to a value

*/
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
/*
    Struct used to hold data to be transfered to the kernel or extracted to the user
*/
typedef struct list_transfer_struct{
    int transfersLeft , currentTransfers;
    int size;
    transfer_node T_node[100];
}list_transfer_struct;

/*
    List node holding addr_struct elements
*/
typedef struct list{
    addr_struct *add_str;
    struct list *next;
}list;

/*
    Used for searching
*/
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

/*
    Old struct for searching. used for single reads or writes
*/
typedef struct instructionPkg{
    char processName[256];
    unsigned long vaddr;
    int size;
    int callerPid;
    int read;
    int write;

    int state; //1 for successful operation and 0 for unsuccessful
    int TargetPid;
    void* value;
} instructionPkg;

//Function Declerations
void emptylist(list** root);
void addtolist(list** root , addr_struct* addr);
list* removefromlist(list** root, unsigned long addr);
static struct task_struct *getProcessByPid(int pid);
int write_to_process_memory(struct task_struct *task, unsigned long addr, void *buf, int size);
void* read_process_memory(struct task_struct* task , unsigned long addr , int size);
void searchWholeAddressSpaceInit(search_struct *sr);
void searchList(search_struct *sr);
void writeWholeList(search_struct* sr);