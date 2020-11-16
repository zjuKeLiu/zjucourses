extern unsigned long pfcount;  
asmlinkage int sys_mysyscall(void)  
{  
        struct task_struct* p =NULL;  
        printk("---------START 335 ----------\n");  
        printk("System page fault: %lu \n",pfcount);  
        printk("Current process page fault:%lu \n",current->pf);  
        printk("----------------------------------\n");  
        printk("Dirty pages of every process:\n");  
  
        for(p = &init_task; (p=next_task(p))!=&init_task;)  
        {  
                printk("PID:---------%d---------\n",p->pid);  
                printk("NAME: %s \n",p->comm);  
                printk("DIRTY PAGE: %d \n",p->nr_dirtied);  
        }  
        printk("----------DONE----------\n");  
        return 0;  
} 
