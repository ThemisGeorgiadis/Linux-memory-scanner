#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/string.h>


#define DEVICE_NAME "ReadProcessVAs"
#define MAGIC 'x'
#define SINGLE_READWRITE _IOR(MAGIC, 1, struct instructionPkg *)
#define EMPTY_LIST _IOR(MAGIC, 2, int)
#define INIT_SEARCH _IOR(MAGIC, 3, struct search_struct *)
#define SEARCH_CONT _IOR(MAGIC, 4, struct search_struct *)
#define WRITE_LIST _IOR(MAGIC, 5, struct search_struct *)
#define TRANSFER _IOR(MAGIC, 6, struct list_transfer_struct *)
#define BYTE 8


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

int major;
list *MainListRoot;
int listLenght;


void emptylist(list** root){
    list* curr = *root;
    list* tmpNext;
    while (curr) {
        tmpNext = curr->next;

        if (curr->add_str) {
            if (curr->add_str->value) {
                kfree(curr->add_str->value);
                curr->add_str->value = NULL;
            }
            kfree(curr->add_str);
            curr->add_str = NULL;
        }
        kfree(curr);
        curr = tmpNext;
    }
    *root = NULL;
    listLenght = 0;
}

void addtolist(list** root , addr_struct* addr){
   
    if(*root != NULL){
     list* newnode = kmalloc(sizeof(list),GFP_KERNEL);
     newnode->next = *root;
     newnode->add_str = addr;
     *root = newnode;
    }else{
     
     *root = kmalloc(sizeof(list),GFP_KERNEL);
     (*root)->add_str = addr;
     (*root)->next = NULL;
     
    }
    listLenght++;
}

list* removefromlist(list** root, unsigned long addr) {
    if (*root == NULL) return NULL;

    list* currenT = *root;
    list* prev = NULL;

    while (currenT != NULL && currenT->add_str->address != addr) {
        prev = currenT;
        currenT = currenT->next;
        listLenght--;
    }

    if (currenT == NULL) return NULL; 

    if (prev == NULL) {
        listLenght--;
        *root = currenT->next;
    } else {
        listLenght--;
        prev->next = currenT->next;
    }

    kfree(currenT);
    return *root;
}

 static struct task_struct *getProcessByName(char* Pname){

    struct task_struct *CurrentTask;
    char buffer[TASK_COMM_LEN];

    for_each_process(CurrentTask){
        get_task_comm(buffer ,CurrentTask );

        if(strcmp(buffer , Pname) == 0)return CurrentTask;

    }
    printk(KERN_ERR "PROCESS NOT FOUND\n");
    return NULL;
}

static struct task_struct *getProcessByPid(int pid){

    struct task_struct *CurrentTask;
    for_each_process(CurrentTask){
        
        if(CurrentTask->pid == pid)return CurrentTask;
           
    }
    printk(KERN_ERR "PROCESS NOT FOUND\n");
    return NULL;
}

int write_to_process_memory(struct task_struct *task, unsigned long addr, void *buf, int size) {
    int ret;
    
    ret = access_process_vm(task, addr, buf, size, 1);

    if (ret < 0) {
        printk(KERN_ERR "Failed to write memory to task\n");
        return ret;
    }
    //printk(KERN_INFO "Successfully wrote data to task memory\n");
    return ret;
}

void* read_process_memory(struct task_struct* task , unsigned long addr , int size){
    
    void* buf = kmalloc(size,GFP_KERNEL);
    int result;
    int ret = access_process_vm(task ,addr ,buf,size,0);
    if(ret<0){
        //printk(KERN_ERR "  Unsuccessful read operation!\n");
        return 0;
    }
    //result = *((int*)buf);
    //printk(KERN_INFO "  Contents %x\n", result);

    return buf;

}



