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

// Define the segment structure
typedef struct segment {
  void *start_addr;
  size_t size;
  segment_type_t type;
  uintptr_t physical_addr; // Physical address of the segment
} segment_t;

// Define the free_list_node structure
typedef struct free_list_node {
  segment_t segment;
  struct free_list_node *next;
} free_list_node;

// Declare the global variables
free_list_node *free_list_head = NULL;
const uintptr_t STARTING_MEMS_VIRTUAL_ADDR = 0x10000000; // Starting MeMS virtual address
uintptr_t mems_virtual_addr = STARTING_MEMS_VIRTUAL_ADDR; // Current MeMS virtual address


void mems_init(){
    // Initialize the head of the free list to NULL
    free_list_head = NULL;

    // Set the starting MeMS virtual address
    mems_virtual_addr = STARTING_MEMS_VIRTUAL_ADDR;

}


/*
This function will be called at the end of the MeMS system and its main job is to unmap the 
allocated memory using the munmap system call.
Input Parameter: Nothing
Returns: Nothing
*/
void mems_finish(){
    // Traverse the free list and unmap each segment
    free_list_node *curr_node = free_list_head;
    while (curr_node != NULL) {
        munmap((void *) curr_node->segment.start_addr, curr_node->segment.size);
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
void* mems_malloc(size_t size) {
    // Check if there's a sufficiently large segment in the free list
    free_list_node *curr_node = free_list_head;
    free_list_node *prev_node = NULL;
    while (curr_node != NULL) {
        if (curr_node->segment.size >= size) {
            // Split the segment if necessary
            if (curr_node->segment.size > size + sizeof(free_list_node)) {
                // Create a new segment for the remaining unused space
                free_list_node *new_node = malloc(sizeof(free_list_node));
                new_node->segment.start_addr = curr_node->segment.start_addr + size + sizeof(free_list_node);
                new_node->segment.size = curr_node->segment.size - size - sizeof(free_list_node);
                new_node->next = curr_node->next;

                // Update the current segment's size
                curr_node->segment.size = size;

                // Insert the new segment into the free list
                if (curr_node == free_list_head) {
                    free_list_head = new_node;
                } else {
                    prev_node->next = new_node;
                }
            }

            // Allocate the memory and remove the segment from the free list
            void *mems_virtual_addr = curr_node->segment.start_addr;
            if (curr_node == free_list_head) {
                free_list_head = curr_node->next;
            } else {
                prev_node->next = curr_node->next;
            }
            free(curr_node);

            return mems_virtual_addr;
        }

        prev_node = curr_node;
        curr_node = curr_node->next;
    }

    // No suitable segment found in the free list, use mmap to allocate new memory
    void *mems_virtual_addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mems_virtual_addr == MAP_FAILED) {
        return NULL;
    }

    // Add the unused space from mmap to the free list
    size_t unused_space = PAGE_SIZE - (size % PAGE_SIZE);
    if (unused_space > 0) {
        free_list_node *new_node = malloc(sizeof(free_list_node));
        new_node->segment.start_addr = mems_virtual_addr + size;
        new_node->segment.size = unused_space;
        new_node->next = free_list_head;
        free_list_head = new_node;
    }

    return mems_virtual_addr;
}



/*
this function print the stats of the MeMS system like
1. How many pages are utilised by using the mems_malloc
2. how much memory is unused i.e. the memory that is in freelist and is not used.
3. It also prints details about each node in the main chain and each segment (PROCESS or HOLE) in the sub-chain.
Parameter: Nothing
Returns: Nothing but should print the necessary information on STDOUT
*/
void mems_print_stats() {
    // Calculate total utilized memory and unused memory
    size_t total_utilized_memory = 0;
    size_t total_unused_memory = 0;

    free_list_node *curr_node = free_list_head;
    while (curr_node != NULL) {
        if (curr_node->segment.type == PROCESS) {
            total_utilized_memory += curr_node->segment.size;
        } else {
            total_unused_memory += curr_node->segment.size;
        }
        curr_node = curr_node->next;
    }

    // Print statistics header
    printf("\nMeMS System Statistics:\n");

    // Print total utilized memory
    printf("Total Utilized Memory: %lu bytes\n", total_utilized_memory);

    // Print total unused memory
    printf("Total Unused Memory: %lu bytes\n", total_unused_memory);

    // Print free list details
    printf("\nFree List Details:\n");
    curr_node = free_list_head;
    while (curr_node != NULL) {
        printf("Segment Address: %p\n", curr_node->segment.start_addr);
        printf("Segment Size: %lu bytes\n", curr_node->segment.size);
        printf("Segment Type: %s\n", (curr_node->segment.type == PROCESS) ? "PROCESS" : "HOLE");
        printf("\n");
        curr_node = curr_node->next;
    }
}



/*
Returns the MeMS physical address mapped to ptr ( ptr is MeMS virtual address).
Parameter: MeMS Virtual address (that is created by MeMS)
Returns: MeMS physical address mapped to the passed ptr (MeMS virtual address).
*/
void* mems_get(void *v_ptr) {
    // Check if the virtual address is within the MeMS virtual address space
    if ((uintptr_t) v_ptr < mems_virtual_addr) {
        return NULL;
    }

    // Traverse the free list to find the corresponding segment
    free_list_node *curr_node = free_list_head;
    while (curr_node != NULL) {
        if (v_ptr >= curr_node->segment.start_addr &&
            v_ptr < curr_node->segment.start_addr + curr_node->segment.size) {
            // Calculate the physical address offset from the segment's start address
            uintptr_t offset = (uintptr_t) v_ptr - (uintptr_t) curr_node->segment.start_addr;

            // Return the physical address by adding the offset to the segment's physical address
            return (void *) (curr_node->segment.physical_addr + offset);
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
void mems_free(void *v_ptr) {
  // Check if the virtual address is within the MeMS virtual address space
  if ((uintptr_t)v_ptr < mems_virtual_addr) {
    return;
  }

  // Traverse the free list to find the segment before the virtual address
  free_list_node *prev_node = NULL;
  free_list_node *curr_node = free_list_head;
  while (curr_node != NULL) {
    if (v_ptr < curr_node->segment.start_addr) {
      break;
    }
    prev_node = curr_node;
    curr_node = curr_node->next;
  }

  // Check if the virtual address is within the current segment
  if (curr_node != NULL && v_ptr >= curr_node->segment.start_addr &&
      v_ptr < curr_node->segment.start_addr + curr_node->segment.size) {
    // Merge with the previous segment if possible
    if (prev_node != NULL && prev_node->segment.type == HOLE) {
      prev_node->segment.size += curr_node->segment.size;
      prev_node->next = curr_node->next;
      free(curr_node);
      curr_node = prev_node;
    }

    // Merge with the next segment if possible
    if (curr_node->next != NULL && curr_node->next->segment.type == HOLE) {
      curr_node->segment.size += curr_node->next->segment.size;
      free(curr_node->next);
    }

    // Convert the current segment to a HOLE
    curr_node->segment.type = HOLE;
  } else {
    // Create a new HOLE segment for the freed memory
    free_list_node *new_node = malloc(sizeof(free_list_node));
    new_node->segment.start_addr = v_ptr;
    new_node->segment.size = PAGE_SIZE;
    new_node->segment.type = HOLE;

    // Insert the new HOLE segment into the free list
    if (prev_node == NULL) {
      free_list_head = new_node;
      new_node->next = curr_node;
    } else {
      prev_node->next = new_node;
      new_node->next = curr_node;
    }
  }
}
