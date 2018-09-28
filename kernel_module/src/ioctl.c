/*
*   Project 1 : Krithika Sekhar, ksekhar ; Ragavendran Balakrishnan, rbalakr2
*
*/

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
    struct task_struct *data;
    struct list_head list;
 };

struct Container_list
{
    __u64 cid;
    struct Thread_list thread_head;
    struct list_head list;
};


// initializing the pointers

extern struct Container_list container_head;
extern struct mutex list_lock;
// the process container for the kernel space

// get_container: return the container with the given _cid
// This iterates through the container list and returns the
// container matching the given cid
struct Container_list *get_container(__u64 cid){

    struct Container_list *temp;
    struct list_head *pos, *q;
    list_for_each_safe(pos, q, &container_head.list) {
        temp = list_entry(pos, struct Container_list, list);
        if( cid == temp->cid) {
            printk("Container with Cid: %llu already exists \n", cid);
            return temp;
        }
    }
    return NULL;
}

//create_container: Return a container with the given cid
// if the cid exists already return the same container
// if its a new cid, create a container, set the container
// with cid and initialize the thread list and returh self.
struct Container_list *create_container(__u64 cid){

    struct Container_list *temp = get_container(cid);
    if(temp == NULL )
    {
        printk("Creating a new container with Cid: %llu\n", cid);
        temp = (struct Container_list*)kmalloc(sizeof(struct Container_list),GFP_KERNEL);
        memset(temp, 0, sizeof(struct Container_list));
        temp->cid = cid;
        INIT_LIST_HEAD(&temp->thread_head.list);
        mutex_lock(&list_lock);
        list_add(&(temp->list), &(container_head.list));
        mutex_unlock(&list_lock);
    }
    return temp;
}

// get_thread: return the thread with the given thread id and container
// This iterates through the container list and returns the
// thread matching the given tid.
struct Thread_list *get_thread(struct Container_list* container, pid_t tid){
    struct Thread_list *temp;
    struct list_head *pos, *q;
    list_for_each_safe(pos, q, &((container->thread_head).list)) {
        temp = list_entry(pos, struct Thread_list, list);
        if( tid == temp->data->pid) {
            printk("Thread with tid: %d already exists \n", tid);
            return temp;
        }
    }
    return NULL;
}

// create_thread: Return the current thread in the given container.
// if the current thread exists already return the thread
// if its a new thread, put it in the container and returh self.
struct Thread_list *create_thread(struct Container_list* container){
    struct Thread_list *temp = get_thread(container, current->pid);
    if(temp == NULL)
    {
        printk("Creating a new thread with Tid: %d\n", current->pid);
        temp = (struct Thread_list*)kmalloc(sizeof(struct Thread_list),GFP_KERNEL);
        memset(temp, 0, sizeof(struct Thread_list));
        temp->data = current;
        mutex_lock(&list_lock);
        list_add(&(temp->list), &((container->thread_head).list));
        mutex_unlock(&list_lock);
    }
    return temp;
}

/*
* Check whether any other thread is active other than the current thread
* Will return the active thread other than the current pointer.
*/
struct Thread_list *get_active_thread(struct Container_list* container, pid_t tid){
    struct list_head *pos, *q;
    struct Thread_list *temp;
    list_for_each_safe(pos, q, &((container->thread_head).list)) {
        temp = list_entry(pos, struct Thread_list, list);
        if ( tid != temp->data->pid) {
            if (temp->data->state == TASK_RUNNING){
                return temp;
            }
        }
    }
    return NULL;
}

/*
* Set the current thread states to running if no other threads are active
* Otherwise set the state to sleep.
*/

void set_thread_state(struct Container_list* container){
    struct Thread_list* other_active_thread = get_active_thread(container, current->pid);
    if(other_active_thread == NULL)
    {
        set_current_state(TASK_RUNNING);
    }
    else
    {
        set_current_state(TASK_INTERRUPTIBLE);
    }
    schedule();
}

/*
* Get the next thread found next to the current thread in the
* current container.
*/
struct Thread_list *get_next_thread(struct task_struct* thread){
    struct Container_list *temp;
    struct list_head *pos, *q, *pos1, *q1;
    struct Thread_list *temp_thread;
    list_for_each_safe(pos, q, &container_head.list) {
        temp = list_entry(pos, struct Container_list, list);
        list_for_each_safe(pos1, q1, &((temp->thread_head).list)) {
            temp_thread = list_entry(pos1, struct Thread_list, list);
            if( thread->pid == temp_thread->data->pid) {
                // return the first entry if the current thread is the last one.
                if (list_is_last(pos1,&temp->thread_head.list))
                {
                    return list_first_entry(&temp->thread_head.list, struct Thread_list, list) ;
                }

                return list_entry(pos1->next, struct Thread_list, list);
            }
        }
    }
    return NULL;
}

/*
* Delete and free the current thread from given thread list
*/
void delete_current_thread(struct Thread_list *current_thread){
    printk("Deleting thread with TID: %d",current_thread->data->pid);
    list_del(&(current_thread->list));
    kfree(current_thread);
    printk(KERN_INFO "Deleted Thread\n");
}

/*
* Delete and free the container container from given container list
*/
void delete_current_container(struct Container_list *current_container){
    printk("Deleting container with CID: %llu",current_container->cid);
    mutex_lock(&list_lock);
    list_del(&(current_container->list));
    mutex_unlock(&list_lock);
    kfree(current_container);
    printk(KERN_INFO "Deleted Container");
}

/*
* Delete the current thread
* set the next thread to wake up
* If all threads are deleted then delete the container
*/

void delete_thread_and_container(__u64 cid, struct task_struct* thread){
    struct Container_list *temp = get_container(cid);
    struct Thread_list* pos = get_thread(temp, thread->pid);
    struct Thread_list *next_thread = NULL;
    struct Thread_list* thread_head = &(temp->thread_head);
    next_thread = get_next_thread(thread);
    wake_up_process(next_thread->data);
    delete_current_thread(pos);
    // checking if the thread list is empty
    if(list_empty(&thread_head->list))
    {
        delete_current_container(temp);
    }
    schedule();
}


/**
 * Delete the task in the container.
 *
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(),
 */
int processor_container_delete(struct processor_container_cmd __user *user_cmd)
{
    struct processor_container_cmd kernel_cmd;
    copy_from_user(&kernel_cmd, (void __user *) user_cmd, sizeof(struct processor_container_cmd));
    delete_thread_and_container(kernel_cmd.cid, current);
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
    struct Container_list *container =  NULL;
    struct Thread_list *thread =  NULL;
    struct processor_container_cmd kernel_cmd;
    copy_from_user(&kernel_cmd, (void __user *) user_cmd, sizeof(struct processor_container_cmd));
    container = create_container(kernel_cmd.cid);
    thread = create_thread(container);
    set_thread_state(container);
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
    struct Thread_list *next_thread = NULL;
    next_thread = get_next_thread(current);
    if(next_thread == NULL)
    {
        return 0;
    }
    if(next_thread !=NULL && next_thread->data->pid != current->pid)
    {
        printk("Switching from %d to %d \n",current->pid,next_thread->data->pid);
        set_current_state(TASK_INTERRUPTIBLE);
        wake_up_process(next_thread->data);
    }
    schedule();
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
