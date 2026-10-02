/* os9proto.c - probe: does Microware C 3.2's preprocessor skip the
   GCC-only branch of the _OS9PROTO header macro (OS9-COMPAT-DESIGN.md,
   section 4, "Headers for Microware libraries")?  The macro block below
   is copied from there.  Compile with: xcc -a -s -bp os9proto.c
   It must compile without errors, and the calls must come out as K&R
   calls: the first argument in d0, the second in d1, the rest pushed.
   (Microware C 3.2's cpp rejects the error directive even in a skipped
   group, so a GCC without os9call gets a deliberate syntax error instead.)  */

#ifdef __GNUC__
#if defined(__has_attribute)
#if __has_attribute(os9call)
#define _OS9PROTO(p) p __attribute__((os9call))    /* GCC with OS-9 call support */
#endif
#endif
#ifndef _OS9PROTO
#define _OS9PROTO(p) p __os9call_attribute_not_supported_by_this_GCC
#endif
#else
#ifdef __STDC__
#define _OS9PROTO(p) p                             /* other ANSI compilers: see below */
#else
#define _OS9PROTO(p) ()                            /* Microware C 3.2 (K&R) */
#endif
#endif

int create _OS9PROTO((char *name, int mode, int perm, ...));
int _errmsg _OS9PROTO((int nerr, char *msg, ...));

int
probe()
{
	return create("file", 3, 0x1b, 4096) + _errmsg(1, "msg %d", 2);
}
