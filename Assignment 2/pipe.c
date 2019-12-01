#include<pipe.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
/***********************************************************************
 * Use this function to allocate pipe info && Don't Modify below function
 ***********************************************************************/
struct pipe_info* alloc_pipe_info()
{
    struct pipe_info *pipe = (struct pipe_info*)os_page_alloc(OS_DS_REG);
    char* buffer = (char*) os_page_alloc(OS_DS_REG);
    pipe ->pipe_buff = buffer;
    return pipe;
}


void free_pipe_info(struct pipe_info *p_info)
{
    if(p_info)
    {
        os_page_free(OS_DS_REG ,p_info->pipe_buff);
        os_page_free(OS_DS_REG ,p_info);
    }
}
/*************************************************************************/
/*************************************************************************/


int pipe_read(struct file *filep, char *buff, u32 count)
{
    /**
    *  TODO:: Implementation of Pipe Read
    *  Read the contect from buff (pipe_info -> pipe_buff) and write to the buff(argument 2);
    *  Validate size of buff, the mode of pipe (pipe_info->mode),etc
    *  Incase of Error return valid Error code 
    */
    int ret_fd = -EINVAL; 

    // Validating input
    if(filep == NULL || filep->type != PIPE || filep->mode != O_READ) return -EINVAL;

    struct pipe_info *pipe = filep->pipe;

    // Checking if count is more than the asked number of bytes
    if(count > pipe->buffer_offset) return -EINVAL;

    // Checking the remaining length to read
    count = pipe->buffer_offset>count?count:pipe->buffer_offset;

    // Reading from pipe buffer
    for(int i = 0; i<count; ++i) {
        buff[i] = pipe->pipe_buff[(pipe->read_pos+i)%PIPE_MAX_SIZE];
    }

    // Update the pipe read and size variables
    pipe->read_pos = (pipe->read_pos + count)%PIPE_MAX_SIZE;
    pipe->buffer_offset -= count;

    return count;
}


int pipe_write(struct file *filep, char *buff, u32 count)
{
    /**
    *  TODO:: Implementation of Pipe Read
    *  Write the contect from   the buff(argument 2);  and write to buff(pipe_info -> pipe_buff)
    *  Validate size of buff, the mode of pipe (pipe_info->mode),etc
    *  Incase of Error return valid Error code 
    */
    int ret_fd = -EINVAL; 

    // Validating input
    if(filep == NULL || filep->type != PIPE || filep->mode != O_WRITE) return -EINVAL;

    // Checking the available space
    long int maxlength = PIPE_MAX_SIZE - filep->pipe->buffer_offset;

    if(maxlength < count) return -EOTHERS;

    struct pipe_info *pipe = filep->pipe;

    // Storing in the pipe_buff in a cyclic manner
    for(int i = 0; i < count; ++i) {
        pipe->pipe_buff[(pipe->write_pos+i)%PIPE_MAX_SIZE] = buff[i];
    }

    // Update the pipe variables
    pipe->write_pos = (pipe->write_pos + count)%PIPE_MAX_SIZE;
    pipe->buffer_offset += count;


    return count;
}

int create_pipe(struct exec_context *current, int *fd)
{
    /**
    *  TODO:: Implementation of Pipe Create
    *  Create file struct by invoking the alloc_file() function, 
    *  Create pipe_info struct by invoking the alloc_pipe_info() function
    *  fill the valid file descriptor in *fd param
    *  Incase of Error return valid Error code 
    */
    int ret_fd = -EINVAL; 

    // Validating inputs
    if(current == NULL || fd == NULL || (fd+1) == NULL) return -EINVAL;

    // Allocate the pipe_info object
        struct pipe_info* pipe = alloc_pipe_info();

        // Check Memory error
        if(pipe == NULL) return -ENOMEM;

        // Fill in the parameters
        pipe->read_pos = 0;
        pipe->write_pos = 0;
        pipe->buffer_offset = 0;
        pipe->is_ropen = 1;
        pipe->is_wopen = 1;
    
    // Search for entries in file descriptor table
        int input_fd = 3;
        while(current->files[input_fd] != NULL && input_fd < MAX_OPEN_FILES) input_fd++;
        if(input_fd == MAX_OPEN_FILES) return -EINVAL;

        int output_fd = input_fd+1;
        while(current->files[output_fd] != NULL && output_fd < MAX_OPEN_FILES) output_fd++;
        if(output_fd >= MAX_OPEN_FILES) return -EINVAL;

    // Allocate input file descriptor
        struct file* input_pipe = alloc_file();

        // Check for the allocation error
        if(input_pipe == NULL) return -ENOMEM;

        // Fill the data
        input_pipe->type = PIPE;
        input_pipe->mode = O_READ;
        input_pipe->offp = 0;
        input_pipe->ref_count = 1;

        input_pipe->inode = NULL;
        input_pipe->pipe = pipe;
        input_pipe->fops->read = pipe_read;
        input_pipe->fops->write = pipe_write;
        input_pipe->fops->close = generic_close;

        current->files[input_fd] = input_pipe;

    // Allocate output file descriptor table
        struct file* output_pipe = alloc_file();

        // Check for the allocation error
        if(output_pipe == NULL) return -ENOMEM;

        // Fill the data
        output_pipe->type = PIPE;
        output_pipe->mode = O_WRITE;
        output_pipe->offp = 0;
        output_pipe->ref_count = 1;

        output_pipe->inode = NULL;
        output_pipe->pipe = pipe;
        output_pipe->fops->read = pipe_read;
        output_pipe->fops->write = pipe_write;
        output_pipe->fops->close = generic_close;

        current->files[output_fd] = output_pipe;

    // Assign file descriptors
        fd[0] = input_fd;
        fd[1] = output_fd;

    return 0;
}


