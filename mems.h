/*
All the main functions with respect to the MeMS are inplemented here
read the function discription for more details

NOTE: DO NOT CHANGE THE NAME OR SIGNATURE OF FUNCTIONS ALREADY PROVIDED
you are only allowed to implement the functions 
you can also make additional helper functions a you wish

REFER DOCUMENTATION FOR MORE DETAILS ON FUNSTIONS AND THEIR FUNCTIONALITY
*/
// add other headers as required
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include <unistd.h>
#include <sys/mman.h>


/*
Use this macro where ever you need PAGE_SIZE.
As PAGESIZE can differ system to system we should have flexibility to modify this 
macro to make the output of all system same and conduct a fair evaluation. 
*/
#define PAGE_SIZE 4096


/*
Initializes all the required parameters for the MeMS system. The main parameters to be initialized are:
1. the head of the free list i.e. the pointer that points to the head of the free list
2. the starting MeMS virtual address from which the heap in our MeMS virtual address space will start.
3. any other global variable that you want for the MeMS implementation can be initialized here.
Input Parameter: Nothing
Returns: Nothing
*/

// Define the PROCESS and HOLE types
typedef enum { PROCESS, HOLE } segment_type_t;

typedef struct segment {
  uintptr_t physical_addr; // Physical address of the segment
  uintptr_t mems_virtual_addr; // MeMS virtual address of the segment
  uintptr_t start_addr; // Starting MeMS virtual address of the segment (for process segments)
  size_t size; // Size of the segment in bytes
  int type; // Type of the segment (PROCESS or HOLE)
} segment_t;


typedef struct free_list_node {
  segment_t segment; // Segment information
  struct free_list_node *next; // Pointer to the next node in the free list
  struct free_list_node *sub_chain_head; // Pointer to the head of the sub-chain (if applicable)
} free_list_node;



// Declare the global variables
free_list_node *free_list_head = NULL;
// const uintptr_t STARTING_MEMS_VIRTUAL_ADDR = 4000; // Starting MeMS virtual address
uintptr_t mems_virtual_addr = 4000; // Current MeMS virtual address


void mems_init() {
  // Initialize the free list head to NULL
  free_list_head = NULL;

  // Set the starting MeMS virtual address to 4000
  mems_virtual_addr = 4000;
}



/*
This function will be called at the end of the MeMS system and its main job is to unmap the 
allocated memory using the munmap system call.
Input Parameter: Nothing
Returns: Nothing
*/
void mems_finish() {
  // Traverse the free list and munmap each segment
  free_list_node *curr_node = free_list_head;
  while (curr_node != NULL) {
    munmap((void *) curr_node->segment.physical_addr, curr_node->segment.size);
    curr_node = curr_node->next;
  }
}



/*
Allocates memory of the specified size by reusing a segment from the free list if 
a sufficiently large segment is available. 

Else, uses the mmap system call to allocate more memory on the heap and updates 
the free list accordingly.

Note that while mapping using mmap do not forget to reuse the unused space from mapping
by adding it to the free list.
Parameter: The size of the memory the user program wants
Returns: MeMS Virtual address (that is created by MeMS)
*/ 


