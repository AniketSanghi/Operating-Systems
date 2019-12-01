#include<types.h>
#include<context.h>
#include<file.h>
#include<lib.h>
#include<serial.h>
#include<entry.h>
#include<memory.h>
#include<fs.h>
#include<kbd.h>
#include<pipe.h>


/************************************************************************************/
/***************************Do Not Modify below Functions****************************/
/************************************************************************************/
void free_file_object(struct file *filep)
{
    if(filep)
    {
       os_page_free(OS_DS_REG ,filep);
       stats->file_objects--;
    }
}

struct file *alloc_file()
{
  
  struct file *file = (struct file *) os_page_alloc(OS_DS_REG); 
  file->fops = (struct fileops *) (file + sizeof(struct file)); 
  bzero((char *)file->fops, sizeof(struct fileops));
  stats->file_objects++;
  return file; 
}

static int do_read_kbd(struct file* filep, char * buff, u32 count)
{
  kbd_read(buff);
  return 1;
}

static int do_write_console(struct file* filep, char * buff, u32 count)
{
  struct exec_context *current = get_current_ctx();
  return do_write(current, (u64)buff, (u64)count);
}

struct file *create_standard_IO(int type)
{
  struct file *filep = alloc_file();
  filep->type = type;
  if(type == STDIN)
     filep->mode = O_READ;
  else
      filep->mode = O_WRITE;
  if(type == STDIN){
        filep->fops->read = do_read_kbd;
  }else{
        filep->fops->write = do_write_console;
  }
  filep->fops->close = generic_close;
  filep->ref_count = 1;
  return filep;
}

int open_standard_IO(struct exec_context *ctx, int type)
{
   int fd = type;
   struct file *filep = ctx->files[type];
   if(!filep){
        filep = create_standard_IO(type);
   }else{
         filep->ref_count++;
         fd = 3;
         while(ctx->files[fd])
             fd++; 
   }
   ctx->files[fd] = filep;
   return fd;
}
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/



void do_file_fork(struct exec_context *child)
{
   /*TODO the child fds are a copy of the parent. Adjust the refcount*/
    
  // Update all the file descriptor's ref_count for child
  for(int i = 0; i<MAX_OPEN_FILES; ++i) {

    if(child->files[i] != NULL) {
      child->files[i]->ref_count += 1;
    }

  }

}

void do_file_exit(struct exec_context *ctx)
{
   /*TODO the process is exiting. Adjust the ref_count
     of files*/

  // Close all the file descriptors
  for(int i = 0; i<MAX_OPEN_FILES; ++i) {

    if(ctx->files[i] != NULL) {

      // Close the file descriptor
      generic_close(ctx->files[i]);
      // Nullify the pointer
      ctx->files[i] = NULL;

    }

  }

}

long generic_close(struct file *filep)
{
  /** TODO Implementation of close (pipe, file) based on the type 
   * Adjust the ref_count, free file object
   * Incase of Error return valid Error code 
   */
    int ret_fd = -EINVAL; 

    // Checking for valid input
    if(filep == NULL) return ret_fd;

    // If more than one refernce exists
    if(filep->ref_count > 1) {
      filep->ref_count -= 1;
      return 0;
    }
    
    // Now if no more references exist for the file object

    // For file
    if(filep->type != PIPE) {
      free_file_object(filep);
      if(filep != NULL) return -EOTHERS;
      return 0;
    }

    // For PIPE
    if(filep->type == PIPE) {

      // If this is the last read port of the pipe
      if(filep->mode == O_READ) {

        // Check if the write port of the pipe is already closed
        if(!filep->pipe->is_wopen) {

          // then free the pipe object as the pipe is going to get closed completely
          free_pipe_info(filep->pipe);
        }
        else {

          // Tell the pipe object that read port is gonna close
          filep->pipe->is_ropen = 0;
        }

        free_file_object(filep);
        
      }

      // If this is the last write port of the pipe
      if(filep->mode == O_WRITE) {

        // Check if the read port of the pipe is already closed
        if(!filep->pipe->is_ropen) {

          // then free the pipe object as the pipe is going to get closed completely
          free_pipe_info(filep->pipe);
        }
        else {

          // Tell the pipe object that write port is gonna close
          filep->pipe->is_wopen = 0;
        }

        free_file_object(filep);
        
      }
    }

    return 0;
}

