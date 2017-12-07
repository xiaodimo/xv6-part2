#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

int
exec(char *path, char **argv)
{

  //changed cs 153 lab 2 part 1 (for debugging only)
  //cprintf("\n\n//////////////////////////////////////////////////\n");
  //cprintf("Enter exec() in exec.c...\n\n");

  // used to see if myproc() had a pgdir, but it comes out negative number
  //cprintf("\n\nmyproc()->pgdir: %d\n\n", myproc()->pgdir);


  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3+MAXARG+1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;
  struct proc *curproc = myproc();

  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    cprintf("exec: fail\n");
    return -1;
  }
  ilock(ip);
  pgdir = 0;

  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pgdir = setupkvm()) == 0)
    goto bad;

  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;



/*
  // below is old notes for way xv6 use to run:
  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz); // round up user code to be a full page
  if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  clearpteu(pgdir, (char*)(sz - 2*PGSIZE));
  sp = sz;
*/

  /////////////////////////
  // changed for cs153 lab2
  
  sp = 0;
  
  //cprintf("\n\nsz before PGROUNDUP and buffer/guard created, only program code loaded...\n");
  //cprintf("sz: %d\n", sz);
  

  sz = PGROUNDUP(sz); // round up user code to be a full page

  //cprintf("\n\nafter sz assignment...\n");
  //cprintf("sz: %d\n", sz);

  // below creates a buffer above the code/text so that we can't grow into 
  // the code of the program
  if((sz = allocuvm(pgdir, sz, sz + PGSIZE)) == 0)
    goto bad;
  clearpteu(pgdir, (char*)(sz - PGSIZE));



  ///////////////////////
  // changed cs 153 lab 2
  //cprintf("\n\nafter allocuvm for sz, before sp assignment...\n");
  //cprintf("sz: %d\n", sz);
  ///cprintf("sp: %d\n", sp);
  //cprintf("STACKBASE: %d\n", STACKBASE);

  sp = STACKBASE; // make stack pointer point to just below the KERNBASE to start

  //cprintf("\n\nafter sp assignment...\n");
  //cprintf("sp: %d\n", sp);
  //cprintf("STACKBASE: %d\n", STACKBASE);
  //cprintf("sp - PGSIZE: %d\n", sp - PGSIZE);

  // now create the first page for the stack
  if((allocuvm(pgdir, sp - PGSIZE, sp)) == 0)
    goto bad;
 

  // changed cs153 lab 2
  curproc->numStackPages = 1; // says we created a page for the stack

  //cprintf("\n\nafter allocuvm for sp and curproc->numStackPages...\n");
  //cprintf("sp: %d\n", sp);
  //cprintf("STACKBASE: %d\n", STACKBASE);
  //cprintf("curproc->numStackPages: %d\n", curproc->numStackPages);


  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[3+argc] = sp;
  }
  ustack[3+argc] = 0;


  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(curproc->name, last, sizeof(curproc->name));


  ///////////////////////
  // changed cs 153 lab 2 (for debugging only)
  //cprintf("\n\nsz: %d\n", sz);
  //cprintf("sp: %d\n\n", sp);



  // Commit to the user image.
  oldpgdir = curproc->pgdir;
  curproc->pgdir = pgdir;
  curproc->sz = sz;
  curproc->tf->eip = elf.entry;  // main
  curproc->tf->esp = sp;
  switchuvm(curproc);
  freevm(oldpgdir);
  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}
