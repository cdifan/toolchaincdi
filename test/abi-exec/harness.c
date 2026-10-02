/* abirun: runs an m68k-elfos9 test image on the Musashi 68000 emulator.

   Level 2 of the OS-9 calling-convention tests (see OS9-COMPAT-DESIGN.md,
   section 11).  The image is a linked ELF file without an operating
   system, laid out like an OS-9 program (os9.lds): code linked at a low
   address and position-independent (-mpcrel), data and bss linked from
   -0x8000 and reached through a6 (-ma6rel).

   The harness loads code at its link address and the data at DATA_BASE,
   points a6 at DATA_BASE + 0x8000, sets up a stack, and runs from the
   entry point.  The image reports through two I/O addresses: a byte
   written to IO_PUTCHAR is printed, and a long written to IO_EXIT ends
   the run with that value as the exit status.  An exception (illegal
   instruction, address error, ...) or running too long fails the run.

   With -m, the file is an OS-9 module instead (elf2mod output): it's
   loaded at MOD_BASE, its initialized data copied to DATA_BASE and its
   initialized data references relocated, as OS-9 does, and a6 points at
   DATA_BASE + 0x8000.

   Usage: abirun [-t] [-c 68040] [-m] file
     -t  trace every instruction
     -c  emulate a 68040 (with FPU, for -m68881 code) instead of a 68000
     -m  the file is an OS-9 module  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "m68k.h"

#define MEM_SIZE    0x1000000		/* the 68000's 24-bit address space */
#define DATA_BASE   0x800000		/* where data linked at -0x8000 goes */
#define MOD_BASE    0x400000		/* where an OS-9 module goes (-m) */
#define STACK_TOP   0xE00000
#define VECTOR_TRAP 0xF00100		/* all exception vectors point here */
#define IO_PUTCHAR  0xF00000
#define IO_EXIT     0xF00004
#define MAX_STEPS   100000000L

static unsigned char mem[MEM_SIZE];
static int done, exit_status, trace;
static unsigned cpu_type = M68K_CPU_TYPE_68000;

/* Data is linked from -0x8000.  Absolute references to it (libgcc's
   soft-float code reads its rounding mode through the GOT) come out as
   0xFF8000 and up in the 24-bit address space; map them onto the data
   area, like a6-relative accesses.  */

static unsigned
map (unsigned address)
{
  address &= MEM_SIZE - 1;
  if (address >= 0xFF8000)
    address = address - 0xFF8000 + DATA_BASE;
  return address;
}

static unsigned
get16 (const unsigned char *p)
{
  return (p[0] << 8) | p[1];
}

