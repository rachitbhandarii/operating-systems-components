#include "loader.h"
#include <setjmp.h>
#include <signal.h>

Elf32_Ehdr *ehdr = NULL;
Elf32_Phdr *phdr = NULL;
int fd;

int PAGE_SIZE = 4096;  // Page Size : 4 KB
int page_faults = 0;
int page_allocations = 0;
int internal_fragmentation = 0;

void page_fault_handler(int signum, siginfo_t *info, void *context){
  if (signum == SIGSEGV){

    void *fault_addr = info->si_addr;
    page_faults++;

    // iterating in the segments of the Program Header Table
    for (int i=0; i < ehdr->e_phnum; i++){
      void *st_addr = (void *)phdr[i].p_vaddr;
      void *end_addr = (void *)(phdr[i].p_vaddr + phdr[i].p_memsz);

      if (fault_addr >= st_addr && fault_addr <= end_addr) {
        // mapping the virtual addresses from PT_LOAD to the real addresses in mem
        void* mem = mmap(fault_addr,PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
        if (mem == MAP_FAILED){
          perror("Unable to map virtual address to memory");
          exit(1);
        }

        // shifting the pointer to the offset of the segment and then reading the contents to the memory (mem) which we just allocated
        if (lseek(fd,phdr[i].p_offset + ((uintptr_t)fault_addr / PAGE_SIZE * PAGE_SIZE - phdr[i].p_vaddr), SEEK_SET) == -1){
          perror("Can't move the pointer as offset not present.");
          loader_cleanup();
          exit(1);
        }

        if (read(fd,mem,PAGE_SIZE) == -1){
          perror("Can't read the instructions");
          loader_cleanup();
          exit(1);
        }
        if (end_addr - fault_addr < PAGE_SIZE) internal_fragmentation += PAGE_SIZE - (end_addr - fault_addr);
        page_allocations ++;
        return;
      }
    }
  }
}

void setup_sigaction() {
    struct sigaction signal_action; // defining a variable of the sigaction structure.
    signal_action.sa_flags = SA_SIGINFO;
    signal_action.sa_sigaction = page_fault_handler; // telling the variable to handle the signal using the "page_fault_handler" function.
    sigemptyset(&signal_action.sa_mask);
    sigaction(SIGSEGV, &signal_action, NULL); // installing the signal handler.
}

void loader_cleanup() {
  if (ehdr){
    // free up the space used by ehdr
    free(ehdr);
    // make ehdr a null pointer
    ehdr = NULL;
  }
  if (phdr){
    // free up space used by phdr
    free(phdr);
    // make phdr a null pointer
    phdr = NULL;
  }
  close(fd);
}

void load_and_run_elf(char** exe) {

  setup_sigaction();
  // the file descriptor (fd) refers to the opened elf file
  fd = open(exe[1], O_RDONLY);
  if (fd == -1){
    perror("Can't open ELF");
    exit(1);
  }

  // allocating memory to ehdr and then reading the elf's ELF Header to ehdr using fd
  ehdr = (Elf32_Ehdr *)malloc(sizeof(Elf32_Ehdr));
  if (ehdr == NULL){
    perror("Faulty ehdr");
    exit(1);
  }
  if (read(fd, ehdr, sizeof(Elf32_Ehdr)) == -1){
    perror("Can't read ELF Header");
    loader_cleanup();
    exit(1);
  }

  // checking if the file is a valid ELF file
  if (ehdr->e_ident[0] != 0x7f || ehdr->e_ident[1] != 'E' || ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
    perror("Not a valid ELF File");
    loader_cleanup();
    exit(1);
  }

  // as the Program Header Table is at some offset from the start of the ELF File, we move the fd to the offset e_phoff
  if (lseek(fd, ehdr->e_phoff, SEEK_SET) == -1){
    perror("Can't move the pointer as offset not present.");
    loader_cleanup();
    exit(1);
  }

  // allocating memory to phdr and then reading the elf's Program Header Table to phdr using fd
  phdr = (Elf32_Phdr *)malloc(ehdr->e_phnum * ehdr->e_phentsize);
  if (phdr == NULL){
    perror("Faulty ehdr");
    exit(1);
  }
  if (read(fd, phdr, ehdr->e_phnum * ehdr->e_phentsize) == -1){
    perror("Can't read Program Header Table");
    loader_cleanup();
    exit(1);
  }

  // decalred a function pointer _start() which calls the program at e_entry and returns its output which is of the datatype - int
  int (*_start)(void) = (int (*)(void))(ehdr->e_entry);
  int result = _start();
  printf("User _start return value = %d\n",result);
  
  // declaring the final statistics on the number of page faults
  printf("Page Faults: %d\n", page_faults);
  printf("Page Allocations: %d\n", page_allocations);
  printf("Internal Fragmentation: %f KB\n", (double)internal_fragmentation / 1024.0);
    
  //cleaning up after the execution
  loader_cleanup();
}