void searchWholeAddressSpaceInit(search_struct *sr){

    struct task_struct *task = getProcessByName(sr->processName);
    if(!task){
        printk(KERN_ERR "PROCESS NOT FOUND\n");
        return;
    }

    unsigned long addr;
    unsigned long start;
    unsigned long end;
    long u=0;

    struct mm_struct *mm = task->mm;
    struct vm_area_struct *vma;
    struct vma_iterator vmi ;   

    vma_iter_init(&vmi, mm, 0);
    down_read(&mm->mmap_lock);
    vma = vma_next(&vmi);
    up_read(&mm->mmap_lock);
    
    while(vma){
        addr = vma->vm_start;
        start = vma->vm_start;
        end = vma->vm_end;
        
        void *value = kmalloc(PAGE_SIZE, GFP_KERNEL);

        while(addr + PAGE_SIZE < end){
            value = read_process_memory(task,addr,PAGE_SIZE);
            //printk(KERN_INFO "ADDRESS %lx\n",addr);

            for(unsigned long i=0; i<PAGE_SIZE-(sr->size); i++){

                //printk(KERN_INFO "Address %lx , Value %i\n",addr+i, *((int*)(value+i)));
                switch (sr->type)
                {
                case 'i':

                    if(*((int*)(value+i)) == (int)sr->value){
                        u++;
                        addr_struct *TA = kmalloc(sizeof(addr_struct),GFP_KERNEL);
                        TA->address = addr+i;
                        TA->value = kmalloc(sr->size , GFP_KERNEL);
                        *((int*)(TA->value)) = value+i;
                        addtolist(&MainListRoot,TA);
                        printk(KERN_INFO "Address %lx , Value int %i   %ld\n",addr+i, *((int*)(value+i)) , u);
                        
                    }
                    break;
                case 'l':
                    
                    if(*((long*)(value+i)) == (long)sr->value){
                        u++;
                        addr_struct *TA = kmalloc(sizeof(addr_struct),GFP_KERNEL);
                        TA->address = addr+i;
                        TA->value = kmalloc(sr->size , GFP_KERNEL);
                        *((long*)(TA->value)) = value+i;
                        addtolist(&MainListRoot,TA);
                        printk(KERN_INFO "Address %lx , Value long %ld   %ld\n",addr+i, *((long*)(value+i)) , u);
                        
                    }
                    break;
                
                case 's':
                    if(*((char*)(value+i))){
                        if(strncmp(sr->string , ((char*)(value+i)) , sr->size-1)==0){
                            u++;
                            char tmpChar[256];
                            strncpy(tmpChar,((char*)(value+i)),sr->size-1);
                            tmpChar[sr->size] = '\0';
                            printk(KERN_INFO "Address %lx , Value string %s   %ld\n", addr+i, tmpChar ,u);

                            addr_struct *TA = kmalloc(sizeof(addr_struct),GFP_KERNEL);
                            TA->address = addr+i;
                            TA->value = kmalloc(sr->size , GFP_KERNEL);
                            strcpy((char*)(TA->value) , tmpChar);
                            addtolist(&MainListRoot,TA);

                            
                        }
                    }

                break;

                default:
                    break;
                }
                

            }
            kfree(value);
            addr = addr + PAGE_SIZE;
        }

        if(addr < end){
            value = read_process_memory(task,addr,end-addr);
            
            for(unsigned long i=0; i<(end-addr)-(sr->size); i++){
                //printk(KERN_INFO "Address %lx , Value %i\n",addr+i, *((int*)(value+i)));

                switch (sr->type)
                {
                case 'i':

                    if(*((int*)(value+i)) == (int)sr->value){
                        u++;
                        addr_struct *TA = kmalloc(sizeof(addr_struct),GFP_KERNEL);
                        TA->address = addr+i;
                        TA->value = kmalloc(sr->size , GFP_KERNEL);
                        *((int*)(TA->value)) = value+i;
                        addtolist(&MainListRoot,TA);
                        printk(KERN_INFO "Address %lx , Value int %i   %ld\n",addr+i, *((int*)(value+i)) , u);
                        
                    }
                    break;
                case 'l':
                
                    if(*((long*)(value+i)) == (long)sr->value){
                        u++;
                        addr_struct *TA = kmalloc(sizeof(addr_struct),GFP_KERNEL);
                        TA->address = addr+i;
                        TA->value = kmalloc(sr->size , GFP_KERNEL);
                        *((long*)(TA->value)) = value+i;
                        addtolist(&MainListRoot,TA);
                        printk(KERN_INFO "Address %lx , Value long %i   %ld\n",addr+i, *((long*)(value+i)) , u);
                        
                    }
                    break;

                case 's':
                    if(*((char*)(value+i))){
                        if(strncmp(sr->string , ((char*)(value+i)) , sr->size-1)==0){
                            u++;
                            char tmpChar[256];
                            strncpy(tmpChar,((char*)(value+i)),sr->size-1);
                            tmpChar[sr->size] = '\0';
                            printk(KERN_INFO "Address %lx , Value string %s   %ld\n", addr+i, tmpChar ,u);

                            addr_struct *TA = kmalloc(sizeof(addr_struct),GFP_KERNEL);
                            TA->address = addr+i;
                            TA->value = kmalloc(sr->size , GFP_KERNEL);
                            strcpy((char*)(TA->value) , tmpChar);
                            addtolist(&MainListRoot,TA);
                        }
                    }

                break;
                
                default:
                    break;
                }

            }
        }
        down_read(&mm->mmap_lock);
        vma = vma_next(&vmi);
        up_read(&mm->mmap_lock);
        kfree(value);
    }
    printk(KERN_INFO "List length %i\n", listLenght);

}