static unsigned long
get32 (const unsigned char *p)
{
  return ((unsigned long) p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static void
fail (const char *msg, unsigned address)
{
  if (done)
    return;
  fprintf (stderr, "abirun: %s at 0x%06X (PC 0x%06X)\n", msg, address,
	   m68k_get_reg (NULL, M68K_REG_PPC));
  done = 1;
  exit_status = 99;
  m68k_end_timeslice ();
}

static int
check_io (unsigned address, int write, unsigned value)
{
  if (address < IO_PUTCHAR || address >= VECTOR_TRAP + 0x100)
    return 0;
  if (address >= VECTOR_TRAP)
    {
      fail ("exception", m68k_get_reg (NULL, M68K_REG_PPC));
      return 1;
    }
  if (write && address == IO_PUTCHAR)
    putchar (value & 0xFF);
  else if (write && address == IO_EXIT)
    {
      exit_status = value;
      done = 1;
      m68k_end_timeslice ();
    }
  else
    fail (write ? "bad I/O write" : "bad I/O read", address);
  return 1;
}

unsigned int
m68k_read_memory_8 (unsigned int address)
{
  address = map (address);
  if (check_io (address, 0, 0))
    return 0;
  return mem[address];
}

unsigned int
m68k_read_memory_16 (unsigned int address)
{
  address = map (address);
  if (check_io (address, 0, 0))
    return 0x4E71;			/* nop */
  if (address & 1)
    fail ("odd word read", address);
  return get16 (mem + address);
}

unsigned int
m68k_read_memory_32 (unsigned int address)
{
  address = map (address);
  if (check_io (address, 0, 0))
    return 0;
  if (address & 1)
    fail ("odd long read", address);
  return get32 (mem + address);
}

unsigned int
m68k_read_disassembler_8 (unsigned int address)
{
  return mem[address & (MEM_SIZE - 1)];
}

unsigned int
m68k_read_disassembler_16 (unsigned int address)
{
  return get16 (mem + (address & (MEM_SIZE - 1)));
}

unsigned int
m68k_read_disassembler_32 (unsigned int address)
{
  return get32 (mem + (address & (MEM_SIZE - 1)));
}

void
m68k_write_memory_8 (unsigned int address, unsigned int value)
{
  address = map (address);
  if (check_io (address, 1, value))
    return;
  mem[address] = value;
}

void
m68k_write_memory_16 (unsigned int address, unsigned int value)
{
  address = map (address);
  if (check_io (address, 1, value))
    return;
  if (address & 1)
    fail ("odd word write", address);
  mem[address] = value >> 8;
  mem[address + 1] = value;
}

void
m68k_write_memory_32 (unsigned int address, unsigned int value)
{
  address = map (address);
  if (check_io (address, 1, value))
    return;
  if (address & 1)
    fail ("odd long write", address);
  mem[address] = value >> 24;
  mem[address + 1] = value >> 16;
  mem[address + 2] = value >> 8;
  mem[address + 3] = value;
}

/* Load the PT_LOAD segments of the ELF image FILE; return its entry
   point.  */

static unsigned long
load_elf (const char *file)
{
  FILE *f = fopen (file, "rb");
  unsigned char *img;
  long size;
  unsigned long phoff, entry;
  unsigned phentsize, phnum, i;

  if (f == NULL || fseek (f, 0, SEEK_END) != 0 || (size = ftell (f)) < 52)
    {
      fprintf (stderr, "abirun: can't read %s\n", file);
      exit (2);
    }
  img = malloc (size);
  rewind (f);
  if (fread (img, 1, size, f) != (size_t) size)
    {
      fprintf (stderr, "abirun: can't read %s\n", file);
      exit (2);
    }
  fclose (f);

  if (memcmp (img, "\177ELF\1\2", 6) != 0 || get16 (img + 18) != 4)
    {
      fprintf (stderr, "abirun: %s isn't a 32-bit big-endian m68k ELF file\n",
	       file);
      exit (2);
    }
  entry = get32 (img + 24);
  phoff = get32 (img + 28);
  phentsize = get16 (img + 42);
  phnum = get16 (img + 44);

  for (i = 0; i < phnum; i++)
    {
      const unsigned char *ph = img + phoff + i * phentsize;
      unsigned long offset = get32 (ph + 4), vaddr = get32 (ph + 8);
      unsigned long filesz = get32 (ph + 16), memsz = get32 (ph + 20);
      unsigned long addr;

      if (get32 (ph) != 1)		/* PT_LOAD */
	continue;
      /* Data linked at -0x8000 and up goes to DATA_BASE.  */
      /* Code (executable segments) goes at its address; data, linked from
	 -0x8000, goes to DATA_BASE.  */
      addr = (get32 (ph + 24) & 1) ? vaddr : DATA_BASE + (vaddr + 0x8000UL);
      addr &= 0xFFFFFFFFUL;
      if (addr + memsz > STACK_TOP - 0x10000 || filesz > memsz)
	{
	  fprintf (stderr, "abirun: segment at 0x%lX doesn't fit\n", vaddr);
	  exit (2);
	}
      memcpy (mem + addr, img + offset, filesz);
      memset (mem + addr + filesz, 0, memsz - filesz);
    }
  free (img);
  return entry;
}

/* Read the whole file NAME.  */

static unsigned char *
read_file (const char *name, long *size)
{
  FILE *f = fopen (name, "rb");
  unsigned char *img;

  if (f == NULL || fseek (f, 0, SEEK_END) != 0 || (*size = ftell (f)) < 0)
    {
      fprintf (stderr, "abirun: can't read %s\n", name);
      exit (2);
    }
  img = malloc (*size ? *size : 1);
  rewind (f);
  if (fread (img, 1, *size, f) != (size_t) *size)
    {
      fprintf (stderr, "abirun: can't read %s\n", name);
      exit (2);
    }
  fclose (f);
  return img;
}

/* Apply one list of initialized data references (hi16, count, count
   lo16 offsets, ..., ending with 0,0) at P: add BASE to each long at
   DATA_BASE + offset.  Return the position after the list.  */

static const unsigned char *
relocate_irefs (const unsigned char *p, unsigned long base)
{
  for (;;)
    {
      unsigned hi = get16 (p), n = get16 (p + 2);
      p += 4;
      if (n == 0)
	return p;
      while (n--)
	{
	  unsigned long a = DATA_BASE + ((unsigned long) hi << 16) + get16 (p);
	  unsigned long v = get32 (mem + a) + base;
	  mem[a] = v >> 24;
	  mem[a + 1] = v >> 16;
	  mem[a + 2] = v >> 8;
	  mem[a + 3] = v;
	  p += 2;
	}
    }
}

/* Load the OS-9 module FILE as OS-9 would; return its entry point.  */

static unsigned long
load_module (const char *file)
{
  long size;
  unsigned char *m = read_file (file, &size);
  unsigned long msize, exec, dsize, idata, irefs, off, len;
  const unsigned char *p;

  if (size < 0x48 || get16 (m) != 0x4AFC)
    {
      fprintf (stderr, "abirun: %s isn't an OS-9 module\n", file);
      exit (2);
    }
  msize = get32 (m + 4);
  exec = get32 (m + 0x30);
  dsize = get32 (m + 0x38);
  idata = get32 (m + 0x40);
  irefs = get32 (m + 0x44);
  if (msize > (unsigned long) size || MOD_BASE + msize > DATA_BASE
      || DATA_BASE + dsize > STACK_TOP - 0x10000)
    {
      fprintf (stderr, "abirun: module %s doesn't fit\n", file);
      exit (2);
    }
  memcpy (mem + MOD_BASE, m, msize);
  memset (mem + DATA_BASE, 0, dsize);
  if (idata)
    {
      off = get32 (m + idata);
      len = get32 (m + idata + 4);
      memcpy (mem + DATA_BASE + off, m + idata + 8, len);
    }
  if (irefs)
    {
      p = relocate_irefs (m + irefs, MOD_BASE);	/* code references */
      relocate_irefs (p, DATA_BASE);		/* data references */
    }
  free (m);
  return MOD_BASE + exec;
}

static void
put32 (unsigned address, unsigned long value)
{
  mem[address] = value >> 24;
  mem[address + 1] = value >> 16;
  mem[address + 2] = value >> 8;
  mem[address + 3] = value;
}

int
main (int argc, char **argv)
{
  unsigned long entry;
  long steps = 0;
  int i, module = 0;

  for (;;)
    {
      if (argc > 1 && strcmp (argv[1], "-t") == 0)
	trace = 1;
      else if (argc > 1 && strcmp (argv[1], "-m") == 0)
	module = 1;
      else if (argc > 2 && strcmp (argv[1], "-c") == 0
	       && strcmp (argv[2], "68040") == 0)
	{
	  cpu_type = M68K_CPU_TYPE_68040;
	  argc--, argv++;
	}
      else
	break;
      argc--, argv++;
    }
  if (argc != 2)
    {
      fprintf (stderr, "usage: abirun [-t] [-c 68040] [-m] file\n");
      return 2;
    }

  entry = module ? load_module (argv[1]) : load_elf (argv[1]);
  put32 (0, STACK_TOP);
  put32 (4, entry);
  for (i = 2; i < 256; i++)
    put32 (i * 4, VECTOR_TRAP);

  m68k_init ();
  m68k_set_cpu_type (cpu_type);
  m68k_pulse_reset ();
  m68k_set_reg (M68K_REG_A6, DATA_BASE + 0x8000);

  while (!done && steps < MAX_STEPS)
    {
      if (trace)
	{
	  char buf[100];
	  unsigned pc = m68k_get_reg (NULL, M68K_REG_PC);
	  m68k_disassemble (buf, pc, cpu_type);
	  fprintf (stderr, "%06X  %-30s", pc, buf);
	  for (i = M68K_REG_D0; i <= M68K_REG_A7; i++)
	    fprintf (stderr, " %08X", m68k_get_reg (NULL, (m68k_register_t) i));
	  fprintf (stderr, "\n");
	  steps += m68k_execute (1);
	}
      else
	steps += m68k_execute (10000);
    }
  if (!done)
    {
      fprintf (stderr, "abirun: no exit after %ld cycles\n", steps);
      return 99;
    }
  fflush (stdout);
  if (exit_status != 0)
    {
      /* Tests return a bit mask of failed checks; keep it visible, and
	 nonzero even when the low byte is 0.  */
      fprintf (stderr, "abirun: exit status 0x%X\n", exit_status);
      return (exit_status & 0xFF) != 0 ? exit_status & 0xFF : 1;
    }
  return 0;
}