static int do_read_regular(struct file *filep, char * buff, u32 count)
{
   /** TODO Implementation of File Read, 
    *  You should be reading the content from File using file system read function call and fill the buf
    *  Validate the permission, file existence, Max length etc
    *  Incase of Error return valid Error code 
    * */
    int ret_fd = -EINVAL; 

    // Checking the validity of arguments
    if(filep == NULL) return ret_fd;
    
    // Checking the access rights of the file descriptor
    if(!(filep->mode & O_READ)) return -EACCES; 

    // Checking if the read is more that the file size
    if(count > filep->inode->file_size - filep->offp) return -EINVAL;

    // Call read syscall and check the number of bytes read
    int read_bytes = flat_read(filep->inode, buff, count, &filep->offp);

    // Update the offset
    filep->offp += read_bytes;

    ret_fd = read_bytes;

    return ret_fd;
}


static int do_write_regular(struct file *filep, char * buff, u32 count)
{
    /** TODO Implementation of File write, 
    *   You should be writing the content from buff to File by using File system write function
    *   Validate the permission, file existence, Max length etc
    *   Incase of Error return valid Error code 
    * */
    int ret_fd = -EINVAL; 

    // Checking the validity of arguments
    if(filep == NULL) return ret_fd;
    
    // Checking the access rights of the file descriptor
    if(!(filep->mode & O_WRITE)) return -EACCES; 

    // Call read syscall and check the number of bytes read
    int write_bytes = flat_write(filep->inode, buff, count, &filep->offp);

    // If count 
    if(write_bytes == -1) return -ENOMEM;

    // Update the offset
    filep->offp += write_bytes;

    ret_fd = write_bytes;

    return ret_fd;
}

static long do_lseek_regular(struct file *filep, long offset, int whence)
{
    /** TODO Implementation of lseek 
    *   Set, Adjust the ofset based on the whence
    *   Incase of Error return valid Error code 
    * */
    int ret_fd = -EINVAL; 

    // Checking for valid input
    if(filep == NULL) return ret_fd;

    if (whence == SEEK_SET) {

      // If the offset given is out of MAX_FILE_SIZE
      if(offset >= FILE_SIZE) return ret_fd;

      // Update the offset
      filep->offp = offset;
      ret_fd = offset;

    }
    else if(whence == SEEK_CUR) {

      // Check if the offset goes out of file bounds
      if(filep->offp + offset >= FILE_SIZE) return ret_fd;

      // Update the offset
      filep->offp += offset;
      ret_fd = filep->offp;

    }
    else if(whence == SEEK_END) {

      // Check if the offset goes out of file bounds
      if(offset + filep->inode->file_size >= FILE_SIZE) return ret_fd;

      // Update the offset 
      filep->offp = filep->inode->file_size + offset;
      ret_fd = filep->offp;
    }

    return ret_fd; 
}

