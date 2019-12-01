// Aniket Sanghi
// 170110

#include <cfork.h>
#include <page.h>
#include <mmap.h>



/* You need to implement cfork_copy_mm which will be called from do_cfork in entry.c. Don't remove copy_os_pts()*/
void cfork_copy_mm(struct exec_context *child, struct exec_context *parent ){

	void *phy_addr;
	struct mm_segment *mem;
	u64 vir_addr;

	// Allocate a separate CR3 for child and initialise it to zero
	child->pgd = os_pfn_alloc(OS_PT_REG);
	phy_addr = osmap(child->pgd);
	
	// Initialise the page with empty entries
	bzero((char *)phy_addr, PAGE_SIZE);

	// For Code Segment 
	// Credits: copy_mm()
	mem = &parent->mms[MM_SEG_CODE];
	for(vir_addr = mem->start; vir_addr < mem->next_free; vir_addr += PAGE_SIZE) {

		// Get user PTE and set same value for child
		u64 *pte = get_user_pte(parent, vir_addr, 0);
		if(pte != NULL) 
			install_ptable((u64)phy_addr, mem, vir_addr, (*pte & FLAG_MASK) >> PAGE_SHIFT);
	}


	// For RODATA Segment 
	// Credits: copy_mm()
	mem = &parent->mms[MM_SEG_RODATA];
	for(vir_addr = mem->start; vir_addr < mem->next_free; vir_addr += PAGE_SIZE) {

		// Get user PTE and set same value for child
		u64 *pte = get_user_pte(parent, vir_addr, 0);
		if(pte != NULL) 
			install_ptable((u64)phy_addr, mem, vir_addr, (*pte & FLAG_MASK) >> PAGE_SHIFT);
	}

	//
	// For DATA Segment 
	//
	mem = &parent->mms[MM_SEG_DATA];
	for(vir_addr = mem->start; vir_addr < mem->next_free; vir_addr += PAGE_SIZE) {

		// Get user PTE and set same value for child
		u64 *pte = get_user_pte(parent, vir_addr, 0);
		if(pte != NULL) {

			// Map it to same physical page with only READ permission
			u64 pfn = map_physical_page((u64)phy_addr, vir_addr, MM_RD, (*pte & FLAG_MASK) >> PAGE_SHIFT);

			// Change Parent's write permission to also read permission
			*pte = *pte & ~(1 << 1);

			// Increment the ref_count for the pfn
			struct pfn_info *pfn_ref = get_pfn_info((u32) pfn);
			increment_pfn_info_refcount(pfn_ref);

			asm volatile ("invlpg (%0);" 
                    :: "r"(vir_addr) 
                    : "memory");   // Flush TLB so that set read permission can take effect
		} 
			
	}


	//
	// For MMapped Segment
	//
	struct vm_area *head = parent->vm_area;

	// Iterate over all allocated regions | regions with 
	while(head != NULL) {

		for(vir_addr = head->vm_start; vir_addr < head->vm_end; vir_addr += PAGE_SIZE) {

			// Get user PTE and set same value for child
			u64 *pte = get_user_pte(parent, vir_addr, 0);
			if(pte != NULL) {

				// Map it to same physical page with only READ permission
				u64 pfn = map_physical_page((u64)phy_addr, vir_addr, MM_RD, (*pte & FLAG_MASK) >> PAGE_SHIFT);

				// Change Parent's write permission to also read permission
				*pte = *pte & ~(1 << 1);

				// Increment the ref_count for the pfn
				struct pfn_info *pfn_ref = get_pfn_info((u32) pfn);
				increment_pfn_info_refcount(pfn_ref);

				asm volatile ("invlpg (%0);" 
                    :: "r"(vir_addr) 
                    : "memory");   // Flush TLB so that set read permission can take effect
			} 

		}

		head = head->vm_next;
	}

	// Copying the vm_area
	child->vm_area = NULL;

	if(parent->vm_area != NULL) {

		child->vm_area = alloc_vm_area();
		*child->vm_area = *parent->vm_area;
	

		struct vm_area *headp = parent->vm_area;
		struct vm_area *headc = child->vm_area;

		while(headp->vm_next != NULL) {

			headc->vm_next = alloc_vm_area();
			*headc->vm_next = *headp->vm_next;
			headp = headp->vm_next;
			headc = headc->vm_next;
		}
	}





	//
	// STACK segment
	//
	mem = &parent->mms[MM_SEG_STACK];
  	for(vir_addr = mem->end - PAGE_SIZE; vir_addr >= mem->next_free; vir_addr -= PAGE_SIZE){
    	u64 *pte =  get_user_pte(parent, vir_addr, 0);
      
    	if(pte != NULL){
        	u64 pfn = install_ptable((u64)phy_addr, mem, vir_addr, 0);  //Returns the blank page  
            pfn = (u64)osmap(pfn);
            memcpy((char *)pfn, (char *)(*pte & FLAG_MASK), PAGE_SIZE); 
      	}
  	} 

    copy_os_pts(parent->pgd, child->pgd);
    return;
    
}