void searchList(search_struct *sr){
    struct task_struct *task = getProcessByName(sr->processName);
    list* curr = MainListRoot;
    list* tmpC = NULL;
    long u = 0;
    void* value;
    if(!task){
        printk(KERN_ERR "PROCESS NOT FOUND\n");
        return;
    }

    while(curr){

        value = read_process_memory(task , curr->add_str->address , sr->size);

        switch(sr->type){

            case 'i':
                if(*((int*)value) != (int)sr->value){
                    tmpC = curr->next;
                    removefromlist(&MainListRoot , curr->add_str->address);
                }else{
                    u++;
                    printk(KERN_INFO "Address %lx , Value int %i   %ld\n",curr->add_str->address, *((int*)(value)) , u);
                    tmpC = curr->next;
                }
            break;

            case 'l':
                if(*((long*)value) != (long)sr->value){
                    tmpC = curr->next;
                    removefromlist(&MainListRoot , curr->add_str->address);
                }else{
                    u++;
                    printk(KERN_INFO "Address %lx , Value int %i   %ld\n",curr->add_str->address, *((int*)(value)) , u);
                    tmpC = curr->next;
                }
            break;

            case 's':
                char tmpChar[256];
                strncpy(tmpChar,(char*)value,sr->size-1);
                tmpChar[sr->size]='\0';
                if(!(strcmp(sr->string,tmpChar)==0)){
                    tmpC = curr->next;
                    removefromlist(&MainListRoot , curr->add_str->address);
                }else{
                    u++;
                    printk(KERN_INFO "Address %lx , Value string %s   %ld\n",curr->add_str->address, tmpChar, u);
                    tmpC = curr->next;
                }
            break;


        }


        curr = tmpC;
    }
    printk(KERN_INFO "List length %i\n", listLenght);
}

void writeWholeList(search_struct* sr){
    list* tmpList = MainListRoot;
    struct task_struct *task = getProcessByName(sr->processName);
    if(!task){
        printk(KERN_ERR "PROCESS NOT FOUND\n");
        return;
    }
    while(tmpList){
        printk(KERN_INFO "WRITING ADDR %lx" , tmpList->add_str->address);
        
        switch(sr->type){
            case 'i':
            write_to_process_memory(task, tmpList->add_str->address, &sr->value, sr->size);
            break;
            case 'l':
            write_to_process_memory(task, tmpList->add_str->address, &sr->value, sr->size);
            break;
            case 's':
            write_to_process_memory(task, tmpList->add_str->address, sr->string, sr->size);
            break;
        }
        tmpList = tmpList->next;
    }
}