extern int do_regular_file_open(struct exec_context *ctx, char* filename, u64 flags, u64 mode)
{ 
  /**  TODO Implementation of file open, 
    *  You should be creating file(use the alloc_file function to creat file), 
    *  To create or Get inode use File system function calls, 
    *  Handle mode and flags 
    *  Validate file existence, Max File count is 32, Max Size is 4KB, etc
    *  Incase of Error return valid Error code 
    * */

    // Checking the validity of the ctx
    if(ctx == NULL) return -EINVAL;

    int ret_fd = -EINVAL; 

    if (strlen(filename) >= 256) return ret_fd;

    // Check for existence of the file
    struct inode *curr_inode = lookup_inode(filename);
    if(curr_inode == NULL) {
      
      // If user didn't allowed to create file
      if(!(flags & O_CREAT)) return -EINVAL;

      // Checking the validity of Mode
      if(mode & 0x8) return -EINVAL;

      curr_inode = create_inode(filename, mode);

      // If file creation fails
      if(curr_inode == NULL) return -ENOMEM;

    }
    else {

      // O_CREAT should not be in flag
      if(flags & O_CREAT) return -EINVAL;

      // If flag says O_EXEC (Invalid Argument)
      if(flags & 0x4) return -EINVAL;

    }

    // Check if the access modes match with asked flags
    if(flags & 0x1) {
      if(!(curr_inode->mode & 0x1)) return -EACCES;
    }
    if(flags & 0x2) {
      if(!(curr_inode->mode & 0x2)) return -EACCES;
    }

    ret_fd = 3;
    while(ctx->files[ret_fd] != NULL && ret_fd < MAX_OPEN_FILES) ret_fd++;

    // Max file count reached
    if(ret_fd == MAX_OPEN_FILES) return -EOTHERS;

    // Creating a new file descriptor
    struct file *new_file = alloc_file();

    // If file object cannot be allocated memory
    if(new_file == NULL) return -ENOMEM;

    // update the file descriptor table
    ctx->files[ret_fd] = new_file;

    // Fill in the file structure
    new_file->type = REGULAR;
    new_file->mode = flags;
    new_file->offp = 0;
    new_file->ref_count = 1;
    new_file->inode = curr_inode;
    new_file->fops->read = do_read_regular;
    new_file->fops->write = do_write_regular;
    new_file->fops->lseek = do_lseek_regular;
    new_file->fops->close = generic_close;

    new_file->pipe = NULL;

    return ret_fd;
}

int fd_dup(struct exec_context *current, int oldfd)
{
     /** TODO Implementation of dup 
      *  Read the man page of dup and implement accordingly 
      *  return the file descriptor,
      *  Incase of Error return valid Error code 
      * */
    int ret_fd = -EINVAL;

    // Check for the validity of the arguments
    if(oldfd < 0 || oldfd >= MAX_OPEN_FILES) return ret_fd;

    // Check for the validity of the arguments
    if(current == NULL || current->files[oldfd] == NULL) return ret_fd;

    // Find the minimum file descriptor not allocated
    ret_fd = 0;
    while(current->files[ret_fd] != NULL && ret_fd < MAX_OPEN_FILES) ret_fd++;

    // If no more files can be open
    if(ret_fd == MAX_OPEN_FILES) return -EOTHERS;

    // Point it to the oldfd file descriptor
    current->files[ret_fd] = current->files[oldfd];

    // Increasing the reference count of the file descriptor
    current->files[ret_fd]->ref_count++;

    return ret_fd;
}


int fd_dup2(struct exec_context *current, int oldfd, int newfd)
{
  /** TODO Implementation of the dup2 
    *  Read the man page of dup2 and implement accordingly 
    *  return the file descriptor,
    *  Incase of Error return valid Error code 
    * */
    int ret_fd = -EINVAL; 

    // Check for the validity of the arguments
    if(oldfd < 0 || oldfd >= MAX_OPEN_FILES) return ret_fd;

    // Check for the validity of the arguments
    if(current == NULL || current->files[oldfd] == NULL) return ret_fd;

    // Check if the newfd is in range
    if(newfd >= MAX_OPEN_FILES || newfd < 0) return ret_fd;

    // If both are equal then no need to create another one
    if(oldfd == newfd) return oldfd;

    // If newfd is open, close it
    if(current->files[newfd]) generic_close(current->files[newfd]);

    // Allocate the newfd to oldfd
    current->files[newfd] = current->files[oldfd];

    // Increasing the reference count of the file descriptor
    current->files[oldfd]->ref_count++;

    ret_fd = newfd;

    return ret_fd;
}