// Allocate memory of the specified size
void* mems_malloc(size_t size) {
  // Round up the size to the nearest multiple of PAGE_SIZE
  size = (size + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;

  // Search for a sufficiently large HOLE segment in the free list
  free_list_node *curr_node = free_list_head;
  while (curr_node != NULL) {
    free_list_node *hole_node = curr_node->sub_chain_head;
    while (hole_node != NULL) {
      if (hole_node->segment.type == HOLE && hole_node->segment.size >= size) {
        // Found a suitable HOLE segment, allocate memory from it
        if (hole_node->segment.size == size) {
          // No remaining space, mark the entire segment as PROCESS
          hole_node->segment.type = PROCESS;
          return (void *) (hole_node->segment.mems_virtual_addr);
        } else {
          // Split the HOLE segment into PROCESS and HOLE segments
          free_list_node *new_hole_node = (free_list_node *) malloc(sizeof(free_list_node));
          new_hole_node->segment.mems_virtual_addr = hole_node->segment.mems_virtual_addr + size;
          new_hole_node->segment.physical_addr = hole_node->segment.physical_addr + size;
          new_hole_node->segment.size = hole_node->segment.size - size;
          new_hole_node->segment.type = HOLE;
          new_hole_node->next = hole_node->next;
          hole_node->next = new_hole_node;

          hole_node->segment.size = size;
          hole_node->segment.type = PROCESS;
          return (void *) (hole_node->segment.mems_virtual_addr);
        }
      }

      hole_node = hole_node->next;
    }

    curr_node = curr_node->next;
  }

  // No suitable HOLE segment found, mmap new memory
  segment_t new_segment;
  new_segment.mems_virtual_addr = mems_virtual_addr;
  new_segment.physical_addr = (uintptr_t) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (new_segment.physical_addr == (uintptr_t)MAP_FAILED) {
  return NULL; // Handle error
  }

  new_segment.size = size;
  new_segment.type = PROCESS;

  // Add the new segment to the free list
  free_list_node *new_node = (free_list_node *) malloc(sizeof(free_list_node));
  new_node->segment = new_segment;
  new_node->next = free_list_head;
  free_list_head = new_node;

  // Update the starting MeMS virtual address for future allocations
  mems_virtual_addr += size;

  return (void *) (new_segment.mems_virtual_addr);
}





/*
this function print the stats of the MeMS system like
1. How many pages are utilised by using the mems_malloc
2. how much memory is unused i.e. the memory that is in freelist and is not used.
3. It also prints details about each node in the main chain and each segment (PROCESS or HOLE) in the sub-chain.
Parameter: Nothing
Returns: Nothing but should print the necessary information on STDOUT
*/


// Print statistics about the MeMS system
void mems_print_stats() {
  // Initialize counters
  int total_mapped_pages = 0;
  size_t total_unused_memory = 0;

  // Traverse the free list and accumulate statistics
  free_list_node *curr_node = free_list_head;
  while (curr_node != NULL) {
    total_mapped_pages += curr_node->segment.size / PAGE_SIZE;

    free_list_node *hole_node = curr_node->sub_chain_head;
    while (hole_node != NULL) {
      if (hole_node->segment.type == HOLE) {
        total_unused_memory += hole_node->segment.size;
      }

      hole_node = hole_node->next;
    }

    curr_node = curr_node->next;
  }

  // Print statistics
  printf("Total mapped pages: %d\n", total_mapped_pages);
  printf("Total unused memory: %zu bytes\n", total_unused_memory);

  // Print details about each node in the free list
  curr_node = free_list_head;
  while (curr_node != NULL) {
    printf("Node: %p\n", curr_node);
    printf("  Physical address: %p\n", (void *) curr_node->segment.physical_addr);
    printf("  MeMS virtual address: %p\n", (void *) curr_node->segment.mems_virtual_addr);
    printf("  Segment size: %zu bytes\n", curr_node->segment.size);
    printf("  Segment type: %s\n", curr_node->segment.type == PROCESS ? "PROCESS" : "HOLE");

    if (curr_node->sub_chain_head != NULL) {
      printf("  Sub-chain:\n");
      free_list_node *hole_node = curr_node->sub_chain_head;
      while (hole_node != NULL) {
        printf("    Hole: %p\n", hole_node);
        printf("      Physical address: %p\n", (void *) hole_node->segment.physical_addr);
        printf("      MeMS virtual address: %p\n", (void *) hole_node->segment.mems_virtual_addr);
        printf("      Hole size: %zu bytes\n", hole_node->segment.size);
        printf("      Hole type: %s\n", hole_node->segment.type == PROCESS ? "PROCESS" : "HOLE");

        hole_node = hole_node->next;
      }
    }

    printf("---------------------------------\n");
    curr_node = curr_node->next;
  }
}





/*
Returns the MeMS physical address mapped to ptr ( ptr is MeMS virtual address).
Parameter: MeMS Virtual address (that is created by MeMS)
Returns: MeMS physical address mapped to the passed ptr (MeMS virtual address).
*/
void* mems_get(void *v_ptr) {
    // Check if the virtual address is NULL
    if (v_ptr == NULL) {
        return NULL;
    }

    // Check if the virtual address is within the MeMS virtual address space
    if ((uintptr_t) v_ptr < 4000) {
        return NULL;
    }

    // Traverse the free list to find the corresponding segment
    free_list_node *curr_node = free_list_head;
    while (curr_node != NULL) {
        if ((uintptr_t)v_ptr >= (uintptr_t)curr_node->segment.start_addr &&
        (uintptr_t)v_ptr < (uintptr_t)(curr_node->segment.start_addr + curr_node->segment.size)) {
          uintptr_t offset = (uintptr_t)v_ptr - (uintptr_t)curr_node->segment.start_addr;
          return (void *)(curr_node->segment.physical_addr + offset);
        }
        curr_node = curr_node->next;
    }

    // Virtual address not found in the MeMS virtual address space
    return NULL;
}




/*
this function free up the memory pointed by our virtual_address and add it to the free list
Parameter: MeMS Virtual address (that is created by MeMS) 
Returns: nothing
*/
void mems_free(void* ptr) {
  // Validate the MeMS virtual address
  if (ptr == NULL) {
    return;
  }

  // Traverse the free list to find the corresponding segment
  free_list_node *curr_node = free_list_head;
  while (curr_node != NULL) {
    free_list_node *hole_node = curr_node->sub_chain_head;
    while (hole_node != NULL) {
      if (hole_node->segment.type == PROCESS && hole_node->segment.mems_virtual_addr == (uintptr_t) ptr) {
        // Found the corresponding PROCESS segment, mark it as HOLE
        hole_node->segment.type = HOLE;

        // Coalesce adjacent HOLE segments
        free_list_node *prev_node = curr_node;
        while (prev_node != NULL && prev_node->sub_chain_head->segment.type == HOLE) {
          // Merge the current HOLE segment with the previous HOLE segment
          prev_node->sub_chain_head->segment.size += hole_node->segment.size;
          prev_node->sub_chain_head->next = hole_node->next;
          free(hole_node);
          hole_node = prev_node->sub_chain_head;
        }

        return;
      }

      hole_node = hole_node->next;
    }

    curr_node = curr_node->next;
  }

  // Invalid MeMS virtual address
  // Handle invalid virtual address
}

