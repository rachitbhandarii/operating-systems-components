#include "loader.h"

Elf32_Ehdr *ehdr = NULL;
Elf32_Phdr *phdr = NULL;
int fd;
void *mem = NULL;

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
  if (mem){
    // make mem a null pointer
    mem = NULL;
  }
  close(fd);
}

void load_and_run_elf(char** exe) {
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
  if (ehdr->e_ident[0] != 0x7f && ehdr->e_ident[1] != 'E' && ehdr->e_ident[2] != 'L' && ehdr->e_ident[3] != 'F') {
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
  if (ehdr == NULL){
    perror("Faulty ehdr");
    exit(1);
  }
  if (read(fd, phdr, ehdr->e_phnum * ehdr->e_phentsize) == -1){
    perror("Can't read Program Header Table");
    loader_cleanup();
    exit(1);
  }

  // iterating in the segments of the Program Header Table
  for (int i=0; i < ehdr->e_phnum; i++){
    // finding the segment PT_LOAD
    if (phdr[i].p_type == PT_LOAD){
      // mapping the virtual addresses from PT_LOAD to the real addresses in mem
      mem = mmap((void *) phdr[i].p_vaddr, phdr[i].p_memsz, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
      if (mem == MAP_FAILED){
        perror("Unable to map virtual address to memory");
        exit(1);
      }

      // shifting the pointer to the offset at which PT_LOAD segment is present and then reading the contents of PT_LOAD to the memory (mem) which we just allocated
      if (lseek(fd, phdr[i].p_offset, SEEK_SET) == -1){
        perror("Can't move the pointer as offset not present.");
        loader_cleanup();
        exit(1);
      }
      if (read(fd, mem, phdr[i].p_memsz) == -1){
        perror("Can't read the instructions");
        loader_cleanup();
        exit(1);
      }
    }
  }

  // decalred a function pointer _start() which calls the program at e_entry and returns its output which is of the datatype - int
  int (*_start)(void) = (int (*)(void))(ehdr->e_entry);
  int result = _start();
  printf("User _start return value = %d\n",result);

  //cleaning up after the execution
  loader_cleanup();
}

int main(int argc, char** argv) {
  if(argc != 2) {
    printf("Usage: %s <ELF Executable> \n",argv[0]);
    exit(1);
  }
  load_and_run_elf(argv);
  loader_cleanup();
  return 0;
}
