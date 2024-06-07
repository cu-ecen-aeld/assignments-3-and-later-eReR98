/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include "aesd_ioctl.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("eReR98"); /** DO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{

    loff_t retval = 0;
    loff_t totalSize = 0;
    loff_t new_fpos = 0;
    size_t i = 0;
    struct aesd_buffer_entry *tempEntry;

    if(mutex_lock_interruptible(&(aesd_device.aesdLock)) != 0)
    {
        return -1;
    }

    AESD_CIRCULAR_BUFFER_FOREACH(tempEntry, &aesd_device.circBuff, i)
    {
        totalSize += tempEntry->size;
    }

    new_fpos = fixed_size_llseek(filp, offset, whence, totalSize); //kernel provided

    filp->f_pos = new_fpos;
    retval = new_fpos;

    mutex_unlock(&(aesd_device.aesdLock));

    return retval;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long retval = 0;
    

    if(mutex_lock_interruptible(&(aesd_device.aesdLock)) != 0)
    {
        return -1;
    }


    switch(cmd)
    {
        case AESDCHAR_IOCSEEKTO:
        {
            struct aesd_seekto seekto;
            loff_t totalCnt = 0;

            if(copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)) != 0)
            {
                retval = EFAULT;
            }
            else
            {
                //retval = aesd_adjust_file_offset(filp,seekto.write_cmd, seekto.write_cmd_offset);
                
                // compiler didn't like the for loop
                // for(int i = 0; i < seekto.write_cmd; i++)
                // {
                //     totalCnt += aesd_device.circBuff.entry[i].size;
                // }

                size_t i=0;

                while(i < seekto.write_cmd)
                {
                    totalCnt += aesd_device.circBuff.entry[i].size;
                    i++;
                }

                totalCnt += seekto.write_cmd_offset;

                filp->f_pos = totalCnt;
            }
            break;
        }

        default:
        {
            retval = -1;
        }
    }

    mutex_unlock(&(aesd_device.aesdLock));

    return retval;


}

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * DO: handle open
     */

    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * DO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;

    struct aesd_buffer_entry *tempEntry;
    size_t char_offset = 0;
    size_t char_remaining = 0;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    
    if (mutex_lock_interruptible(&aesd_device.aesdLock) !=0)
    {
        return -ERESTART;
    }

    tempEntry = aesd_circular_buffer_find_entry_offset_for_fpos(&(aesd_device.circBuff), *f_pos, &char_offset);
    
    if(tempEntry == NULL)
    {
        retval=0;
    } 
    else
    {
        char_remaining = tempEntry->size - char_offset;

        if(count == char_remaining)
        {
            retval = count;
        } 
        else
        {
            retval = char_remaining;
        }

        if(copy_to_user(buf, tempEntry->buffptr + char_offset, retval) != 0)
        {
            retval = -1;
        }
    } 

    *f_pos = *f_pos + retval; // Should work for lseek

    mutex_unlock(&aesd_device.aesdLock);

    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    size_t curr_count = 0;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */

    if(mutex_lock_interruptible(&(aesd_device.aesdLock)) != 0)
    { 
        return -1;
    }

    if(aesd_device.buffString == NULL)
    {
        aesd_device.buffString = kmalloc(count+1, GFP_KERNEL);
        memset(aesd_device.buffString, 0, count+1);

        if(copy_from_user(&aesd_device.buffString[0], buf, count) != 0)
        {
            retval = -1;
        }
        else
        {
            retval = count;
        }

    }
    else
    {
        char* tempChar = aesd_device.buffString;
        curr_count = strlen(tempChar);

        aesd_device.buffString = krealloc(aesd_device.buffString, curr_count + count + 1, GFP_KERNEL);
        memset(&aesd_device.buffString[curr_count], 0, curr_count+count);

        if(copy_from_user(&aesd_device.buffString[curr_count], buf, count) != 0)
        {
            retval = -1;
        }
        else
        {
            retval = count;
        }
    }


    if(aesd_device.buffString[(count+curr_count) - 1] == '\n')
    {
        const char *retEntry;
        struct aesd_buffer_entry *newEntry; 
        newEntry = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);
        newEntry->size=count+curr_count;
        newEntry->buffptr=aesd_device.buffString;

        retEntry = aesd_circular_buffer_add_entry(&aesd_device.circBuff, newEntry);
        kfree(retEntry);
        aesd_device.buffString = NULL;
    }

    *f_pos = *f_pos + retval; // for lseek

    mutex_unlock(&(aesd_device.aesdLock)); 
    return retval;
}

struct file_operations aesd_fops = {
    .owner =            THIS_MODULE,
    .read =             aesd_read,
    .write =            aesd_write,
    .open =             aesd_open,
    .release =          aesd_release,
    .llseek =           aesd_llseek,
    .unlocked_ioctl =   aesd_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * DO: initialize the AESD specific portion of the device
     */

    mutex_init(&aesd_device.aesdLock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    struct aesd_buffer_entry *tempEntry;
    size_t i;

    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);



    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    kfree(aesd_device.buffString);

    AESD_CIRCULAR_BUFFER_FOREACH(tempEntry, &aesd_device.circBuff, i)
    {
        kfree(tempEntry->buffptr);
    }

    mutex_destroy(&aesd_device.aesdLock);
    
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
