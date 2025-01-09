# Simple Smart Loader
This is a Simple Smart Loader that helps mount the Executable file to the memory while handling page faults and run the file and returns the desired output.

## Steps to use:

- Download the zip file
- Extract the file
- Right click and enter terminal
- Execute the following commands:
  
      make
      LD_LIBRARY_PATH=./bin/ ./bin/launch ./test/fib
      LD_LIBRARY_PATH=./bin/ ./bin/launch ./test/sum

## Contribution:

1) Rachit Bhandari
- did memory allocations, memory mapping, and changed the file descriptor positions
- made the necessary MakeFiles, so that the code is compiled the right way
- made all the arithmetic logic behind the page fault handler

2) Akshat Lakhera
- setup the sigaction, and the right method for signal handler, read files and managed the error handling and ELF checking part, so as to perfectly tackle any error while executing the files

## Implementation:

### Loader:

1) The loader mainly uses three pointers, namely: ehdr, phdr and mem; and a file descriptor namely: fd.
2) We use the file descriptor to open the ELF file.
3) We allocate required memory to ehdr and read the ELF header into the memory to which ehdr is pointing, and check whether the file we are reading is an ELF file or not.
4) Then we change the position of fd to the start of the PHT (Program Header Table) and allocate required memory to phdr and read the Program Header Table into the memory to which phdr is pointing. 
5) After that, we declare a function pointer where the function namely: _start calls the process at the entry point of the ELF file and slowly allocates physical memory to it and returns the value we get from that process.
6) Do do that, we declare a Page Fault Handler which gets alarmed by thhe SIGSEGV signal. It iterates in the segments of the PHT, to find the segment in which the fault address is present.
7) In a linked ELF File, the virtual memory addresses of all the text and symbols are fixated. Therefore, we map those virtual memory addresses page by page to the mem pointer using the mmap function.
8) Now, we shift the file descriptor to the start of the fault address and then read the memory allocated to the mem pointer.
9) Then we output that value, the statistics of the page faults and call the loader cleanup, where we deallocate all the memory used up by the pointers.

### Loader's MakeFile:

1) The loader's MakeFile compiles the code present in the Loader into a shared library namely: lib_simpleloader.so inside the bin folder.
2) It also performs the necessary cleanups.

### Launcher:

1) The launcher takes the ELF file as an argument.
2) It uses the shared library to call the loader which further delivers the output of the process implemented in the ELF file.

### Launcher's MakeFile:

1) The launcher's MakeFile compiles the code present by using the shared library namely: lib_simpleloader.so inside the bin folder as the existing library and places the executable inside the bin folder.
2) It also performs the necessary cleanups.

> Credits: Rachit Bhandari - 2023413, Akshat Lakhera - 2023061, Sophomores @IIIT-Delhi.
