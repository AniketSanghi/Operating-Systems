// Aniket Sanghi
// 170110


#include "common.h"

/*Function templates. TODO*/

static int atomic_add(unsigned *ptr, long val)
{
       int ret = 0;
       asm volatile( 
                     "lock add %%rsi, (%%rdi);"
                     "pushf;"
                     "pop %%rax;" 
                     "movl %%eax, %0;"
                     : "=r" (ret)
                     : 
                     : "memory", "rax"
                    );

     
     if(ret & 0x80)
               ret = -1;
     else if(ret & 0x40)
               ret = 0;
     else
               ret = 1;
     return ret;
}

void done_one(struct input_manager *in, int tnum)
{
	pthread_mutex_lock(&(in->lock));

	in->being_processed[tnum-1] = NULL;

	pthread_cond_broadcast(&(in->cond));

	pthread_mutex_unlock(&(in->lock));

    return;
}

int read_op(struct input_manager *in, op_t *op, int tnum)
{
   unsigned size = sizeof(op_t);


   // Wait to get the lock... entering critical section
   pthread_mutex_lock(&(in->lock));

   // Copy content except the value of data pointer (as of course thats not in in->curr)
   memcpy(op, in->curr, size - sizeof(unsigned long));

   // Evaluate the op->type and read accordingly
   if(op->op_type == GET || op->op_type == DEL){

   		// No data provided in these scenarios 	
       	in->curr += size - sizeof(op->datalen) - sizeof(op->data);
   
   }
   else if(op->op_type == PUT){

   	   // Update the data pointer of op->data 
       in->curr += size - sizeof(op->data);
       op->data = in->curr;
       in->curr += op->datalen;

   }
   else{
   		// If some random bullshit input type is given
        pthread_mutex_unlock(&(in->lock));
       assert(0);
   }

   // If we exceeded the data counter
   if(in->curr > in->data + in->size)
        {
          pthread_mutex_unlock(&(in->lock));
          return -1;
        }

    // Now we have updated the critical section
    // But If this key is already being processed by some other thread
    // then this thread should wait!
    in->being_processed[tnum-1] = op;


    // Now we will check if there exist some thread which is processing (or) waiting to be processed 
    // has the op with same key but with a lower id value
    int min_id = op->id;
    for(int i = 0; i<THREADS; ++i) {

    	if(in->being_processed[i] != NULL && in->being_processed[i]->key == op->key && in->being_processed[i]->id < min_id) {
    		min_id = in->being_processed[i]->id;
    	}
    }

    // If the current unexecuted min_id is not equal to the current id
    // Then wait
    while(min_id != op->id) {

    	pthread_cond_wait(&(in->cond), &(in->lock));
    	
    	min_id = op->id;
	    for(int i = 0; i<THREADS; ++i) {

	    	if(in->being_processed[i] != NULL && in->being_processed[i]->key == op->key && in->being_processed[i]->id < min_id) {

	    		min_id = in->being_processed[i]->id;
	    	}
	    }
    }

    pthread_mutex_unlock(&(in->lock));


   return 0;    
}

int lookup(hash_t *h, op_t *op)
{
	// Calculating the value of hash for this key
	// Lock not needed since critical section is not used yet
	unsigned ctr;
	unsigned hashval = hashfunc(op->key, h->table_size);
	hash_entry_t *entry = h->table + hashval;
	ctr = hashval;

	pthread_mutex_lock(&(entry->lock)); // Take lock for the entry
	
	while((entry->key || entry->id == (unsigned) -1) && entry->key != op->key){
      

    	ctr = (ctr + 1) % h->table_size;
    	
    	// If all elements have been visited 
    	if(ctr == hashval) break;
        
        pthread_mutex_unlock(&(entry->lock));
    		entry = h->table + ctr;
    		pthread_mutex_lock(&(entry->lock));

  	} 
 	
 	if(entry->key == op->key){
    	
    	op->datalen = entry->datalen;
    	op->data = entry->data;
      pthread_mutex_unlock(&(entry->lock));
    	return 0;
 	}

 	pthread_mutex_unlock(&(entry->lock));
 
	return -1;    // Failed
}

int insert_update(hash_t *h, op_t *op)
{

  unsigned ctr;
  unsigned hashval = hashfunc(op->key, h->table_size);
  hash_entry_t *entry = h->table + hashval;

  assert(h && h->used < h->table_size);
  assert(op && op->key);

  ctr = hashval;

  pthread_mutex_lock(&(entry->lock));

   while((entry->key || entry->id == (unsigned) -1) && entry->key != op->key){
         ctr = (ctr + 1) % h->table_size;
         if(ctr == hashval) {
            ctr = -1;
            break;
         }
         pthread_mutex_unlock(&(entry->lock));
         entry = h->table + ctr; 
         pthread_mutex_lock(&(entry->lock));
   } 

  assert(ctr != -1);

  if(entry->key == op->key){  //It is an update
      entry->id = op->id;
      entry->datalen = op->datalen;
      entry->data = op->data;
      pthread_mutex_unlock(&(entry->lock));
      return 0;
  }
  assert(!entry->key);   // Fresh insertion
 
  entry->id = op->id;
  entry->datalen = op->datalen;
  entry->key = op->key;
  entry->data = op->data;

  pthread_mutex_unlock(&(entry->lock));
  
  atomic_add(&(h->used), 1);

  return 0;    
}

int purge_key(hash_t *h, op_t *op)
{

   unsigned ctr;
   unsigned hashval = hashfunc(op->key, h->table_size);
   hash_entry_t *entry = h->table + hashval;
   
   ctr = hashval;

   pthread_mutex_lock(&(entry->lock));
   while((entry->key || entry->id == (unsigned) -1) && 
          entry->key != op->key){

         ctr = (ctr + 1) % h->table_size;
         if(ctr == hashval) break;

         pthread_mutex_unlock(&(entry->lock));
         entry = h->table + ctr; 
         pthread_mutex_lock(&(entry->lock));
   } 


   if(entry->key == op->key){  //Found. purge it
      entry->id = (unsigned) -1;  //Empty but deleted
      entry->key = 0;
      entry->datalen = 0;
      entry->data = NULL;
      

      pthread_mutex_unlock(&(entry->lock));
      atomic_add(&(h->used), -1);
      return 0;
   }

   pthread_mutex_unlock(&(entry->lock));

  return -1;    // Bogus purge
}