/* You need to implement cfork_copy_mm which will be called from do_vfork in entry.c.*/
void vfork_copy_mm(struct exec_context *child, struct exec_context *parent ){
  	
  	parent->state = WAITING;
  	
  	struct mm_segment *mem;
  	u64 vir_addr;

  	mem = &parent->mms[MM_SEG_STACK];
  	u64 child_vir_addr;

  	// Allocating child a separate stack in the same memory space as the parent won't run until child finish
  	for(vir_addr = mem->end - PAGE_SIZE, child_vir_addr = mem->next_free - PAGE_SIZE; vir_addr >= mem->next_free; vir_addr -= PAGE_SIZE, child_vir_addr -= PAGE_SIZE){
    	u64 *pte =  get_user_pte(parent, vir_addr, 0);
    	u64 *pte2= get_user_pte(child, child_vir_addr, 0);
      
    	if(pte != NULL){
    		u64 pfn;
        	if(pte2 != NULL) pfn = install_ptable((u64)osmap(child->pgd), mem, child_vir_addr, *pte2 >> PAGE_SHIFT);  //Returns the blank page  
        	else pfn = install_ptable((u64)osmap(child->pgd), mem, child_vir_addr, 0);
            pfn = (u64)osmap(pfn);
            memcpy((char *)pfn, (char *)(*pte & FLAG_MASK), PAGE_SIZE); 
      	}

  	} 

  	// Updating child parameters for the same
  	child->regs.entry_rsp += (mem->next_free - mem->end);
  	child->regs.rbp += (mem->next_free - mem->end);
  	(&child->mms[MM_SEG_STACK])->next_free += (mem->next_free - mem->end);

    return;
    
}

/*You need to implement handle_cow_fault which will be called from do_page_fault 
incase of a copy-on-write fault

* For valid acess. Map the physical page 
 * Return 1
 * 
 * For invalid access,
 * Return -1. 
*/

int handle_cow_fault(struct exec_context *current, u64 cr2){

	struct mm_segment *mem = &current->mms[MM_SEG_DATA];

	// If the address is in DATA SEGMENT
	if(cr2 >= mem->start && cr2 < mem->next_free) {

		// get current process page
		u64 *pte = get_user_pte(current, cr2, 0);

		// Check the refcount of the corresponding pfn
		struct pfn_info *pfn_ref = get_pfn_info((*pte & FLAG_MASK) >> PAGE_SHIFT);
		u8 refcount = get_pfn_info_refcount(pfn_ref);

		// If refcount was 1 then just change the permission of the corresponding PTE entry
		if(refcount == 1) {

			*pte |= 0x2;
		}
		// Else, We need to alloc a different page for current process at cr2
		else {

			u64 pfn = os_pfn_alloc(USER_REG); 
			memcpy((char *)osmap(pfn), (char *)(*pte & FLAG_MASK), PAGE_SIZE);
			*pte = (pfn << PAGE_SHIFT) | 0x7;
			decrement_pfn_info_refcount(pfn_ref);
			
		}

		asm volatile ("invlpg (%0);" 
                    :: "r"(cr2) 
                    : "memory");   // Flush TLB so that our PTE change's effect can be seen

		return 1;
	}

	struct vm_area *head = current->vm_area;

	// If the address is in the MMAPPED SEGMENT
	while(head != NULL) {

		if(cr2 >= head->vm_start && cr2 < head->vm_end){

			if(!(head->access_flags & MM_WR)) return -1;
			else {

				// get current process page
				u64 *pte = get_user_pte(current, cr2, 0);

				// Check the refcount of the corresponding pfn
				struct pfn_info *pfn_ref = get_pfn_info((*pte & FLAG_MASK) >> PAGE_SHIFT);
				u8 refcount = get_pfn_info_refcount(pfn_ref);

				// If refcount was 1 then just change the permission of the corresponding PTE entry
				if(refcount == 1) {

					*pte |= 0x2;
				}
				// Else, We need to alloc a different page for current process at cr2
				else {

					u64 pfn = os_pfn_alloc(USER_REG); 
					memcpy((char *)osmap(pfn), (char *)(*pte & FLAG_MASK), PAGE_SIZE);
					*pte = (pfn << PAGE_SHIFT) | 0x7;
					decrement_pfn_info_refcount(pfn_ref);
					
				}

				asm volatile ("invlpg (%0);" 
		                    :: "r"(cr2) 
		                    : "memory");   // Flush TLB so that our PTE change's effect can be seen

				return 1;
			}			
		}

		head = head->vm_next;
	}

	

    return -1;
}

/* You need to handle any specific exit case for vfork here, called from do_exit*/
void vfork_exit_handle(struct exec_context *ctx){
	
	// Get parent and update its state to READY
	struct exec_context *parent = get_ctx_by_pid(ctx->ppid);

	if(parent->pgd != ctx->pgd) return;

	parent->state = READY;

	// Updating child updated parameters for parent
	parent->mms[MM_SEG_DATA] = ctx->mms[MM_SEG_DATA];
	parent->vm_area  		 = ctx->vm_area;
  
	return;
}