static long executeInstruction(struct file *file, unsigned int cmd, unsigned long arg){
    instructionPkg curInst;
    
    search_struct sr;
    sr.value = kmalloc(256 , GFP_KERNEL);
    int flag = 1;


    if(cmd == TRANSFER){
        
        list_transfer_struct lts;
        
        list* curr = MainListRoot;
        int count = 0;

        if (copy_from_user(<s, (list_transfer_struct __user *)arg, sizeof(list_transfer_struct))) {
            printk(KERN_ERR "ERROR copying instructions from user!\n");
            return -EFAULT;
        }
        lts.add_str = kmalloc(sizeof(addr_struct)*1024 , GFP_KERNEL);
        
        while(curr && count < 1024){

            lts.add_str[count].address = curr->add_str->address;
            list* tmp = curr->next;
            removefromlist(&MainListRoot , curr->add_str->address);

            count++;
            lts.currentTransfers = count;
            curr = tmp;
        }

        lts.transfersLeft = listLenght;
        printk(KERN_ERR "ll! %i\n",listLenght);



        if (copy_to_user(arg, <s, sizeof(list_transfer_struct))) {
            printk(KERN_ERR "ERROR copying struct to user!\n");
            kfree(lts.add_str);
            return -EFAULT;
        }
        
        if (copy_to_user(&(((list_transfer_struct *)arg)->add_str), lts.add_str, count * sizeof(addr_struct))) {//WRONG
            printk(KERN_ERR "ERROR copying address array to user!\n");
            kfree(lts.add_str);
            return -EFAULT;
        }
        kfree(lts.add_str);
        return 0;
        

    }



    if(cmd == WRITE_LIST){
        if (copy_from_user(&sr, (search_struct __user *)arg, sizeof(search_struct))) {
            printk(KERN_ERR "ERROR copying instructions from user!\n");
            return -EFAULT;
        }
        writeWholeList(&sr);

        if (copy_to_user((search_struct __user *)arg, &sr, sizeof(sr))) {
            printk(KERN_ERR "ERROR copying instructions to user!\n");
            
        }
        return 0;

    }

    if(cmd == SEARCH_CONT){
        if (copy_from_user(&sr, (search_struct __user *)arg, sizeof(search_struct))) {
            printk(KERN_ERR "ERROR copying instructions from user!\n");
            return -EFAULT;
        }

       
        searchList(&sr);
        

        if (copy_to_user((search_struct __user *)arg, &sr, sizeof(sr))) {
            printk(KERN_ERR "ERROR copying instructions to user!\n");
            
        }
        return 0;
    }

    if(cmd == INIT_SEARCH){
       
        if (copy_from_user(&sr, (search_struct __user *)arg, sizeof(search_struct))) {
            printk(KERN_ERR "ERROR copying instructions from user!\n");
            return -EFAULT;
        }

       
        searchWholeAddressSpaceInit(&sr);
        

        if (copy_to_user((search_struct __user *)arg, &sr, sizeof(sr))) {
            printk(KERN_ERR "ERROR copying instructions to user!\n");
            
        }
        return 0;
    }
    

    if(cmd == EMPTY_LIST){

        int state;

        if (copy_from_user(&state, (int __user *)arg, sizeof(int))) {
            printk(KERN_ERR "ERROR copying instructions from user!\n");
            return -EFAULT;
        }
        printk(KERN_INFO "Emptying list!\n");
        
        emptylist(&MainListRoot);
        state = 1;

        if (copy_to_user((int __user *)arg, &state, sizeof(int))) {
            printk(KERN_ERR "ERROR copying instructions to user!\n");
            
        }

        return 0;
    }


    if(cmd == SINGLE_READWRITE){

        if (copy_from_user(&curInst, (instructionPkg __user *)arg, sizeof(curInst))) {
            printk(KERN_ERR "ERROR copying instructions from user!\n");
            return -EFAULT;
        }

        //printk(KERN_INFO "Getting the Caller task!\n");
        struct task_struct* callertask = getProcessByPid(curInst.callerPid);
        if(callertask == NULL){
            printk(KERN_ERR "Caller task not found!\n");
            flag = 0;
            curInst.state = 0;
        }

        struct task_struct* task;

        //printk(KERN_INFO "Getting the Target task!\n");

        if((task = getProcessByName(curInst.processName)) == NULL){
            printk(KERN_WARNING "Failed to find target task!\n");
            flag = 0;
            curInst.state = 0;
        }
        
        if(flag && curInst.read){
            void* temp = read_process_memory(task , (unsigned long)curInst.vaddr , curInst.size);
            if(write_to_process_memory(callertask, (unsigned long)curInst.value, temp, curInst.size)<0) curInst.state = 0;
            curInst.TargetPid = task->pid;
            kfree(temp);
            curInst.state = 1;
        }
        if(flag && curInst.write){

            if(write_to_process_memory(task, (unsigned long)curInst.vaddr, &curInst.value, curInst.size)<0)curInst.state = 0;
            
        }

        if (copy_to_user((instructionPkg __user *)arg, &curInst, sizeof(curInst))) {
            printk(KERN_ERR "ERROR copying instructions to user!\n");
            
        }
        if(curInst.state ==0){
            printk(KERN_ERR "Failed!\n");
        }else{
            //printk(KERN_INFO "Success!\n");
        }
        
        return 0;
        
    }

    
    
    return 0;


}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = executeInstruction,
};



static struct class *cls; 

static int __init M_init(void) {
    printk(KERN_INFO "~~~~~~~~~~~~~~~~~~~~~~~~~Module initialization~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    
    major = register_chrdev(0, DEVICE_NAME, &fops);
     pr_info("major number %d.\n", major); 
 
    cls = class_create(DEVICE_NAME);
    device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME); 

    pr_info("Device created on /dev/%s\n", DEVICE_NAME); 

    return 0;
}

static void __exit M_exit(void) {

    emptylist(&MainListRoot);

    device_destroy(cls, MKDEV(major, 0)); 
    class_destroy(cls); 

    unregister_chrdev(major, DEVICE_NAME);
    printk(KERN_INFO "Unloading module!\n");
}

module_init(M_init);
module_exit(M_exit);



MODULE_LICENSE("GPL");
MODULE_AUTHOR("Themis");
MODULE_DESCRIPTION("");