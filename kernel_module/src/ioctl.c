//////////////////////////////////////////////////////////////////////
//                      North Carolina State University
//
//
//
//                             Copyright 2016
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng, Yu-Chia Liu
//
//   Description:
//     Core of Kernel Module for Processor Container
//
////////////////////////////////////////////////////////////////////////

#include "processor_container.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/types.h>

 
 struct Thread_list
 {
    struct list_head list;
 };

struct Container_list
{
    __u64 cid;
    struct Thread_list *head;
    struct list_head list;
};


// initializing the pointers 

extern struct Container_list container_head;
extern struct mutex list_lock;
// the process container for the kernel space

struct Container_list *get_container(__u64 cid){
    
    struct Container_list *temp;
    struct list_head *pos, *q;   
    list_for_each_safe(pos, q, &container_head.list) {
        temp = list_entry(pos, struct Container_list, list);
        if( cid == temp->cid) {
            return temp;
        }
    }
    return NULL;
}



struct Container_list *create_container(__u64 cid){

    struct Container_list *temp = get_container(cid);
    if(temp == NULL )
    {
        printk("Creating a new container with Cid: %llu\n", cid);
        temp = (struct Container_list*)kmalloc(sizeof(struct Container_list),GFP_KERNEL);
        memset(temp, 0, sizeof(struct Container_list));
        mutex_init(&list_lock);
        temp->cid = cid;
        mutex_lock(&list_lock);
        list_add(&(temp->list), &(container_head.list));
        mutex_unlock(&list_lock);
    }
    return temp;
}


/**
 * Delete the task in the container.
 * 
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(), 
 */
int processor_container_delete(struct processor_container_cmd __user *user_cmd)
{
    printk(KERN_INFO "deleting container\n");
    return 0;
}

/**
 * Create a task in the corresponding container.
 * external functions needed:
 * copy_from_user(), mutex_lock(), mutex_unlock(), set_current_state(), schedule()
 * 
 * external variables needed:
 * struct task_struct* current  
 */
int processor_container_create(struct processor_container_cmd __user *user_cmd)
{
    struct processor_container_cmd kernel_cmd;
    //struct task_struct* current;
    copy_from_user(&kernel_cmd, (void __user *) user_cmd, sizeof(struct processor_container_cmd));
    struct Container_list *container =  NULL;
    container = create_container(kernel_cmd.cid);
    printk(KERN_INFO "Hello world 1.\n");
    printk(KERN_INFO "creating container\n");
    return 0;
}

/**
 * switch to the next task in the current container
 * 
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(), set_current_state(), schedule()
 */
int processor_container_switch(struct processor_container_cmd __user *user_cmd)
{
    //struct task_struct *current;

    printk(KERN_INFO "switching container\n");
    // https://www.linuxjournal.com/article/8144
    // mutex_lock()
    // sleeping_task = current;
    // set_current_state(TASK_INTERRUPTIBLE);
    // schedule();
    // mutex_unlock();

    return 0;
}

/**
 * control function that receive the command in user space and pass arguments to
 * corresponding functions.
 */
int processor_container_ioctl(struct file *filp, unsigned int cmd,
                              unsigned long arg)
{
    switch (cmd)
    {
    case PCONTAINER_IOCTL_CSWITCH:
        return processor_container_switch((void __user *)arg);
    case PCONTAINER_IOCTL_CREATE:
        return processor_container_create((void __user *)arg);
    case PCONTAINER_IOCTL_DELETE:
        return processor_container_delete((void __user *)arg);
    default:    
        return -ENOTTY;
    }
}
