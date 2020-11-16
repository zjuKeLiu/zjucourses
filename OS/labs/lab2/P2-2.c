#include<unistd.h>  
#include<stdio.h>  
#include<sys/syscall.h>  
#include<stdlib.h>  
#define __NR_mysyscall 335  
  
int main()  
{  
        syscall(__NR_mysyscall);  
        system("dmesg");  
        return 0;  
}  
