/* Compiled by KLIC compiler version 3.01 (Sun, 10 Apr 2016 19:09:11 +0900) */
#include <klic/klichdr.h>
#include "atom.h"
#include "funct.h"

#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#ifdef USESELECT
#include <sys/select.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/times.h>
#include <klic/gobj.h>
#include <klic/g_pointer.h>
#include <klic/gd_macro.h>
#ifdef USEULIMIT
#include <ulimit.h>
#endif
#ifdef USESIG
#include <signal.h>
#endif

#ifdef ASYNCIO
extern void init_asynchronous_io();
extern void register_asynchronous_io_stream(long fd, q stream);
extern void close_asynchronous_io_stream(long fd);
#ifndef FASYNC
#include <fcntl.h>
#ifndef USESTREAMINCLUDEDIR
#include <stropts.h>
#endif
#endif
#endif

extern int poll_read_available(int fd);

#ifndef O_NONBLOCK
#define O_NONBLOCK FNDELAY
#endif

extern char *malloc_check();
GD_USE_CLASS(pointer);
q *register_streamed_signal();

extern char *generic_string_body();
extern char *convert_klic_string_to_c_string();
extern q convert_c_string_to_klic_string();
extern q convert_binary_c_string_to_klic_string();

#define KLIC2C convert_klic_string_to_c_string
#define C2KLIC convert_c_string_to_klic_string
#define BC2KLIC convert_binary_c_string_to_klic_string

extern q gd_new_pointer();
#define FilePointer(x) \
  ((FILE*)((struct pointer_object *)(data_objectp(x)))->pointer)

#define MakeFilePointer(klicvar,file) \
{ \
  klicvar = gd_new_pointer((q)file, allocp); \
  allocp = heapp; \
}

#define Fopen(P,F,L,M) \
{ \
  char *path = KLIC2C(P); \
  FILE *file = fopen(path, M); \
  free(path); \
  if (file==NULL) goto L; \
  MakeFilePointer(F, file); \
}

#define Fdopen(Fd,F,L,M) \
{ \
  FILE *file = fdopen(Fd, M); \
  if (file==NULL) goto L; \
  MakeFilePointer(F, file); \
}

struct iobuf {
  unsigned char *ptr, *lim, *buf;
  int fd;
  int bufsize;
};

static struct iobuf *make_iobuf(fd, size, isout)
     int fd, size, isout;
{
  struct iobuf *iob = (struct iobuf *)malloc_check(sizeof(struct iobuf));
  iob->fd = fd;
  iob->bufsize = size;
  iob->buf = (unsigned char *)malloc_check(iob->bufsize);
  iob->ptr = iob->buf;
  if (isout) {
    iob->lim = iob->buf+size;
  } else {
    iob->lim = iob->buf;
  }
  return iob;
}

struct biobuf {
  struct iobuf ibuf;
  struct iobuf obuf;
};

static struct biobuf *make_biobuf(ifd, ofd, size)
     int ifd, ofd, size;
{
  struct biobuf *biob = (struct biobuf *)malloc_check(sizeof(struct biobuf));
  struct iobuf *ibuf = &(biob->ibuf);
  struct iobuf *obuf = &(biob->obuf);

  ibuf->fd = ifd;
  ibuf->bufsize = size;
  ibuf->buf = (unsigned char *)malloc_check(ibuf->bufsize);
  ibuf->ptr = ibuf->buf;
  ibuf->lim = ibuf->buf;

  obuf->fd = ofd;
  obuf->bufsize = size;
  obuf->buf = (unsigned char *)malloc_check(obuf->bufsize);
  obuf->ptr = obuf->buf;
  obuf->lim = obuf->buf+size;

  return biob;
}

#define MakeInBuf(klicvar, fd, size) \
{ \
  klicvar = gd_new_pointer((q)make_iobuf(fd,size,0), allocp); \
  allocp = heapp; \
}

#define MakeOutBuf(klicvar, fd, size) \
{ \
  klicvar = gd_new_pointer((q)make_iobuf(fd,size,1), allocp); \
  allocp = heapp; \
}

#define IOBuf(x) \
  ((struct iobuf*)((struct pointer_object *)(data_objectp(x)))->pointer)

#define MakeBIOBuf(klicvar, ifd, ofd, size) \
{ \
  klicvar = gd_new_pointer((q)make_biobuf(ifd,ofd,size), allocp); \
  allocp = heapp; \
}

#define BIOBuf(x) \
  ((struct biobuf*)((struct pointer_object *)(data_objectp(x)))->pointer)

static int fill_buf(iob)
     struct iobuf *iob;
{
  while (1) {
    int result = read(iob->fd, iob->buf, iob->bufsize);
    if (result < 0) {
      switch (errno) {
      case EINTR: continue;
#ifdef EPIPE
      case EPIPE:
        return -2;
#endif /* EPIPE */
#ifdef ASYNCIO
      case EAGAIN:
#ifdef EWOULDBLOCK
#if EWOULDBLOCK != EAGAIN
      case EWOULDBLOCK:
#endif
#endif
	return -1;
#endif
      default:
	fatalp("read", "Error in asynchronous input");
      }
    } else if (result == 0) {
      return 0;			/* end of file */
    } else {
      iob->lim = iob->buf+result;
      iob->ptr = iob->buf;
      return 1;
    }
  }
}

static int write_buf(iob)
     struct iobuf *iob;
{
  unsigned char *wp = iob->buf;
  while (1) {
    int result = write(iob->fd, wp, (iob->ptr - wp));
    if (result < 0) {
      switch (errno) {
      case EINTR: continue;
#ifdef EPIPE
      case EPIPE:
        return -2;
#endif /* EPIPE */
#ifdef ASYNCIO
      case EAGAIN:
#ifdef EWOULDBLOCK
#if EWOULDBLOCK != EAGAIN
      case EWOULDBLOCK:
#endif
#endif

/*
  Patch to detour a suspected bug of SunOS 4.1.3
	(1994/12/02 Takashi Chikayama)

  As SIGIO signal is not raised during sigpause for socket output,
  we will use polling instead of signal-based I/O.

  The original code was the following.

	return -1;
*/

/* Begin Patch */
	{
	  fd_set fdsw;
	  int fd_setsize;
#ifdef USEGETDTABLESIZE
	  fd_setsize = getdtablesize();
#else
#ifdef USEULIMIT
	  fd_setsize = ulimit(4, 0);
	  if (fd_setsize < 0) {
	    fatal("Can't obtain file descriptor table size");
	  }
#else
	  fatal("Don't know how to obtaine file descriptor table size");
#endif
#endif
	  FD_ZERO(&fdsw);
	  FD_SET(iob->fd, &fdsw);
	  select(fd_setsize, 0, &fdsw, 0, 0);
	}
	continue;
/* End of Patch */
#endif
      default:
	fatalp("write", "Error in asynchronous output");
      }
    } else {
      wp += result;
      if (wp != iob->ptr) {
	continue;
      } else {
	iob->lim = iob->buf + iob->bufsize;
	iob->ptr = iob->buf;
	return 1;
      }
    }
  }
}

static void setasync(sock, msg)
     int sock;
     char *msg;
{
#ifdef ASYNCIO
  if (fcntl(sock, F_SETOWN, getpid()) < -1) {
    fatalp("fcntl", "Setting error for %s", msg);
  }
#ifdef FASYNC
  if (fcntl(sock, F_SETFL, FASYNC|O_NONBLOCK) < -1) {
    fatalp("fcntl", "Setting error for %s", msg);
  }
#else
  if (fcntl(sock, F_SETFL, O_NONBLOCK) < -1) {
    fatalp("fcntl", "Setting error for %s", msg);
  }
  if (ioctl(sock, I_SETSIG, S_INPUT|S_OUTPUT) != 0) {
    fatalp("ioctl", "Setting error for %s", msg);
  }
#endif
#endif
}


module module_unix();
Const struct predicate predicate_unix_xexit_1 =
   { module_unix, 0, 1 };
Const struct predicate predicate_unix_xunix_1 =
   { module_unix, 1, 1 };
Const struct predicate predicate_unix_xcont_3 =
   { module_unix, 2, 3 };
Const struct predicate predicate_unix_xstdin_1 =
   { module_unix, 3, 1 };
Const struct predicate predicate_unix_xstdout_1 =
   { module_unix, 4, 1 };
Const struct predicate predicate_unix_xstderr_1 =
   { module_unix, 5, 1 };
Const struct predicate predicate_unix_xstdio_1 =
   { module_unix, 6, 1 };
Const struct predicate predicate_unix_xread__open_2 =
   { module_unix, 7, 2 };
Const struct predicate predicate_unix_xwrite__open_2 =
   { module_unix, 8, 2 };
Const struct predicate predicate_unix_xappend__open_2 =
   { module_unix, 9, 2 };
Const struct predicate predicate_unix_xupdate__open_2 =
   { module_unix, 10, 2 };
Const struct predicate predicate_unix_xsignal__stream_2 =
   { module_unix, 11, 2 };
Const struct predicate predicate_unix_xsignal__stream_3 =
   { module_unix, 12, 3 };
Const struct predicate predicate_unix_xnet__convert_3 =
   { module_unix, 13, 3 };
Const struct predicate predicate_unix_xconnect_2 =
   { module_unix, 14, 2 };
Const struct predicate predicate_unix_xconnect_4 =
   { module_unix, 15, 4 };
Const struct predicate predicate_unix_xconnect__sub_4 =
   { module_unix, 16, 4 };
Const struct predicate predicate_unix_xconnect__sub_5 =
   { module_unix, 17, 5 };
Const struct predicate predicate_unix_xconnect2_2 =
   { module_unix, 18, 2 };
Const struct predicate predicate_unix_xconnect2_4 =
   { module_unix, 19, 4 };
Const struct predicate predicate_unix_xconnect2__sub_4 =
   { module_unix, 20, 4 };
Const struct predicate predicate_unix_xconnect2__sub_5 =
   { module_unix, 21, 5 };
Const struct predicate predicate_unix_xbind_2 =
   { module_unix, 22, 2 };
Const struct predicate predicate_unix_xbind_4 =
   { module_unix, 23, 4 };
Const struct predicate predicate_unix_xbound__sock_4 =
   { module_unix, 24, 4 };
Const struct predicate predicate_unix_xpipe_1 =
   { module_unix, 25, 1 };
Const struct predicate predicate_unix_xasync__io_5 =
   { module_unix, 26, 5 };
Const struct predicate predicate_unix_xasync__input_4 =
   { module_unix, 27, 4 };
Const struct predicate predicate_unix_xasync__output_4 =
   { module_unix, 28, 4 };
Const struct predicate predicate_unix_xsystem_2 =
   { module_unix, 29, 2 };
Const struct predicate predicate_unix_xcd_2 =
   { module_unix, 30, 2 };
Const struct predicate predicate_unix_xunlink_2 =
   { module_unix, 31, 2 };
Const struct predicate predicate_unix_xmktemp_2 =
   { module_unix, 32, 2 };
Const struct predicate predicate_unix_xaccess_3 =
   { module_unix, 33, 3 };
Const struct predicate predicate_unix_xchmod_3 =
   { module_unix, 34, 3 };
Const struct predicate predicate_unix_xumask_1 =
   { module_unix, 35, 1 };
Const struct predicate predicate_unix_xumask_2 =
   { module_unix, 36, 2 };
Const struct predicate predicate_unix_xgetenv_2 =
   { module_unix, 37, 2 };
Const struct predicate predicate_unix_xputenv_2 =
   { module_unix, 38, 2 };
Const struct predicate predicate_unix_xkill_3 =
   { module_unix, 39, 3 };
Const struct predicate predicate_unix_xfork_1 =
   { module_unix, 40, 1 };
Const struct predicate predicate_unix_xfork__with__pipes_1 =
   { module_unix, 41, 1 };
Const struct predicate predicate_unix_xfork__with__pipes_2F1_240_6 =
   { module_unix, 42, 6 };
Const struct predicate predicate_unix_xargc_1 =
   { module_unix, 43, 1 };
Const struct predicate predicate_unix_xargv_1 =
   { module_unix, 44, 1 };
Const struct predicate predicate_unix_xmake__argv__list_3 =
   { module_unix, 45, 3 };
Const struct predicate predicate_unix_xtimes_4 =
   { module_unix, 46, 4 };
Const struct predicate predicate_unix_xdummy_0 =
   { module_unix, 47, 0 };
extern q file__io_g_new();
extern Const struct predicate predicate_timer_xinstantiate__after_2;
declare_method_table_of(vector);
declare_method_table_of(byte__string);
declare_method_table_of(pointer);

module module_unix(glbl, qp, allocp, toppred)
  struct global_variables *glbl;
  struct goalrec *qp;
  register q *allocp;
  Const struct predicate *toppred;
{
  static Const q vectconst_body_0[] = {
    makeint(0),
    makeint(0),
    makeint(0),
    makeint(0),
  };
  declare_method_table_of(vector);
  static Const vector_structure_type vector_const_0 =
      declare_vector_constant(vectconst_body_0,4);
  declare_method_table_of(byte__string);
  static Const string_structure_type_8 string_const_1 =
      declare_string_constant_8(0,0);
  static Const unsigned char strconst_body_2[12] =
    "pipe-input";
  declare_method_table_of(byte__string);
  static Const string_structure_type_8 string_const_2 =
      declare_string_constant_8(strconst_body_2,10);
  static Const unsigned char strconst_body_3[12] =
    "pipe-output";
  declare_method_table_of(byte__string);
  static Const string_structure_type_8 string_const_3 =
      declare_string_constant_8(strconst_body_3,11);
  static Const unsigned char strconst_body_4[8] =
    "stdin";
  declare_method_table_of(byte__string);
  static Const string_structure_type_8 string_const_4 =
      declare_string_constant_8(strconst_body_4,5);
  static Const unsigned char strconst_body_5[8] =
    "stdout";
  declare_method_table_of(byte__string);
  static Const string_structure_type_8 string_const_5 =
      declare_string_constant_8(strconst_body_5,6);
  static Const q funct_const_6[] = {
    makesym(functor_time_3),
    makeint(0),
    makeint(3),
    makeint(0),
  };
  q a0, a1, a2, a3, a4, a5;

  q *reasonp;
 module_top:
  switch_on_pred() {
    case_pred(0, exit_1_top);
    case_pred(1, unix_1_top);
    case_pred(2, cont_3_top);
    case_pred(3, stdin_1_top);
    case_pred(4, stdout_1_top);
    case_pred(5, stderr_1_top);
    case_pred(6, stdio_1_top);
    case_pred(7, read__open_2_top);
    case_pred(8, write__open_2_top);
    case_pred(9, append__open_2_top);
    case_pred(10, update__open_2_top);
    case_pred(11, signal__stream_2_top);
    case_pred(12, signal__stream_3_top);
    case_pred(13, net__convert_3_top);
    case_pred(14, connect_2_top);
    case_pred(15, connect_4_top);
    case_pred(16, connect__sub_4_top);
    case_pred(17, connect__sub_5_top);
    case_pred(18, connect2_2_top);
    case_pred(19, connect2_4_top);
    case_pred(20, connect2__sub_4_top);
    case_pred(21, connect2__sub_5_top);
    case_pred(22, bind_2_top);
    case_pred(23, bind_4_top);
    case_pred(24, bound__sock_4_top);
    case_pred(25, pipe_1_top);
    case_pred(26, async__io_5_top);
    case_pred(27, async__input_4_top);
    case_pred(28, async__output_4_top);
    case_pred(29, system_2_top);
    case_pred(30, cd_2_top);
    case_pred(31, unlink_2_top);
    case_pred(32, mktemp_2_top);
    case_pred(33, access_3_top);
    case_pred(34, chmod_3_top);
    case_pred(35, umask_1_top);
    case_pred(36, umask_2_top);
    case_pred(37, getenv_2_top);
    case_pred(38, putenv_2_top);
    case_pred(39, kill_3_top);
    case_pred(40, fork_1_top);
    case_pred(41, fork__with__pipes_1_top);
    case_pred(42, fork__with__pipes_2F1_240_6_top);
    case_pred(43, argc_1_top);
    case_pred(44, argv_1_top);
    case_pred(45, make__argv__list_3_top);
    case_pred(46, times_4_top);
    last_case_pred(47, dummy_0_top);
  }

 exit_1_top: {
  a0 = qp->args[0];
  qp = qp->next;
 exit_1_clear_reason:
  reasonp = reasons;
 exit_1_0:
 exit_1_1:
  if (!isint(a0)) goto exit_1_2;
  
extern void klic_exit();
klic_exit((intval(a0)));

  proceed();
 exit_1_2:
  if (!isref(a0)) goto exit_1_interrupt;
  deref_and_jump(a0,exit_1_1);
  *reasonp++ =  a0;
  goto exit_1_interrupt;
 exit_1_ext_interrupt:
  reasonp = 0l;
 exit_1_interrupt:
  goto interrupt_1;
 }

 unix_1_top: {
  q x0, x1, x2, x3;
  a0 = qp->args[0];
  qp = qp->next;
 unix_1_clear_reason:
  reasonp = reasons;
 unix_1_0:
 unix_1_1:
  switch (ptagof(a0)) {
 case CONS:
  x0 = car_of(a0);
 unix_1_2:
  switch (ptagof(x0)) {
 case FUNCTOR:
  switch (symval(functor_of(x0))) {
 case functor_stdin_1:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 0);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(stdin_1_0);
  goto stdin_1_ext_interrupt;
 case functor_stdout_1:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 0);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(stdout_1_0);
  goto stdout_1_ext_interrupt;
 case functor_stderr_1:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 0);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(stderr_1_0);
  goto stderr_1_ext_interrupt;
 case functor_stdio_1:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 0);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(stdio_1_0);
  goto stdio_1_ext_interrupt;
 case functor_read__open_2:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 1);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = arg(x0, 0);
  a1 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(read__open_2_0);
  goto read__open_2_ext_interrupt;
 case functor_write__open_2:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 1);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = arg(x0, 0);
  a1 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(write__open_2_0);
  goto write__open_2_ext_interrupt;
 case functor_append__open_2:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 1);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = arg(x0, 0);
  a1 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(append__open_2_0);
  goto append__open_2_ext_interrupt;
 case functor_update__open_2:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 1);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = arg(x0, 0);
  a1 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(update__open_2_0);
  goto update__open_2_ext_interrupt;
 case functor_signal__stream_2:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 1);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = arg(x0, 0);
  a1 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(signal__stream_2_0);
  goto signal__stream_2_ext_interrupt;
 case functor_connect_2:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 1);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = arg(x0, 0);
  a1 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(connect_2_0);
  goto connect_2_ext_interrupt;
 case functor_connect2_2:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 1);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = arg(x0, 0);
  a1 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(connect2_2_0);
  goto connect2_2_ext_interrupt;
 case functor_bind_2:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 1);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = arg(x0, 0);
  a1 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(bind_2_0);
  goto bind_2_ext_interrupt;
 case functor_pipe_1:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 0);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(pipe_1_0);
  goto pipe_1_ext_interrupt;
 case functor_system_2:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 1);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = arg(x0, 0);
  a1 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(system_2_0);
  goto system_2_ext_interrupt;
 case functor_cd_2:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 1);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = arg(x0, 0);
  a1 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(cd_2_0);
  goto cd_2_ext_interrupt;
 case functor_unlink_2:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 1);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = arg(x0, 0);
  a1 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(unlink_2_0);
  goto unlink_2_ext_interrupt;
 case functor_mktemp_2:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 1);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = arg(x0, 0);
  a1 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(mktemp_2_0);
  goto mktemp_2_ext_interrupt;
 case functor_access_3:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 2);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = arg(x0, 0);
  a1 = arg(x0, 1);
  a2 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(access_3_0);
  goto access_3_ext_interrupt;
 case functor_chmod_3:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 2);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = arg(x0, 0);
  a1 = arg(x0, 1);
  a2 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(chmod_3_0);
  goto chmod_3_ext_interrupt;
 case functor_umask_1:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 0);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(umask_1_0);
  goto umask_1_ext_interrupt;
 case functor_umask_2:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 1);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = arg(x0, 0);
  a1 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(umask_2_0);
  goto umask_2_ext_interrupt;
 case functor_getenv_2:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 1);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = arg(x0, 0);
  a1 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(getenv_2_0);
  goto getenv_2_ext_interrupt;
 case functor_putenv_2:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 1);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = arg(x0, 0);
  a1 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(putenv_2_0);
  goto putenv_2_ext_interrupt;
 case functor_kill_3:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 2);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = arg(x0, 0);
  a1 = arg(x0, 1);
  a2 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(kill_3_0);
  goto kill_3_ext_interrupt;
 case functor_fork_1:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 0);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(fork_1_0);
  goto fork_1_ext_interrupt;
 case functor_fork__with__pipes_1:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xcont_3);
  allocp[2] = x1 = makeref(&allocp[2]);
  x2 = arg(x0, 0);
  allocp[3] = x2;
  x3 = cdr_of(a0);
  allocp[4] = x3;
  a0 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 5;
  execute(fork__with__pipes_1_0);
  goto fork__with__pipes_1_ext_interrupt;
 case functor_sync_1:
  x1 = arg(x0, 0);
  x2 = makeint(0L);
  unify_value(x1, x2);
  a0 = cdr_of(a0);
  execute(unix_1_0);
  goto unix_1_ext_interrupt;
  };
  goto unix_1_interrupt;
 case VARREF:
  deref_and_jump(x0,unix_1_2);
  *reasonp++ =  x0;
  goto unix_1_interrupt;
  };
  goto unix_1_interrupt;
 case ATOMIC:
  if (a0 != NILATOM) goto unix_1_interrupt;
  proceed();
 case VARREF:
  deref_and_jump(a0,unix_1_1);
  *reasonp++ =  a0;
  goto unix_1_interrupt;
  };
  goto unix_1_interrupt;
 unix_1_ext_interrupt:
  reasonp = 0l;
 unix_1_interrupt:
  toppred = &predicate_unix_xunix_1;
  goto interrupt_1;
 }

 cont_3_top: {
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  qp = qp->next;
 cont_3_clear_reason:
  reasonp = reasons;
 cont_3_0:
 cont_3_1:
  if (isref(a0)) goto cont_3_2;
  gblt_wait(a0,cont_3_interrupt);
  unify_value(a1, a0);
  a0 = a2;
  execute(unix_1_0);
  goto unix_1_ext_interrupt;
 cont_3_2:
  deref_and_jump(a0,cont_3_1);
  *reasonp++ =  a0;
  goto cont_3_interrupt;
 cont_3_ext_interrupt:
  reasonp = 0l;
 cont_3_interrupt:
  goto interrupt_3;
 }

 stdin_1_top: {
  q x0, x1, x2, x3, x4, x5;
  a0 = qp->args[0];
  qp = qp->next;
 stdin_1_clear_reason:
  reasonp = reasons;
 stdin_1_0:
  MakeFilePointer(x0, stdin);
  generic_arg[0] = x0;
  x1 = makefunctor(&string_const_4);
  generic_arg[1] = x1;
  x2 = NILATOM;
  generic_arg[2] = x2;
  x3 = makefunctor(&string_const_1);
  generic_arg[3] = x3;
  new_generic(file__io_g_new, 4, x4, 0);
  allocp[0] = makesym(functor_normal_1);
  allocp[1] = x4;
  x5 = makefunctor(&allocp[0]);
  allocp += 2;
  unify_value(a0, x5);
  proceed();
 stdin_1_ext_interrupt:
  reasonp = 0l;
 stdin_1_interrupt:
  toppred = &predicate_unix_xstdin_1;
  goto interrupt_1;
 }

 stdout_1_top: {
  q x0, x1, x2, x3, x4, x5;
  a0 = qp->args[0];
  qp = qp->next;
 stdout_1_clear_reason:
  reasonp = reasons;
 stdout_1_0:
  MakeFilePointer(x0, stdout);
  x1 = NILATOM;
  generic_arg[0] = x1;
  x2 = makefunctor(&string_const_1);
  generic_arg[1] = x2;
  generic_arg[2] = x0;
  x3 = makefunctor(&string_const_5);
  generic_arg[3] = x3;
  new_generic(file__io_g_new, 4, x4, 0);
  allocp[0] = makesym(functor_normal_1);
  allocp[1] = x4;
  x5 = makefunctor(&allocp[0]);
  allocp += 2;
  unify_value(a0, x5);
  proceed();
 stdout_1_ext_interrupt:
  reasonp = 0l;
 stdout_1_interrupt:
  toppred = &predicate_unix_xstdout_1;
  goto interrupt_1;
 }

 stderr_1_top: {
  q x0, x1, x2, x3, x4, x5;
  a0 = qp->args[0];
  qp = qp->next;
 stderr_1_clear_reason:
  reasonp = reasons;
 stderr_1_0:
  MakeFilePointer(x0, stderr);
  x1 = NILATOM;
  generic_arg[0] = x1;
  x2 = makefunctor(&string_const_1);
  generic_arg[1] = x2;
  generic_arg[2] = x0;
  x3 = makefunctor(&string_const_5);
  generic_arg[3] = x3;
  new_generic(file__io_g_new, 4, x4, 0);
  allocp[0] = makesym(functor_normal_1);
  allocp[1] = x4;
  x5 = makefunctor(&allocp[0]);
  allocp += 2;
  unify_value(a0, x5);
  proceed();
 stderr_1_ext_interrupt:
  reasonp = 0l;
 stderr_1_interrupt:
  toppred = &predicate_unix_xstderr_1;
  goto interrupt_1;
 }

 stdio_1_top: {
  q x0, x1, x2, x3, x4, x5;
  a0 = qp->args[0];
  qp = qp->next;
 stdio_1_clear_reason:
  reasonp = reasons;
 stdio_1_0:
  
{
  MakeFilePointer(x0, stdin);
  MakeFilePointer(x1, stdout);
}
  generic_arg[0] = x0;
  x2 = makefunctor(&string_const_4);
  generic_arg[1] = x2;
  generic_arg[2] = x1;
  x3 = makefunctor(&string_const_5);
  generic_arg[3] = x3;
  new_generic(file__io_g_new, 4, x4, 0);
  allocp[0] = makesym(functor_normal_1);
  allocp[1] = x4;
  x5 = makefunctor(&allocp[0]);
  allocp += 2;
  unify_value(a0, x5);
  proceed();
 stdio_1_ext_interrupt:
  reasonp = 0l;
 stdio_1_interrupt:
  toppred = &predicate_unix_xstdio_1;
  goto interrupt_1;
 }

 read__open_2_top: {
  q x0, x1, x2, x3, x4;
  a0 = qp->args[0];
  a1 = qp->args[1];
  qp = qp->next;
 read__open_2_clear_reason:
  reasonp = reasons;
 read__open_2_0:
 read__open_2_1:
  switch (ptagof(a0)) {
 case FUNCTOR:
 read__open_2_2:
  if (!isgobj(a0)) goto read__open_2_3;
  if (!isclass(a0,byte__string)) goto read__open_2_3;
  Fopen(a0,x0,read__open_2_3,"r");
  generic_arg[0] = x0;
  generic_arg[1] = a0;
  x1 = NILATOM;
  generic_arg[2] = x1;
  x2 = makefunctor(&string_const_1);
  generic_arg[3] = x2;
  new_generic(file__io_g_new, 4, x3, 0);
  allocp[0] = makesym(functor_normal_1);
  allocp[1] = x3;
  x4 = makefunctor(&allocp[0]);
  allocp += 2;
  unify_value(a1, x4);
  proceed();
 case VARREF:
  deref_and_jump(a0,read__open_2_1);
  *reasonp++ =  a0;
  goto read__open_2_3;
  };
  goto read__open_2_3;
 read__open_2_3:
  otherwise(read__open_2_interrupt);
  x0 = makesym(atom_abnormal);
  unify_value(a1, x0);
  proceed();
 read__open_2_ext_interrupt:
  reasonp = 0l;
 read__open_2_interrupt:
  toppred = &predicate_unix_xread__open_2;
  goto interrupt_2;
 }

 write__open_2_top: {
  q x0, x1, x2, x3, x4;
  a0 = qp->args[0];
  a1 = qp->args[1];
  qp = qp->next;
 write__open_2_clear_reason:
  reasonp = reasons;
 write__open_2_0:
 write__open_2_1:
  switch (ptagof(a0)) {
 case FUNCTOR:
 write__open_2_2:
  if (!isgobj(a0)) goto write__open_2_3;
  if (!isclass(a0,byte__string)) goto write__open_2_3;
  Fopen(a0,x0,write__open_2_3,"w");
  x1 = NILATOM;
  generic_arg[0] = x1;
  x2 = makefunctor(&string_const_1);
  generic_arg[1] = x2;
  generic_arg[2] = x0;
  generic_arg[3] = a0;
  new_generic(file__io_g_new, 4, x3, 0);
  allocp[0] = makesym(functor_normal_1);
  allocp[1] = x3;
  x4 = makefunctor(&allocp[0]);
  allocp += 2;
  unify_value(a1, x4);
  proceed();
 case VARREF:
  deref_and_jump(a0,write__open_2_1);
  *reasonp++ =  a0;
  goto write__open_2_3;
  };
  goto write__open_2_3;
 write__open_2_3:
  otherwise(write__open_2_interrupt);
  x0 = makesym(atom_abnormal);
  unify_value(a1, x0);
  proceed();
 write__open_2_ext_interrupt:
  reasonp = 0l;
 write__open_2_interrupt:
  toppred = &predicate_unix_xwrite__open_2;
  goto interrupt_2;
 }

 append__open_2_top: {
  q x0, x1, x2, x3, x4;
  a0 = qp->args[0];
  a1 = qp->args[1];
  qp = qp->next;
 append__open_2_clear_reason:
  reasonp = reasons;
 append__open_2_0:
 append__open_2_1:
  switch (ptagof(a0)) {
 case FUNCTOR:
 append__open_2_2:
  if (!isgobj(a0)) goto append__open_2_3;
  if (!isclass(a0,byte__string)) goto append__open_2_3;
  Fopen(a0,x0,append__open_2_3,"a");
  x1 = NILATOM;
  generic_arg[0] = x1;
  x2 = makefunctor(&string_const_1);
  generic_arg[1] = x2;
  generic_arg[2] = x0;
  generic_arg[3] = a0;
  new_generic(file__io_g_new, 4, x3, 0);
  allocp[0] = makesym(functor_normal_1);
  allocp[1] = x3;
  x4 = makefunctor(&allocp[0]);
  allocp += 2;
  unify_value(a1, x4);
  proceed();
 case VARREF:
  deref_and_jump(a0,append__open_2_1);
  *reasonp++ =  a0;
  goto append__open_2_3;
  };
  goto append__open_2_3;
 append__open_2_3:
  otherwise(append__open_2_interrupt);
  x0 = makesym(atom_abnormal);
  unify_value(a1, x0);
  proceed();
 append__open_2_ext_interrupt:
  reasonp = 0l;
 append__open_2_interrupt:
  toppred = &predicate_unix_xappend__open_2;
  goto interrupt_2;
 }

 update__open_2_top: {
  q x0, x1, x2;
  a0 = qp->args[0];
  a1 = qp->args[1];
  qp = qp->next;
 update__open_2_clear_reason:
  reasonp = reasons;
 update__open_2_0:
 update__open_2_1:
  switch (ptagof(a0)) {
 case FUNCTOR:
 update__open_2_2:
  if (!isgobj(a0)) goto update__open_2_3;
  if (!isclass(a0,byte__string)) goto update__open_2_3;
  Fopen(a0,x0,update__open_2_3,"r+");
  generic_arg[0] = x0;
  generic_arg[1] = a0;
  generic_arg[2] = x0;
  generic_arg[3] = a0;
  new_generic(file__io_g_new, 4, x1, 0);
  allocp[0] = makesym(functor_normal_1);
  allocp[1] = x1;
  x2 = makefunctor(&allocp[0]);
  allocp += 2;
  unify_value(a1, x2);
  proceed();
 case VARREF:
  deref_and_jump(a0,update__open_2_1);
  *reasonp++ =  a0;
  goto update__open_2_3;
  };
  goto update__open_2_3;
 update__open_2_3:
  otherwise(update__open_2_interrupt);
  x0 = makesym(atom_abnormal);
  unify_value(a1, x0);
  proceed();
 update__open_2_ext_interrupt:
  reasonp = 0l;
 update__open_2_interrupt:
  toppred = &predicate_unix_xupdate__open_2;
  goto interrupt_2;
 }

 signal__stream_2_top: {
  q x0;
  a0 = qp->args[0];
  a1 = qp->args[1];
  qp = qp->next;
 signal__stream_2_clear_reason:
  reasonp = reasons;
 signal__stream_2_0:
  allocp[0] = x0 = makeref(&allocp[0]);
  a2 = x0;
  allocp += 1;
  execute(signal__stream_3_0);
  goto signal__stream_3_ext_interrupt;
 signal__stream_2_ext_interrupt:
  reasonp = 0l;
 signal__stream_2_interrupt:
  toppred = &predicate_unix_xsignal__stream_2;
  goto interrupt_2;
 }

 signal__stream_3_top: {
  q x0;
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  qp = qp->next;
 signal__stream_3_clear_reason:
  reasonp = reasons;
 signal__stream_3_0:
 signal__stream_3_1:
  if (!isint(a0)) goto signal__stream_3_2;
  
{
#ifdef USESIG
  allocp = register_streamed_signal(allocp, intval(a0), a2);
#else
  goto signal__stream_3_interrupt;
#endif
}
  allocp[0] = makesym(functor_normal_1);
  allocp[1] = a2;
  x0 = makefunctor(&allocp[0]);
  allocp += 2;
  unify_value(a1, x0);
  proceed();
 signal__stream_3_2:
  if (!isref(a0)) goto signal__stream_3_interrupt;
  deref_and_jump(a0,signal__stream_3_1);
  *reasonp++ =  a0;
  goto signal__stream_3_interrupt;
 signal__stream_3_ext_interrupt:
  reasonp = 0l;
 signal__stream_3_interrupt:
  toppred = &predicate_unix_xsignal__stream_3;
  goto interrupt_3;
 }

 net__convert_3_top: {
  q x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15;
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  qp = qp->next;
 net__convert_3_clear_reason:
  reasonp = reasons;
 net__convert_3_0:
 net__convert_3_1:
  switch (ptagof(a0)) {
 case FUNCTOR:
  switch (symval(functor_of(a0))) {
 case functor_unix_1:
  x0 = arg(a0, 0);
 net__convert_3_2:
  switch (ptagof(x0)) {
 case FUNCTOR:
 net__convert_3_3:
  if (!isgobj(x0)) goto net__convert_3_interrupt;
  if (!isclass(x0,byte__string)) goto net__convert_3_interrupt;
  
{
#ifndef NO_USESOCKET
  int family = PF_UNIX;
  struct sockaddr *addr;
  char *path = KLIC2C(x0);

  addr =
    (struct sockaddr *)malloc_check(sizeof(struct sockaddr)+strlen(path));
  addr->sa_family = family;
  strcpy(addr->sa_data, path);
  free(path);
  x1 = makeint(family);
  x2 = gd_new_pointer((q)addr, allocp);
  allocp = heapp;
#else
  goto net__convert_3_interrupt;
#endif
}
  unify_value(a1, x1);
  unify_value(a2, x2);
  proceed();
 case VARREF:
  deref_and_jump(x0,net__convert_3_2);
  *reasonp++ =  x0;
  goto net__convert_3_interrupt;
  };
  goto net__convert_3_interrupt;
 case functor_inet_2:
  x0 = arg(a0, 0);
 net__convert_3_4:
  switch (ptagof(x0)) {
 case FUNCTOR:
 net__convert_3_5:
  if (!isgobj(x0)) goto net__convert_3_interrupt;
  if (!isclass(x0,byte__string)) goto net__convert_3_8;
  x1 = arg(a0, 1);
 net__convert_3_6:
  if (!isint(x1)) goto net__convert_3_7;
  
{
#ifndef NO_USESOCKET
  int family = PF_INET;
  struct sockaddr_in *addr;
  char *host = KLIC2C(x0);
  struct hostent *ent = gethostbyname(host);

  if (ent == NULL) { fatalf("Unknown host %s", host); };
  free(host);
  addr = (struct sockaddr_in *)
    malloc_check(sizeof(struct sockaddr_in));
  addr->sin_family = family;
  BCOPY((char*)*ent->h_addr_list, (char*)&addr->sin_addr,
	sizeof(struct in_addr));
/*
  The following is not used as some systems don't understand it.

  addr->sin_addr.S_un.S_addr = *(int*)*ent->h_addr_list;
*/
  addr->sin_port = htons(intval(x1));
  x2 = makeint(family);
  x3 = gd_new_pointer((q)addr, allocp);
  allocp = heapp;
#else
  goto net__convert_3_interrupt;
#endif
}
  unify_value(a1, x2);
  unify_value(a2, x3);
  proceed();
 net__convert_3_7:
  if (!isref(x1)) goto net__convert_3_interrupt;
  deref_and_jump(x1,net__convert_3_6);
  *reasonp++ =  x1;
  goto net__convert_3_interrupt;
 net__convert_3_8:
  if (!isclass(x0,vector)) goto net__convert_3_interrupt;
  gblt_size_of_vector(x0,x4,net__convert_3_interrupt);
 net__convert_3_9:
  if (x4 != makeint(4L)) goto net__convert_3_interrupt;
  x6 = makeint(0L);
  gblt_element_of_vector(x0,x6,x5,net__convert_3_interrupt);
  x8 = makeint(1L);
  gblt_element_of_vector(x0,x8,x7,net__convert_3_interrupt);
  x10 = makeint(2L);
  gblt_element_of_vector(x0,x10,x9,net__convert_3_interrupt);
  x12 = makeint(3L);
  gblt_element_of_vector(x0,x12,x11,net__convert_3_interrupt);
 net__convert_3_10:
  if (!isint(x5)) goto net__convert_3_19;
 net__convert_3_11:
  if (!isint(x7)) goto net__convert_3_18;
 net__convert_3_12:
  if (!isint(x9)) goto net__convert_3_17;
 net__convert_3_13:
  if (!isint(x11)) goto net__convert_3_16;
  x13 = arg(a0, 1);
 net__convert_3_14:
  if (!isint(x13)) goto net__convert_3_15;
  
{
#ifndef NO_USESOCKET
  int family = PF_INET;
  struct sockaddr_in *addr;
  int b1, b2, b3, b4;
  unsigned long laddr;
  char buf[100];

  addr = (struct sockaddr_in *)
    malloc_check(sizeof(struct sockaddr_in));
  addr->sin_family = family;
  b1 = intval(x5); b2 = intval(x7); b3 = intval(x9); b4 = intval(x11);
  (void)sprintf(buf, "%d.%d.%d.%d", b1, b2, b3, b4);
  laddr = inet_addr(buf);
  if (laddr == -1) goto net__convert_3_interrupt;
  BCOPY((char*)&laddr, (char*)&addr->sin_addr, sizeof(struct in_addr));
/*
  The following is not used as some systems don't understand it.

  addr->sin_addr.S_un.S_un_b.s_b1 = intval(x5);
  addr->sin_addr.S_un.S_un_b.s_b2 = intval(x7);
  addr->sin_addr.S_un.S_un_b.s_b3 = intval(x9);
  addr->sin_addr.S_un.S_un_b.s_b4 = intval(x11);
*/
  addr->sin_port = htons(intval(x13));
  x14 = makeint(family);
  x15 = gd_new_pointer((q)addr, allocp);
  allocp = heapp;
#else
  goto net__convert_3_interrupt;
#endif
}
  unify_value(a1, x14);
  unify_value(a2, x15);
  proceed();
 net__convert_3_15:
  if (!isref(x13)) goto net__convert_3_interrupt;
  deref_and_jump(x13,net__convert_3_14);
  *reasonp++ =  x13;
  goto net__convert_3_interrupt;
 net__convert_3_16:
  if (!isref(x11)) goto net__convert_3_interrupt;
  deref_and_jump(x11,net__convert_3_13);
  *reasonp++ =  x11;
  goto net__convert_3_interrupt;
 net__convert_3_17:
  if (!isref(x9)) goto net__convert_3_interrupt;
  deref_and_jump(x9,net__convert_3_12);
  *reasonp++ =  x9;
  goto net__convert_3_interrupt;
 net__convert_3_18:
  if (!isref(x7)) goto net__convert_3_interrupt;
  deref_and_jump(x7,net__convert_3_11);
  *reasonp++ =  x7;
  goto net__convert_3_interrupt;
 net__convert_3_19:
  if (!isref(x5)) goto net__convert_3_interrupt;
  deref_and_jump(x5,net__convert_3_10);
  *reasonp++ =  x5;
  goto net__convert_3_interrupt;
 case VARREF:
  deref_and_jump(x0,net__convert_3_4);
  *reasonp++ =  x0;
  goto net__convert_3_interrupt;
  };
  goto net__convert_3_interrupt;
  };
  goto net__convert_3_interrupt;
 case VARREF:
  deref_and_jump(a0,net__convert_3_1);
  *reasonp++ =  a0;
  goto net__convert_3_interrupt;
  };
  goto net__convert_3_interrupt;
 net__convert_3_ext_interrupt:
  reasonp = 0l;
 net__convert_3_interrupt:
  toppred = &predicate_unix_xnet__convert_3;
  goto interrupt_3;
 }

 connect_2_top: {
  q x0, x1, x2;
  a0 = qp->args[0];
  a1 = qp->args[1];
  qp = qp->next;
 connect_2_clear_reason:
  reasonp = reasons;
 connect_2_0:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xconnect_4);
  allocp[2] = x0 = makeref(&allocp[2]);
  allocp[3] = x1 = makeref(&allocp[3]);
  allocp[4] = x2 = makeref(&allocp[4]);
  allocp[5] = a1;
  a1 = x0;
  a2 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 6;
  execute(net__convert_3_0);
  goto net__convert_3_ext_interrupt;
 connect_2_ext_interrupt:
  reasonp = 0l;
 connect_2_interrupt:
  toppred = &predicate_unix_xconnect_2;
  goto interrupt_2;
 }

 connect_4_top: {
  q x0;
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  a3 = qp->args[3];
  qp = qp->next;
 connect_4_clear_reason:
  reasonp = reasons;
 connect_4_0:
 connect_4_1:
  if (!isint(a0)) goto connect_4_2;
  
{
#ifndef NO_USESOCKET
  int family = intval(a0);
  int sock = socket(family, SOCK_STREAM, 0);

  if (sock < 0) { fatalp("socket", "Socket creation error"); }
#ifdef ASYNCIO
  init_asynchronous_io();
  setasync(sock, "connection");
  register_asynchronous_io_stream(sock, a2);
#endif
  x0 = makeint(sock);
#else
  goto connect_4_interrupt;
#endif
}
  a0 = x0;
  execute(connect__sub_4_clear_reason);
  goto connect__sub_4_ext_interrupt;
 connect_4_2:
  if (!isref(a0)) goto connect_4_interrupt;
  deref_and_jump(a0,connect_4_1);
  *reasonp++ =  a0;
  goto connect_4_interrupt;
 connect_4_ext_interrupt:
  reasonp = 0l;
 connect_4_interrupt:
  goto interrupt_4;
 }

 connect__sub_4_top: {
  q x0, x1, x2, x3;
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  a3 = qp->args[3];
  qp = qp->next;
 connect__sub_4_clear_reason:
  reasonp = reasons;
 connect__sub_4_0:
 connect__sub_4_1:
  if (!isint(a0)) goto connect__sub_4_4;
 connect__sub_4_2:
  switch (ptagof(a1)) {
 case FUNCTOR:
 connect__sub_4_3:
  if (!isgobj(a1)) goto connect__sub_4_5;
  if (!isclass(a1,pointer)) goto connect__sub_4_5;
  
{
#ifndef NO_USESOCKET
  int sock = intval(a0);
  struct sockaddr *addr = (struct sockaddr*)
    ((struct pointer_object *)(data_objectp(a1)))->pointer;

 again:
  if (connect(sock, addr, sizeof(struct sockaddr)) < 0) {
#ifdef ASYNCIO
    if (errno == EINTR) goto again;
    if (errno != EISCONN) {
      if (errno == EINPROGRESS || errno == EALREADY) goto connect__sub_4_5;
#endif
      fatalp("connect", "Socket connection error");
#ifdef ASYNCIO
    }
#endif
  }
  free(addr);
  MakeInBuf(x0, sock, 4096);
  MakeOutBuf(x1, sock, 4096);
#else
  goto connect__sub_4_5;
#endif
}
  allocp[0] = makesym(functor_normal_1);
  allocp[1] = x3 = makeref(&allocp[1]);
  x2 = makefunctor(&allocp[0]);
  allocp += 2;
  unify_value(a3, x2);
  a0 = x3;
  a1 = a2;
  a2 = x0;
  a3 = x1;
  a4 = makeint(0L);
  execute(async__io_5_clear_reason);
  goto async__io_5_ext_interrupt;
 case VARREF:
  deref_and_jump(a1,connect__sub_4_2);
  *reasonp++ =  a1;
  goto connect__sub_4_5;
  };
  goto connect__sub_4_5;
 connect__sub_4_4:
  if (!isref(a0)) goto connect__sub_4_5;
  deref_and_jump(a0,connect__sub_4_1);
  *reasonp++ =  a0;
  goto connect__sub_4_5;
 connect__sub_4_5:
  alternative(connect__sub_4_clear_reason);
  {
#ifdef NO_USE_SOCKET
  goto connect__sub_4_interrupt;
#endif
}
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xconnect__sub_5);
  allocp[2] = a0;
  allocp[3] = a1;
  allocp[4] = a2;
  allocp[5] = x0 = makeref(&allocp[5]);
  allocp[6] = a3;
  allocp[7] = (q)(struct goalrec*)&allocp[0];
  allocp[8] = (q)(&predicate_timer_xinstantiate__after_2);
  allocp[9] = makefunctor(funct_const_6);
  allocp[10] = x0;
  qp = (struct goalrec*)&allocp[7];
  allocp += 11;
  proceed();
 connect__sub_4_ext_interrupt:
  reasonp = 0l;
 connect__sub_4_interrupt:
  toppred = &predicate_unix_xconnect__sub_4;
  goto interrupt_4;
 }

 connect__sub_5_top: {
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  a3 = qp->args[3];
  a4 = qp->args[4];
  qp = qp->next;
 connect__sub_5_clear_reason:
  reasonp = reasons;
 connect__sub_5_0:
 connect__sub_5_1:
  switch (ptagof(a2)) {
 case CONS:
  a2 = cdr_of(a2);
  a3 = a4;
  execute(connect__sub_4_0);
  goto connect__sub_4_ext_interrupt;
 case VARREF:
  deref_and_jump(a2,connect__sub_5_1);
  *reasonp++ =  a2;
  goto connect__sub_5_2;
  };
  goto connect__sub_5_2;
 connect__sub_5_2:
  alternative(connect__sub_5_clear_reason);
 connect__sub_5_3:
  switch (ptagof(a3)) {
 case ATOMIC:
  if (a3 != NILATOM) goto connect__sub_5_interrupt;
  a3 = a4;
  execute(connect__sub_4_clear_reason);
  goto connect__sub_4_ext_interrupt;
 case VARREF:
  deref_and_jump(a3,connect__sub_5_3);
  *reasonp++ =  a3;
  goto connect__sub_5_interrupt;
  };
  goto connect__sub_5_interrupt;
 connect__sub_5_ext_interrupt:
  reasonp = 0l;
 connect__sub_5_interrupt:
  goto interrupt_5;
 }

 connect2_2_top: {
  q x0, x1, x2;
  a0 = qp->args[0];
  a1 = qp->args[1];
  qp = qp->next;
 connect2_2_clear_reason:
  reasonp = reasons;
 connect2_2_0:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xconnect2_4);
  allocp[2] = x0 = makeref(&allocp[2]);
  allocp[3] = x1 = makeref(&allocp[3]);
  allocp[4] = x2 = makeref(&allocp[4]);
  allocp[5] = a1;
  a1 = x0;
  a2 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 6;
  execute(net__convert_3_0);
  goto net__convert_3_ext_interrupt;
 connect2_2_ext_interrupt:
  reasonp = 0l;
 connect2_2_interrupt:
  toppred = &predicate_unix_xconnect2_2;
  goto interrupt_2;
 }

 connect2_4_top: {
  q x0;
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  a3 = qp->args[3];
  qp = qp->next;
 connect2_4_clear_reason:
  reasonp = reasons;
 connect2_4_0:
 connect2_4_1:
  if (!isint(a0)) goto connect2_4_2;
  
{
#ifndef NO_USESOCKET
  int family = intval(a0);
  int sock = socket(family, SOCK_STREAM, 0);

  if (sock < 0) { fatalp("socket", "Socket creation error"); }
#ifdef ASYNCIO
  init_asynchronous_io();
  setasync(sock, "connection");
  register_asynchronous_io_stream(sock, a2);
#endif
  x0 = makeint(sock);
#else
  goto connect2_4_interrupt;
#endif
}
  a0 = x0;
  execute(connect2__sub_4_clear_reason);
  goto connect2__sub_4_ext_interrupt;
 connect2_4_2:
  if (!isref(a0)) goto connect2_4_interrupt;
  deref_and_jump(a0,connect2_4_1);
  *reasonp++ =  a0;
  goto connect2_4_interrupt;
 connect2_4_ext_interrupt:
  reasonp = 0l;
 connect2_4_interrupt:
  goto interrupt_4;
 }

 connect2__sub_4_top: {
  q x0, x1, x2, x3;
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  a3 = qp->args[3];
  qp = qp->next;
 connect2__sub_4_clear_reason:
  reasonp = reasons;
 connect2__sub_4_0:
 connect2__sub_4_1:
  if (!isint(a0)) goto connect2__sub_4_4;
 connect2__sub_4_2:
  switch (ptagof(a1)) {
 case FUNCTOR:
 connect2__sub_4_3:
  if (!isgobj(a1)) goto connect2__sub_4_5;
  if (!isclass(a1,pointer)) goto connect2__sub_4_5;
  
{
#ifndef NO_USESOCKET
  int sock = intval(a0);
  struct sockaddr *addr = (struct sockaddr*)
    ((struct pointer_object *)(data_objectp(a1)))->pointer;

 again2:
  if (connect(sock, addr, sizeof(struct sockaddr)) < 0) {
#ifdef ASYNCIO
    if (errno == EINTR) goto again2;
    if (errno != EISCONN) {
      if (errno == EINPROGRESS || errno == EALREADY) goto connect2__sub_4_5;
#endif
      fatalp("connect", "Socket connection error");
#ifdef ASYNCIO
    }
#endif
  }
  free(addr);
  MakeBIOBuf(x0, sock, sock, 4096);
#else
  goto connect2__sub_4_5;
#endif
}
  allocp[0] = makesym(functor_normal_2);
  allocp[1] = x2 = makeref(&allocp[1]);
  allocp[2] = x3 = makeref(&allocp[2]);
  x1 = makefunctor(&allocp[0]);
  allocp += 3;
  unify_value(a3, x1);
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xasync__output_4);
  allocp[2] = x3;
  allocp[3] = a2;
  allocp[4] = x0;
  allocp[5] = makeint(0L);
  a0 = x2;
  a1 = a2;
  a2 = x0;
  a3 = makeint(0L);
  qp = (struct goalrec*)&allocp[0];
  allocp += 6;
  execute(async__input_4_clear_reason);
  goto async__input_4_ext_interrupt;
 case VARREF:
  deref_and_jump(a1,connect2__sub_4_2);
  *reasonp++ =  a1;
  goto connect2__sub_4_5;
  };
  goto connect2__sub_4_5;
 connect2__sub_4_4:
  if (!isref(a0)) goto connect2__sub_4_5;
  deref_and_jump(a0,connect2__sub_4_1);
  *reasonp++ =  a0;
  goto connect2__sub_4_5;
 connect2__sub_4_5:
  otherwise(connect2__sub_4_interrupt);
  {
#ifdef NO_USE_SOCKET
  goto connect2__sub_4_interrupt;
#endif
}
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xconnect2__sub_5);
  allocp[2] = a0;
  allocp[3] = a1;
  allocp[4] = a2;
  allocp[5] = x0 = makeref(&allocp[5]);
  allocp[6] = a3;
  allocp[7] = (q)(struct goalrec*)&allocp[0];
  allocp[8] = (q)(&predicate_timer_xinstantiate__after_2);
  allocp[9] = makefunctor(funct_const_6);
  allocp[10] = x0;
  qp = (struct goalrec*)&allocp[7];
  allocp += 11;
  proceed();
 connect2__sub_4_ext_interrupt:
  reasonp = 0l;
 connect2__sub_4_interrupt:
  toppred = &predicate_unix_xconnect2__sub_4;
  goto interrupt_4;
 }

 connect2__sub_5_top: {
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  a3 = qp->args[3];
  a4 = qp->args[4];
  qp = qp->next;
 connect2__sub_5_clear_reason:
  reasonp = reasons;
 connect2__sub_5_0:
 connect2__sub_5_1:
  switch (ptagof(a2)) {
 case CONS:
  a2 = cdr_of(a2);
  a3 = a4;
  execute(connect2__sub_4_0);
  goto connect2__sub_4_ext_interrupt;
 case VARREF:
  deref_and_jump(a2,connect2__sub_5_1);
  *reasonp++ =  a2;
  goto connect2__sub_5_2;
  };
  goto connect2__sub_5_2;
 connect2__sub_5_2:
  otherwise(connect2__sub_5_interrupt);
 connect2__sub_5_3:
  switch (ptagof(a3)) {
 case ATOMIC:
  if (a3 != NILATOM) goto connect2__sub_5_interrupt;
  a3 = a4;
  execute(connect2__sub_4_0);
  goto connect2__sub_4_ext_interrupt;
 case VARREF:
  deref_and_jump(a3,connect2__sub_5_3);
  *reasonp++ =  a3;
  goto connect2__sub_5_interrupt;
  };
  goto connect2__sub_5_interrupt;
 connect2__sub_5_ext_interrupt:
  reasonp = 0l;
 connect2__sub_5_interrupt:
  goto interrupt_5;
 }

 bind_2_top: {
  q x0, x1, x2, x3, x4;
  a0 = qp->args[0];
  a1 = qp->args[1];
  qp = qp->next;
 bind_2_clear_reason:
  reasonp = reasons;
 bind_2_0:
 bind_2_1:
  switch (ptagof(a0)) {
 case FUNCTOR:
  switch (symval(functor_of(a0))) {
 case functor_unix_1:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xbind_4);
  allocp[2] = x0 = makeref(&allocp[2]);
  allocp[3] = x1 = makeref(&allocp[3]);
  allocp[4] = x2 = makeref(&allocp[4]);
  allocp[5] = a1;
  allocp[6] = makesym(functor_unix_1);
  x4 = arg(a0, 0);
  allocp[7] = x4;
  x3 = makefunctor(&allocp[6]);
  a0 = x3;
  a1 = x0;
  a2 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 8;
  execute(net__convert_3_0);
  goto net__convert_3_ext_interrupt;
 case functor_inet_1:
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xbind_4);
  allocp[2] = x0 = makeref(&allocp[2]);
  allocp[3] = x1 = makeref(&allocp[3]);
  allocp[4] = x2 = makeref(&allocp[4]);
  allocp[5] = a1;
  allocp[6] = makesym(functor_inet_2);
  allocp[7] = makefunctor(&vector_const_0);
  x4 = arg(a0, 0);
  allocp[8] = x4;
  x3 = makefunctor(&allocp[6]);
  a0 = x3;
  a1 = x0;
  a2 = x1;
  qp = (struct goalrec*)&allocp[0];
  allocp += 9;
  execute(net__convert_3_0);
  goto net__convert_3_ext_interrupt;
  };
  goto bind_2_interrupt;
 case VARREF:
  deref_and_jump(a0,bind_2_1);
  *reasonp++ =  a0;
  goto bind_2_interrupt;
  };
  goto bind_2_interrupt;
 bind_2_ext_interrupt:
  reasonp = 0l;
 bind_2_interrupt:
  toppred = &predicate_unix_xbind_2;
  goto interrupt_2;
 }

 bind_4_top: {
  q x0, x1, x2, x3;
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  a3 = qp->args[3];
  qp = qp->next;
 bind_4_clear_reason:
  reasonp = reasons;
 bind_4_0:
 bind_4_1:
  if (!isint(a0)) goto bind_4_4;
 bind_4_2:
  switch (ptagof(a1)) {
 case FUNCTOR:
 bind_4_3:
  if (!isgobj(a1)) goto bind_4_interrupt;
  if (!isclass(a1,pointer)) goto bind_4_interrupt;
  
{
#ifndef NO_USESOCKET
  int family = intval(a0);
  int reuse = 1;
  struct sockaddr *addr = (struct sockaddr*)
    ((struct pointer_object *)(data_objectp(a1)))->pointer;
  int sock = socket(family, SOCK_STREAM, 0);

  if (sock < 0) { fatalp("socket", "Socket creation error"); }
#ifdef ASYNCIO
  init_asynchronous_io();
#endif

  if (addr->sa_family == PF_INET) {
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		   (char *)&reuse, sizeof(reuse))) {
      fatalp("setsockopt", "Socket reuse setting error for binding");
    }
  }
  if (bind(sock, addr, sizeof(struct sockaddr)) < 0) {
    fatalp("bind", "Socket binding error");
  }
  free(addr);
#ifdef ASYNCIO
  setasync(sock, "bound socket");
#endif
  if (listen(sock, 5) < 0) {
    fatalp("listen", "Socket listen error");
  }
#ifdef ASYNCIO
  register_asynchronous_io_stream(sock, a2);
#endif
  x0 = makeint(sock);
#else
  goto bind_4_interrupt;
#endif
}
  allocp[0] = makesym(functor_normal_1);
  allocp[1] = x2 = makeref(&allocp[1]);
  x1 = makefunctor(&allocp[0]);
  allocp += 2;
  unify_value(a3, x1);
  a0 = x2;
  a1 = x0;
  allocp[0] = x3 = makeref(&allocp[0]);
  a3 = x3;
  allocp += 1;
  execute(bound__sock_4_clear_reason);
  goto bound__sock_4_ext_interrupt;
 case VARREF:
  deref_and_jump(a1,bind_4_2);
  *reasonp++ =  a1;
  goto bind_4_interrupt;
  };
  goto bind_4_interrupt;
 bind_4_4:
  if (!isref(a0)) goto bind_4_interrupt;
  deref_and_jump(a0,bind_4_1);
  *reasonp++ =  a0;
  goto bind_4_interrupt;
 bind_4_ext_interrupt:
  reasonp = 0l;
 bind_4_interrupt:
  goto interrupt_4;
 }

 bound__sock_4_top: {
  q x0, x1, x2, x3, x4, x5, x6, x7;
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  a3 = qp->args[3];
  qp = qp->next;
 bound__sock_4_clear_reason:
  reasonp = reasons;
 bound__sock_4_0:
 bound__sock_4_1:
  switch (ptagof(a0)) {
 case CONS:
  x0 = car_of(a0);
 bound__sock_4_2:
  switch (ptagof(x0)) {
 case FUNCTOR:
  switch (symval(functor_of(x0))) {
 case functor_accept_1:
 bound__sock_4_3:
  if (!isint(a1)) goto bound__sock_4_4;
  
{
#ifndef NO_USESOCKET
  int sock;
  struct sockaddr addr;
  socklen_t socklen = sizeof(addr);
#ifdef ASYNCIO
  if (!poll_read_available(intval(a1))) goto bound__sock_4_9;
#endif
  while (1) {
    if ((sock = accept(intval(a1), &addr, &socklen)) > 0) break;
    if (errno != EINTR) {
      fatalp("accept", "Error in accept");
    }
  }
#ifdef ASYNCIO
  setasync(sock, "accepted socket");
  register_asynchronous_io_stream(sock,a3);
#endif
  MakeInBuf(x1, sock, 4096);
  MakeOutBuf(x2, sock, 4096);
#else
  goto bound__sock_4_9;
#endif
}
  allocp[0] = makesym(functor_normal_1);
  allocp[1] = x4 = makeref(&allocp[1]);
  x3 = makefunctor(&allocp[0]);
  x5 = arg(x0, 0);
  allocp += 2;
  unify_value(x5, x3);
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xbound__sock_4);
  x6 = cdr_of(a0);
  allocp[2] = x6;
  allocp[3] = a1;
  allocp[4] = a2;
  allocp[5] = x7 = makeref(&allocp[5]);
  a0 = x4;
  a1 = a3;
  a2 = x1;
  a3 = x2;
  a4 = makeint(0L);
  qp = (struct goalrec*)&allocp[0];
  allocp += 6;
  execute(async__io_5_clear_reason);
  goto async__io_5_ext_interrupt;
 bound__sock_4_4:
  if (!isref(a1)) goto bound__sock_4_9;
  deref_and_jump(a1,bound__sock_4_3);
  *reasonp++ =  a1;
  goto bound__sock_4_9;
 case functor_accept2_1:
 bound__sock_4_5:
  if (!isint(a1)) goto bound__sock_4_6;
  
{
#ifndef NO_USESOCKET
  int sock;
  struct sockaddr addr;
  socklen_t socklen = sizeof(addr);
#ifdef ASYNCIO
  if (!poll_read_available(intval(a1))) goto bound__sock_4_9;
#endif
  while (1) {
    if ((sock = accept(intval(a1), &addr, &socklen)) > 0) break;
    if (errno != EINTR) {
      fatalp("accept", "Error in accept");
    }
  }
#ifdef ASYNCIO
  setasync(sock, "accepted socket");
  register_asynchronous_io_stream(sock,a3);
#endif
  MakeBIOBuf(x1, sock, sock, 4096);
#else
  goto bound__sock_4_9;
#endif
}
  allocp[0] = makesym(functor_normal_2);
  allocp[1] = x3 = makeref(&allocp[1]);
  allocp[2] = x4 = makeref(&allocp[2]);
  x2 = makefunctor(&allocp[0]);
  x5 = arg(x0, 0);
  allocp += 3;
  unify_value(x5, x2);
  allocp[0] = (q)qp;
  allocp[1] = (q)(&predicate_unix_xbound__sock_4);
  x6 = cdr_of(a0);
  allocp[2] = x6;
  allocp[3] = a1;
  allocp[4] = a2;
  allocp[5] = x7 = makeref(&allocp[5]);
  allocp[6] = (q)(struct goalrec*)&allocp[0];
  allocp[7] = (q)(&predicate_unix_xasync__output_4);
  allocp[8] = x4;
  allocp[9] = a3;
  allocp[10] = x1;
  allocp[11] = makeint(0L);
  a0 = x3;
  a1 = a3;
  a2 = x1;
  a3 = makeint(0L);
  qp = (struct goalrec*)&allocp[6];
  allocp += 12;
  execute(async__input_4_clear_reason);
  goto async__input_4_ext_interrupt;
 bound__sock_4_6:
  if (!isref(a1)) goto bound__sock_4_9;
  deref_and_jump(a1,bound__sock_4_5);
  *reasonp++ =  a1;
  goto bound__sock_4_9;
  };
  goto bound__sock_4_9;
 case VARREF:
  deref_and_jump(x0,bound__sock_4_2);
  *reasonp++ =  x0;
  goto bound__sock_4_9;
  };
  goto bound__sock_4_9;
 case ATOMIC:
  if (a0 != NILATOM) goto bound__sock_4_9;
 bound__sock_4_7:
  if (!isint(a1)) goto bound__sock_4_8;
  
{
#ifndef NO_USESOCKET
  int fd = intval(a1);
  socklen_t namelen = 1000;
  struct sockaddr *name = (struct sockaddr *)malloc_check(namelen);
  getsockname(fd, name, &namelen);
  if (close(fd) != 0) {
    fatalp("close", "Error in closing bound socket");
  }
#ifdef ASYNCIO
  close_asynchronous_io_stream(fd);
#endif
  if (name->sa_family == PF_UNIX) {
    if (unlink(name->sa_data)) {
      fatalp("unlink", "Error in unlinking socket: %s", name->sa_data);
    }
  }
  free(name);
#else
  goto bound__sock_4_9;
#endif
}
  proceed();
 bound__sock_4_8:
  if (!isref(a1)) goto bound__sock_4_9;
  deref_and_jump(a1,bound__sock_4_7);
  *reasonp++ =  a1;
  goto bound__sock_4_9;
 case VARREF:
  deref_and_jump(a0,bound__sock_4_1);
  *reasonp++ =  a0;
  goto bound__sock_4_9;
  };
  goto bound__sock_4_9;
 bound__sock_4_9:
 bound__sock_4_10:
  switch (ptagof(a2)) {
 case CONS:
  a2 = cdr_of(a2);
  execute(bound__sock_4_clear_reason);
  goto bound__sock_4_ext_interrupt;
 case VARREF:
  deref_and_jump(a2,bound__sock_4_10);
  *reasonp++ =  a2;
  goto bound__sock_4_interrupt;
  };
  goto bound__sock_4_interrupt;
 bound__sock_4_ext_interrupt:
  reasonp = 0l;
 bound__sock_4_interrupt:
  toppred = &predicate_unix_xbound__sock_4;
  goto interrupt_4;
 }

 pipe_1_top: {
  q x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10;
  a0 = qp->args[0];
  qp = qp->next;
 pipe_1_clear_reason:
  reasonp = reasons;
 pipe_1_0:
  
{
  int fd[2];
  if (pipe(fd)!=0) goto pipe_1_1;
  Fdopen(fd[0],x0,pipe_1_1,"r");
  Fdopen(fd[1],x1,pipe_1_1,"w");
}
  generic_arg[0] = x0;
  x2 = makefunctor(&string_const_2);
  generic_arg[1] = x2;
  x3 = NILATOM;
  generic_arg[2] = x3;
  x4 = makefunctor(&string_const_1);
  generic_arg[3] = x4;
  new_generic(file__io_g_new, 4, x5, 0);
  x6 = NILATOM;
  generic_arg[0] = x6;
  x7 = makefunctor(&string_const_1);
  generic_arg[1] = x7;
  generic_arg[2] = x1;
  x8 = makefunctor(&string_const_3);
  generic_arg[3] = x8;
  new_generic(file__io_g_new, 4, x9, 0);
  allocp[0] = makesym(functor_normal_2);
  allocp[1] = x5;
  allocp[2] = x9;
  x10 = makefunctor(&allocp[0]);
  allocp += 3;
  unify_value(a0, x10);
  proceed();
 pipe_1_1:
  otherwise(pipe_1_interrupt);
  x0 = makesym(atom_abnormal);
  unify_value(a0, x0);
  proceed();
 pipe_1_ext_interrupt:
  reasonp = 0l;
 pipe_1_interrupt:
  toppred = &predicate_unix_xpipe_1;
  goto interrupt_1;
 }

 async__io_5_top: {
  q x0, x1, x2, x3, x4, x5;
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  a3 = qp->args[3];
  a4 = qp->args[4];
  qp = qp->next;
 async__io_5_clear_reason:
  reasonp = reasons;
 async__io_5_0:
 async__io_5_1:
  switch (ptagof(a0)) {
 case CONS:
  x0 = car_of(a0);
 async__io_5_2:
  switch (ptagof(x0)) {
 case ATOMIC:
  if (!isint(x0)) goto async__io_5_33;
 async__io_5_3:
  gblt_integer(x0,async__io_5_33);
 async__io_5_4:
  switch (ptagof(a3)) {
 case FUNCTOR:
 async__io_5_5:
  if (!isgobj(a3)) goto async__io_5_33;
  if (!isclass(a3,pointer)) goto async__io_5_33;
  
{
#ifdef ASYNCIO
  struct iobuf *iob = IOBuf(a3);
  if (iob->ptr == iob->lim) {
    switch (write_buf(iob)) {
    case -1: goto async__io_5_33;
    case 1: break;
    }
  }
  *iob->ptr++ = intval(x0);
#else
  goto async__io_5_33;
#endif
}
  a0 = cdr_of(a0);
  execute(async__io_5_clear_reason);
  goto async__io_5_ext_interrupt;
 case VARREF:
  deref_and_jump(a3,async__io_5_4);
  *reasonp++ =  a3;
  goto async__io_5_33;
  };
  goto async__io_5_33;
 case FUNCTOR:
  switch (symval(functor_of(x0))) {
 case functor_getc_1:
 async__io_5_6:
  switch (ptagof(a2)) {
 case FUNCTOR:
 async__io_5_7:
  if (!isgobj(a2)) goto async__io_5_33;
  if (!isclass(a2,pointer)) goto async__io_5_33;
 async__io_5_8:
  if (!isint(a4)) goto async__io_5_9;
  
{
#ifdef ASYNCIO
  struct iobuf *iob = IOBuf(a2);
  int c;
  if (iob->ptr == iob->lim) {
    switch (fill_buf(iob)) {
    case -1: goto async__io_5_33;
    case -2: goto async__io_5_33;
    case 0: c = -1; break;
    case 1: c = *iob->ptr++; break;
    }
  } else {
    c = *iob->ptr++;
  }
  x2 = makeint(intval(a4) + ( c=='\n' ? 1 : 0 ));
  x1 = makeint(c);
#else
  goto async__io_5_33;
#endif
}
  x3 = arg(x0, 0);
  unify_value(x3, x1);
  a0 = cdr_of(a0);
  a4 = x2;
  execute(async__io_5_clear_reason);
  goto async__io_5_ext_interrupt;
 async__io_5_9:
  if (!isref(a4)) goto async__io_5_33;
  deref_and_jump(a4,async__io_5_8);
  *reasonp++ =  a4;
  goto async__io_5_33;
 case VARREF:
  deref_and_jump(a2,async__io_5_6);
  *reasonp++ =  a2;
  goto async__io_5_33;
  };
  goto async__io_5_33;
 case functor_fread_2:
 async__io_5_10:
  switch (ptagof(a2)) {
 case FUNCTOR:
 async__io_5_11:
  if (!isgobj(a2)) goto async__io_5_33;
  if (!isclass(a2,pointer)) goto async__io_5_33;
  x1 = arg(x0, 0);
 async__io_5_12:
  if (!isint(x1)) goto async__io_5_15;
 async__io_5_13:
  if (!isint(a4)) goto async__io_5_14;
  
{
#ifdef ASYNCIO
  struct iobuf *iob = IOBuf(a2);
  int toread = intval(x1);
  int k, nnl;
  int ready_bytes = iob->lim - iob->ptr;
  q str;
  if (ready_bytes==0) {
    switch (fill_buf(iob)) {
    case -1: goto async__io_5_33;
    case -2: goto async__io_5_33;
    case 0: goto async__io_5_33;
    case 1: break;
    }
  }
  if (toread > iob->lim - iob->ptr) toread = iob->lim - iob->ptr;
  if ((char *)allocp+
      sizeof(struct byte_string_object)+toread+sizeof(long) >=
      (char *)real_heaplimit) {
    allocp = real_heaplimit;
    goto async__io_5_ext_interrupt;
  }
  for (k=0, nnl=0; k<toread; k++) {
    if (iob->ptr[k] == '\n') nnl++;
  }
  str = BC2KLIC(iob->ptr, toread, allocp);
  if (isref(str)) {
    fatal("internal error: string allocation failure for fread");
  }
  x2 = str;
  allocp = heapp;
  iob->ptr += toread;
  x3 = makeint(intval(a4)+nnl);
#else
  goto async__io_5_33;
#endif
}
  x4 = arg(x0, 1);
  unify_value(x4, x2);
  a0 = cdr_of(a0);
  a4 = x3;
  execute(async__io_5_clear_reason);
  goto async__io_5_ext_interrupt;
 async__io_5_14:
  if (!isref(a4)) goto async__io_5_33;
  deref_and_jump(a4,async__io_5_13);
  *reasonp++ =  a4;
  goto async__io_5_33;
 async__io_5_15:
  if (!isref(x1)) goto async__io_5_33;
  deref_and_jump(x1,async__io_5_12);
  *reasonp++ =  x1;
  goto async__io_5_33;
 case VARREF:
  deref_and_jump(a2,async__io_5_10);
  *reasonp++ =  a2;
  goto async__io_5_33;
  };
  goto async__io_5_33;
 case functor_linecount_1:
  x1 = arg(x0, 0);
  unify(x1, a4);
  a0 = cdr_of(a0);
  execute(async__io_5_0);
  goto async__io_5_ext_interrupt;
 case functor_feof_1:
 async__io_5_16:
  switch (ptagof(a2)) {
 case FUNCTOR:
 async__io_5_17:
  if (!isgobj(a2)) goto async__io_5_33;
  if (!isclass(a2,pointer)) goto async__io_5_33;
  
{
#ifdef ASYNCIO
  struct iobuf *iob = IOBuf(a2);
  if (iob->ptr == iob->lim) {
    switch (fill_buf(iob)) {
    case -1: goto async__io_5_33;
    case 0: x1 = makeint(1); break;
    case 1: x1 = makeint(0); break;
    }
  } else {
    x1 = makeint(0);
  }
#else
  goto async__io_5_33;
#endif
}
  x2 = arg(x0, 0);
  unify_value(x2, x1);
  a0 = cdr_of(a0);
  execute(async__io_5_clear_reason);
  goto async__io_5_ext_interrupt;
 case VARREF:
  deref_and_jump(a2,async__io_5_16);
  *reasonp++ =  a2;
  goto async__io_5_33;
  };
  goto async__io_5_33;
 case functor_putc_1:
  
#ifndef ASYNCIO
#endif

  x2 = cdr_of(a0);
  allocp[0] = x2;
  x3 = arg(x0, 0);
  allocp[1] = x3;
  x1 = makecons(&allocp[0]);
  a0 = x1;
  allocp += 2;
  execute(async__io_5_clear_reason);
  goto async__io_5_ext_interrupt;
 case functor_fwrite_2:
  x1 = arg(x0, 0);
 async__io_5_18:
  switch (ptagof(x1)) {
 case FUNCTOR:
 async__io_5_19:
  if (!isgobj(x1)) goto async__io_5_33;
 async__io_5_20:
  gblt_is_string(x1,async__io_5_33);
  gblt_size_of_string(x1,x2,async__io_5_33);
  gblt_element_size_of_string(x1,x3,async__io_5_33);
 async__io_5_21:
  if (x3 != makeint(8L)) goto async__io_5_33;
 async__io_5_22:
  switch (ptagof(a3)) {
 case FUNCTOR:
 async__io_5_23:
  if (!isgobj(a3)) goto async__io_5_33;
  if (!isclass(a3,pointer)) goto async__io_5_33;
 async__io_5_24:
  if (!isgobj(x1)) goto async__io_5_33;
  if (!isclass(x1,byte__string)) goto async__io_5_33;
  
{
#ifdef ASYNCIO
  struct iobuf *iob = IOBuf(a3);
  char *str = KLIC2C(x1);
  int len =intval(x2);
  int room = iob->lim - iob->ptr;
  while (iob->ptr + len >= iob->lim) {
    BCOPY(str, iob->ptr, room);
    len -= room;
    str += room;
    iob->ptr += room;
    switch (write_buf(iob)) {
    case -1: goto async__io_5_33;
    case 1: break;
    }
  }
  BCOPY(str, iob->ptr, len);
  iob->ptr += len;
#else
  goto async__io_5_33;
#endif
}
  x4 = arg(x0, 1);
  unify_value(x4, x2);
  a0 = cdr_of(a0);
  execute(async__io_5_clear_reason);
  goto async__io_5_ext_interrupt;
 case VARREF:
  deref_and_jump(a3,async__io_5_22);
  *reasonp++ =  a3;
  goto async__io_5_33;
  };
  goto async__io_5_33;
 case VARREF:
  deref_and_jump(x1,async__io_5_18);
  *reasonp++ =  x1;
  goto async__io_5_33;
  };
  goto async__io_5_33;
 case functor_fwrite_1:
  allocp[0] = makesym(functor_fwrite_2);
  x2 = arg(x0, 0);
  allocp[1] = x2;
  allocp[2] = x3 = makeref(&allocp[2]);
  x1 = makefunctor(&allocp[0]);
  x5 = cdr_of(a0);
  allocp[3] = x5;
  allocp[4] = x1;
  x4 = makecons(&allocp[3]);
  a0 = x4;
  allocp += 5;
  execute(async__io_5_0);
  goto async__io_5_ext_interrupt;
 case functor_fflush_1:
 async__io_5_25:
  switch (ptagof(a3)) {
 case FUNCTOR:
 async__io_5_26:
  if (!isgobj(a3)) goto async__io_5_33;
  if (!isclass(a3,pointer)) goto async__io_5_33;
  
{
#ifdef ASYNCIO
  struct iobuf *iob = IOBuf(a3);
  if (iob->ptr != iob->buf) {
    switch (write_buf(iob)) {
    case -1: goto async__io_5_33;
    case 1: break;
    }
  }
#else
  goto async__io_5_33;
#endif
}
  x1 = arg(x0, 0);
  x2 = makeint(0L);
  unify_value(x1, x2);
  a0 = cdr_of(a0);
  execute(async__io_5_clear_reason);
  goto async__io_5_ext_interrupt;
 case VARREF:
  deref_and_jump(a3,async__io_5_25);
  *reasonp++ =  a3;
  goto async__io_5_33;
  };
  goto async__io_5_33;
 case functor_sync_1:
 async__io_5_27:
  switch (ptagof(a3)) {
 case FUNCTOR:
 async__io_5_28:
  if (!isgobj(a3)) goto async__io_5_33;
  if (!isclass(a3,pointer)) goto async__io_5_33;
  
#ifdef ASYNCIO
{
  struct iobuf *iob = IOBuf(a3);
  switch (write_buf(iob)) {
  case -1: goto async__io_5_33;
  case 1: break;
  }
}
#endif

  x1 = arg(x0, 0);
  x2 = makeint(0L);
  unify_value(x1, x2);
  a0 = cdr_of(a0);
  execute(async__io_5_clear_reason);
  goto async__io_5_ext_interrupt;
 case VARREF:
  deref_and_jump(a3,async__io_5_27);
  *reasonp++ =  a3;
  goto async__io_5_33;
  };
  goto async__io_5_33;
  };
  goto async__io_5_33;
 case VARREF:
  deref_and_jump(x0,async__io_5_2);
  *reasonp++ =  x0;
  goto async__io_5_33;
  };
  goto async__io_5_33;
 case ATOMIC:
  if (a0 != NILATOM) goto async__io_5_33;
 async__io_5_29:
  switch (ptagof(a3)) {
 case FUNCTOR:
 async__io_5_30:
  if (!isgobj(a3)) goto async__io_5_33;
  if (!isclass(a3,pointer)) goto async__io_5_33;
 async__io_5_31:
  switch (ptagof(a2)) {
 case FUNCTOR:
 async__io_5_32:
  if (!isgobj(a2)) goto async__io_5_33;
  if (!isclass(a2,pointer)) goto async__io_5_33;
  
{
#ifdef ASYNCIO
  struct iobuf *outb = IOBuf(a3);
  struct iobuf *inb = IOBuf(a2);
  if (outb->ptr != outb->buf) {
    switch (write_buf(outb)) {
    case -1: goto async__io_5_33;
    case 1: break;
    }
  }
  if (close(outb->fd) != 0) {
    fatalp("close", "Error in closing asynchronous I/O");
  }
  close_asynchronous_io_stream(outb->fd);
  free(outb->buf);
  free(inb->buf);
#else
  goto async__io_5_33;
#endif
}
  proceed();
 case VARREF:
  deref_and_jump(a2,async__io_5_31);
  *reasonp++ =  a2;
  goto async__io_5_33;
  };
  goto async__io_5_33;
 case VARREF:
  deref_and_jump(a3,async__io_5_29);
  *reasonp++ =  a3;
  goto async__io_5_33;
  };
  goto async__io_5_33;
 case VARREF:
  deref_and_jump(a0,async__io_5_1);
  *reasonp++ =  a0;
  goto async__io_5_33;
  };
  goto async__io_5_33;
 async__io_5_33:
  alternative(async__io_5_clear_reason);
 async__io_5_34:
  switch (ptagof(a1)) {
 case CONS:
  a1 = cdr_of(a1);
  execute(async__io_5_clear_reason);
  goto async__io_5_ext_interrupt;
 case VARREF:
  deref_and_jump(a1,async__io_5_34);
  *reasonp++ =  a1;
  goto async__io_5_interrupt;
  };
  goto async__io_5_interrupt;
 async__io_5_ext_interrupt:
  reasonp = 0l;
 async__io_5_interrupt:
  toppred = &predicate_unix_xasync__io_5;
  goto interrupt_5;
 }

 async__input_4_top: {
  q x0, x1, x2, x3, x4;
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  a3 = qp->args[3];
  qp = qp->next;
 async__input_4_clear_reason:
  reasonp = reasons;
 async__input_4_0:
 async__input_4_1:
  switch (ptagof(a0)) {
 case CONS:
  x0 = car_of(a0);
 async__input_4_2:
  switch (ptagof(x0)) {
 case FUNCTOR:
  switch (symval(functor_of(x0))) {
 case functor_getc_1:
 async__input_4_3:
  switch (ptagof(a2)) {
 case FUNCTOR:
 async__input_4_4:
  if (!isgobj(a2)) goto async__input_4_17;
  if (!isclass(a2,pointer)) goto async__input_4_17;
 async__input_4_5:
  if (!isint(a3)) goto async__input_4_6;
  
{
#ifdef ASYNCIO
  struct biobuf *biob = BIOBuf(a2);
  struct iobuf *iob = &(biob->ibuf);
  int c;
  if (iob->ptr == iob->lim) {
    switch (fill_buf(iob)) {
    case -1: goto async__input_4_17;
    case -2: goto async__input_4_17;
    case 0: c = -1; break;
    case 1: c = *iob->ptr++; break;
    }
  } else {
    c = *iob->ptr++;
  }
  x2 = makeint(intval(a3) + ( c=='\n' ? 1 : 0 ));
  x1 = makeint(c);
#else
  goto async__input_4_17;
#endif
}
  x3 = arg(x0, 0);
  unify_value(x3, x1);
  a0 = cdr_of(a0);
  a3 = x2;
  execute(async__input_4_clear_reason);
  goto async__input_4_ext_interrupt;
 async__input_4_6:
  if (!isref(a3)) goto async__input_4_17;
  deref_and_jump(a3,async__input_4_5);
  *reasonp++ =  a3;
  goto async__input_4_17;
 case VARREF:
  deref_and_jump(a2,async__input_4_3);
  *reasonp++ =  a2;
  goto async__input_4_17;
  };
  goto async__input_4_17;
 case functor_fread_2:
 async__input_4_7:
  switch (ptagof(a2)) {
 case FUNCTOR:
 async__input_4_8:
  if (!isgobj(a2)) goto async__input_4_17;
  if (!isclass(a2,pointer)) goto async__input_4_17;
  x1 = arg(x0, 0);
 async__input_4_9:
  if (!isint(x1)) goto async__input_4_12;
 async__input_4_10:
  if (!isint(a3)) goto async__input_4_11;
  
{
#ifdef ASYNCIO
  struct biobuf *biob = BIOBuf(a2);
  struct iobuf *iob = &(biob->ibuf);
  int toread = intval(x1);
  int k, nnl;
  int ready_bytes = iob->lim - iob->ptr;
  q str;
  if (ready_bytes==0) {
    switch (fill_buf(iob)) {
    case -1: goto async__input_4_17;
    case -2: goto async__input_4_17;
    case 0: goto async__input_4_17;
    case 1: break;
    }
  }
  if (toread > iob->lim - iob->ptr) toread = iob->lim - iob->ptr;
  if ((char *)allocp+
      sizeof(struct byte_string_object)+toread+sizeof(long) >=
      (char *)real_heaplimit) {
    allocp = real_heaplimit;
    goto async__input_4_ext_interrupt;
  }
  for (k=0, nnl=0; k<toread; k++) {
    if (iob->ptr[k] == '\n') nnl++;
  }
  str = BC2KLIC(iob->ptr, toread, allocp);
  if (isref(str)) {
    fatal("internal error: string allocation failure for fread");
  }
  x2 = str;
  allocp = heapp;
  iob->ptr += toread;
  x3 = makeint(intval(a3)+nnl);
#else
  goto async__input_4_17;
#endif
}
  x4 = arg(x0, 1);
  unify_value(x4, x2);
  a0 = cdr_of(a0);
  a3 = x3;
  execute(async__input_4_clear_reason);
  goto async__input_4_ext_interrupt;
 async__input_4_11:
  if (!isref(a3)) goto async__input_4_17;
  deref_and_jump(a3,async__input_4_10);
  *reasonp++ =  a3;
  goto async__input_4_17;
 async__input_4_12:
  if (!isref(x1)) goto async__input_4_17;
  deref_and_jump(x1,async__input_4_9);
  *reasonp++ =  x1;
  goto async__input_4_17;
 case VARREF:
  deref_and_jump(a2,async__input_4_7);
  *reasonp++ =  a2;
  goto async__input_4_17;
  };
  goto async__input_4_17;
 case functor_linecount_1:
  x1 = arg(x0, 0);
  unify(x1, a3);
  a0 = cdr_of(a0);
  execute(async__input_4_0);
  goto async__input_4_ext_interrupt;
 case functor_feof_1:
 async__input_4_13:
  switch (ptagof(a2)) {
 case FUNCTOR:
 async__input_4_14:
  if (!isgobj(a2)) goto async__input_4_17;
  if (!isclass(a2,pointer)) goto async__input_4_17;
  
{
#ifdef ASYNCIO
  struct biobuf *biob = BIOBuf(a2);
  struct iobuf *iob = &(biob->ibuf);
  if (iob->ptr == iob->lim) {
    switch (fill_buf(iob)) {
    case -1: goto async__input_4_17;
    case -2: goto async__input_4_17;
    case 0: x1 = makeint(1); break;
    case 1: x1 = makeint(0); break;
    }
  } else {
    x1 = makeint(0);
  }
#else
  goto async__input_4_17;
#endif
}
  x2 = arg(x0, 0);
  unify_value(x2, x1);
  a0 = cdr_of(a0);
  execute(async__input_4_clear_reason);
  goto async__input_4_ext_interrupt;
 case VARREF:
  deref_and_jump(a2,async__input_4_13);
  *reasonp++ =  a2;
  goto async__input_4_17;
  };
  goto async__input_4_17;
  };
  goto async__input_4_17;
 case VARREF:
  deref_and_jump(x0,async__input_4_2);
  *reasonp++ =  x0;
  goto async__input_4_17;
  };
  goto async__input_4_17;
 case ATOMIC:
  if (a0 != NILATOM) goto async__input_4_17;
 async__input_4_15:
  switch (ptagof(a2)) {
 case FUNCTOR:
 async__input_4_16:
  if (!isgobj(a2)) goto async__input_4_17;
  if (!isclass(a2,pointer)) goto async__input_4_17;
  
{
#ifdef ASYNCIO
  struct biobuf *biob = BIOBuf(a2);
  struct iobuf *inb = &(biob->ibuf);
  struct iobuf *outb = &(biob->obuf);
  if (inb->fd != outb->fd) {
    if (close(inb->fd) != 0) {
      fatalp("close", "Error in closing asynchronous input");
    }
    close_asynchronous_io_stream(inb->fd);
  }
  if (outb->fd == -1) {
    free(inb->buf);
    free(outb->buf);
  }
  inb->fd = -1;
#else
  goto async__input_4_17;
#endif
}
  proceed();
 case VARREF:
  deref_and_jump(a2,async__input_4_15);
  *reasonp++ =  a2;
  goto async__input_4_17;
  };
  goto async__input_4_17;
 case VARREF:
  deref_and_jump(a0,async__input_4_1);
  *reasonp++ =  a0;
  goto async__input_4_17;
  };
  goto async__input_4_17;
 async__input_4_17:
  alternative(async__input_4_clear_reason);
 async__input_4_18:
  switch (ptagof(a1)) {
 case CONS:
  a1 = cdr_of(a1);
  execute(async__input_4_clear_reason);
  goto async__input_4_ext_interrupt;
 case VARREF:
  deref_and_jump(a1,async__input_4_18);
  *reasonp++ =  a1;
  goto async__input_4_interrupt;
  };
  goto async__input_4_interrupt;
 async__input_4_ext_interrupt:
  reasonp = 0l;
 async__input_4_interrupt:
  toppred = &predicate_unix_xasync__input_4;
  goto interrupt_4;
 }

 async__output_4_top: {
  q x0, x1, x2, x3, x4, x5;
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  a3 = qp->args[3];
  qp = qp->next;
 async__output_4_clear_reason:
  reasonp = reasons;
 async__output_4_0:
 async__output_4_1:
  switch (ptagof(a0)) {
 case CONS:
  x0 = car_of(a0);
 async__output_4_2:
  switch (ptagof(x0)) {
 case ATOMIC:
  if (!isint(x0)) goto async__output_4_15;
 async__output_4_3:
  gblt_integer(x0,async__output_4_15);
 async__output_4_4:
  switch (ptagof(a2)) {
 case FUNCTOR:
 async__output_4_5:
  if (!isgobj(a2)) goto async__output_4_15;
  if (!isclass(a2,pointer)) goto async__output_4_15;
  
{
#ifdef ASYNCIO
  struct biobuf *biob = BIOBuf(a2);
  struct iobuf *iob = &(biob->obuf);
  if (iob->ptr == iob->lim) {
    switch (write_buf(iob)) {
    case -1: goto async__output_4_15;
    case -2: goto async__output_4_15;
    case 1: break;
    }
  }
  *iob->ptr++ = intval(x0);
#else
  goto async__output_4_15;
#endif
}
  a0 = cdr_of(a0);
  execute(async__output_4_clear_reason);
  goto async__output_4_ext_interrupt;
 case VARREF:
  deref_and_jump(a2,async__output_4_4);
  *reasonp++ =  a2;
  goto async__output_4_15;
  };
  goto async__output_4_15;
 case FUNCTOR:
  switch (symval(functor_of(x0))) {
 case functor_linecount_1:
  x1 = arg(x0, 0);
  unify(x1, a3);
  a0 = cdr_of(a0);
  execute(async__output_4_0);
  goto async__output_4_ext_interrupt;
 case functor_putc_1:
  
#ifndef ASYNCIO
#endif

  x2 = cdr_of(a0);
  allocp[0] = x2;
  x3 = arg(x0, 0);
  allocp[1] = x3;
  x1 = makecons(&allocp[0]);
  a0 = x1;
  allocp += 2;
  execute(async__output_4_clear_reason);
  goto async__output_4_ext_interrupt;
 case functor_fwrite_2:
  x1 = arg(x0, 0);
 async__output_4_6:
  switch (ptagof(x1)) {
 case FUNCTOR:
 async__output_4_7:
  if (!isgobj(x1)) goto async__output_4_15;
 async__output_4_8:
  gblt_is_string(x1,async__output_4_15);
  gblt_size_of_string(x1,x2,async__output_4_15);
  gblt_element_size_of_string(x1,x3,async__output_4_15);
 async__output_4_9:
  if (x3 != makeint(8L)) goto async__output_4_15;
 async__output_4_10:
  switch (ptagof(a2)) {
 case FUNCTOR:
 async__output_4_11:
  if (!isgobj(a2)) goto async__output_4_15;
  if (!isclass(a2,pointer)) goto async__output_4_15;
 async__output_4_12:
  if (!isgobj(x1)) goto async__output_4_15;
  if (!isclass(x1,byte__string)) goto async__output_4_15;
  
{
#ifdef ASYNCIO
  struct biobuf *biob = BIOBuf(a2);
  struct iobuf *iob = &(biob->obuf);
  char *str = KLIC2C(x1);
  int len =intval(x2);
  int room = iob->lim - iob->ptr;
  while (iob->ptr + len >= iob->lim) {
    BCOPY(str, iob->ptr, room);
    len -= room;
    str += room;
    iob->ptr += room;
    switch (write_buf(iob)) {
    case -1: goto async__output_4_15;
    case -2: goto async__output_4_15;
    case 1: break;
    }
  }
  BCOPY(str, iob->ptr, len);
  iob->ptr += len;
#else
  goto async__output_4_15;
#endif
}
  x4 = arg(x0, 1);
  unify_value(x4, x2);
  a0 = cdr_of(a0);
  execute(async__output_4_clear_reason);
  goto async__output_4_ext_interrupt;
 case VARREF:
  deref_and_jump(a2,async__output_4_10);
  *reasonp++ =  a2;
  goto async__output_4_15;
  };
  goto async__output_4_15;
 case VARREF:
  deref_and_jump(x1,async__output_4_6);
  *reasonp++ =  x1;
  goto async__output_4_15;
  };
  goto async__output_4_15;
  };
  goto async__output_4_15;
 case VARREF:
  deref_and_jump(x0,async__output_4_2);
  *reasonp++ =  x0;
  goto async__output_4_15;
  };
  goto async__output_4_15;
 case ATOMIC:
  if (a0 != NILATOM) goto async__output_4_15;
 async__output_4_13:
  switch (ptagof(a2)) {
 case FUNCTOR:
 async__output_4_14:
  if (!isgobj(a2)) goto async__output_4_15;
  if (!isclass(a2,pointer)) goto async__output_4_15;
  
{
#ifdef ASYNCIO
  struct biobuf *biob = BIOBuf(a2);
  struct iobuf *inb = &(biob->ibuf);
  struct iobuf *outb = &(biob->obuf);
  if (outb->ptr != outb->buf) {
    switch (write_buf(outb)) {
    case -1: goto async__output_4_15;
    case -2: break;
    case 1: break;
    }
  }
  if (inb->fd != outb->fd) {
    if (close(outb->fd) != 0) {
      fatalp("close", "Error in closing asynchronous output");
    }
    close_asynchronous_io_stream(outb->fd);
  }
  if (inb->fd == -1) {
    free(inb->buf);
    free(outb->buf);
  }
  outb->fd = -1;
#else
  goto async__output_4_15;
#endif
}
  proceed();
 case VARREF:
  deref_and_jump(a2,async__output_4_13);
  *reasonp++ =  a2;
  goto async__output_4_15;
  };
  goto async__output_4_15;
 case VARREF:
  deref_and_jump(a0,async__output_4_1);
  *reasonp++ =  a0;
  goto async__output_4_15;
  };
  goto async__output_4_15;
 async__output_4_15:
  otherwise(async__output_4_interrupt);
 async__output_4_16:
  switch (ptagof(a0)) {
 case CONS:
  x0 = car_of(a0);
 async__output_4_17:
  switch (ptagof(x0)) {
 case FUNCTOR:
  switch (symval(functor_of(x0))) {
 case functor_fwrite_2:
  x1 = arg(x0, 1);
  x2 = makeint(-1L);
  unify_value(x1, x2);
  a0 = cdr_of(a0);
  execute(async__output_4_0);
  goto async__output_4_ext_interrupt;
 case functor_fwrite_1:
  allocp[0] = makesym(functor_fwrite_2);
  x2 = arg(x0, 0);
  allocp[1] = x2;
  allocp[2] = x3 = makeref(&allocp[2]);
  x1 = makefunctor(&allocp[0]);
  x5 = cdr_of(a0);
  allocp[3] = x5;
  allocp[4] = x1;
  x4 = makecons(&allocp[3]);
  a0 = x4;
  allocp += 5;
  execute(async__output_4_0);
  goto async__output_4_ext_interrupt;
 case functor_fflush_1:
 async__output_4_18:
  switch (ptagof(a2)) {
 case FUNCTOR:
 async__output_4_19:
  if (!isgobj(a2)) goto async__output_4_20;
  if (!isclass(a2,pointer)) goto async__output_4_20;
  
{
#ifdef ASYNCIO
  struct biobuf *biob = BIOBuf(a2);
  struct iobuf *iob = &(biob->obuf);
  if (iob->ptr != iob->buf) {
    switch (write_buf(iob)) {
    case -1: goto async__output_4_20;
    case -2: goto async__output_4_20;
    case 1: break;
    }
  }
#else
  goto async__output_4_20;
#endif
}
  x1 = arg(x0, 0);
  x2 = makeint(0L);
  unify_value(x1, x2);
  a0 = cdr_of(a0);
  execute(async__output_4_clear_reason);
  goto async__output_4_ext_interrupt;
 case VARREF:
  deref_and_jump(a2,async__output_4_18);
  *reasonp++ =  a2;
  goto async__output_4_20;
  };
  goto async__output_4_20;
  };
  goto async__output_4_20;
 case VARREF:
  deref_and_jump(x0,async__output_4_17);
  *reasonp++ =  x0;
  goto async__output_4_20;
  };
  goto async__output_4_20;
 case VARREF:
  deref_and_jump(a0,async__output_4_16);
  *reasonp++ =  a0;
  goto async__output_4_20;
  };
  goto async__output_4_20;
 async__output_4_20:
  otherwise(async__output_4_interrupt);
 async__output_4_21:
  switch (ptagof(a0)) {
 case CONS:
  x0 = car_of(a0);
 async__output_4_22:
  switch (ptagof(x0)) {
 case FUNCTOR:
  switch (symval(functor_of(x0))) {
 case functor_fflush_1:
  x1 = arg(x0, 0);
  x2 = makeint(-1L);
  unify_value(x1, x2);
  a0 = cdr_of(a0);
  execute(async__output_4_0);
  goto async__output_4_ext_interrupt;
 case functor_sync_1:
 async__output_4_23:
  switch (ptagof(a2)) {
 case FUNCTOR:
 async__output_4_24:
  if (!isgobj(a2)) goto async__output_4_25;
  if (!isclass(a2,pointer)) goto async__output_4_25;
  
#ifdef ASYNCIO
{
  struct biobuf *biob = BIOBuf(a2);
  struct iobuf *outb = &(biob->obuf);
  switch (write_buf(outb)) {
  case -1: goto async__output_4_25;
  case -2: goto async__output_4_25;
  case 1: break;
  }
}
#endif

  x1 = arg(x0, 0);
  x2 = makeint(0L);
  unify_value(x1, x2);
  a0 = cdr_of(a0);
  execute(async__output_4_clear_reason);
  goto async__output_4_ext_interrupt;
 case VARREF:
  deref_and_jump(a2,async__output_4_23);
  *reasonp++ =  a2;
  goto async__output_4_25;
  };
  goto async__output_4_25;
  };
  goto async__output_4_25;
 case VARREF:
  deref_and_jump(x0,async__output_4_22);
  *reasonp++ =  x0;
  goto async__output_4_25;
  };
  goto async__output_4_25;
 case VARREF:
  deref_and_jump(a0,async__output_4_21);
  *reasonp++ =  a0;
  goto async__output_4_25;
  };
  goto async__output_4_25;
 async__output_4_25:
  otherwise(async__output_4_interrupt);
 async__output_4_26:
  switch (ptagof(a0)) {
 case CONS:
  x0 = car_of(a0);
 async__output_4_27:
  switch (ptagof(x0)) {
 case FUNCTOR:
  if (functor_of(x0) != makesym(functor_sync_1)) goto async__output_4_28;
  x1 = arg(x0, 0);
  x2 = makeint(-1L);
  unify_value(x1, x2);
  a0 = cdr_of(a0);
  execute(async__output_4_0);
  goto async__output_4_ext_interrupt;
 case VARREF:
  deref_and_jump(x0,async__output_4_27);
  *reasonp++ =  x0;
  goto async__output_4_28;
  };
  goto async__output_4_28;
 case VARREF:
  deref_and_jump(a0,async__output_4_26);
  *reasonp++ =  a0;
  goto async__output_4_28;
  };
  goto async__output_4_28;
 async__output_4_28:
  alternative(async__output_4_clear_reason);
 async__output_4_29:
  switch (ptagof(a1)) {
 case CONS:
  a1 = cdr_of(a1);
  execute(async__output_4_clear_reason);
  goto async__output_4_ext_interrupt;
 case VARREF:
  deref_and_jump(a1,async__output_4_29);
  *reasonp++ =  a1;
  goto async__output_4_interrupt;
  };
  goto async__output_4_interrupt;
 async__output_4_ext_interrupt:
  reasonp = 0l;
 async__output_4_interrupt:
  goto interrupt_4;
 }

 system_2_top: {
  q x0;
  a0 = qp->args[0];
  a1 = qp->args[1];
  qp = qp->next;
 system_2_clear_reason:
  reasonp = reasons;
 system_2_0:
 system_2_1:
  switch (ptagof(a0)) {
 case FUNCTOR:
 system_2_2:
  if (!isgobj(a0)) goto system_2_interrupt;
  if (!isclass(a0,byte__string)) goto system_2_interrupt;
  
{
  char *buf = KLIC2C(a0);
  x0 = makeint(system(buf));
  free(buf);
}
  unify_value(a1, x0);
  proceed();
 case VARREF:
  deref_and_jump(a0,system_2_1);
  *reasonp++ =  a0;
  goto system_2_interrupt;
  };
  goto system_2_interrupt;
 system_2_ext_interrupt:
  reasonp = 0l;
 system_2_interrupt:
  toppred = &predicate_unix_xsystem_2;
  goto interrupt_2;
 }

 cd_2_top: {
  q x0;
  a0 = qp->args[0];
  a1 = qp->args[1];
  qp = qp->next;
 cd_2_clear_reason:
  reasonp = reasons;
 cd_2_0:
 cd_2_1:
  switch (ptagof(a0)) {
 case FUNCTOR:
 cd_2_2:
  if (!isgobj(a0)) goto cd_2_interrupt;
  if (!isclass(a0,byte__string)) goto cd_2_interrupt;
  
{
  char *buf = KLIC2C(a0);
  x0 = makeint(chdir(buf));
  free(buf);
}
  unify_value(a1, x0);
  proceed();
 case VARREF:
  deref_and_jump(a0,cd_2_1);
  *reasonp++ =  a0;
  goto cd_2_interrupt;
  };
  goto cd_2_interrupt;
 cd_2_ext_interrupt:
  reasonp = 0l;
 cd_2_interrupt:
  toppred = &predicate_unix_xcd_2;
  goto interrupt_2;
 }

 unlink_2_top: {
  q x0;
  a0 = qp->args[0];
  a1 = qp->args[1];
  qp = qp->next;
 unlink_2_clear_reason:
  reasonp = reasons;
 unlink_2_0:
 unlink_2_1:
  switch (ptagof(a0)) {
 case FUNCTOR:
 unlink_2_2:
  if (!isgobj(a0)) goto unlink_2_interrupt;
  if (!isclass(a0,byte__string)) goto unlink_2_interrupt;
  
{
  char *path = KLIC2C(a0);
  x0 = makeint(unlink(path));
  free(path);
}
  unify_value(a1, x0);
  proceed();
 case VARREF:
  deref_and_jump(a0,unlink_2_1);
  *reasonp++ =  a0;
  goto unlink_2_interrupt;
  };
  goto unlink_2_interrupt;
 unlink_2_ext_interrupt:
  reasonp = 0l;
 unlink_2_interrupt:
  toppred = &predicate_unix_xunlink_2;
  goto interrupt_2;
 }

 mktemp_2_top: {
  q x0;
  a0 = qp->args[0];
  a1 = qp->args[1];
  qp = qp->next;
 mktemp_2_clear_reason:
  reasonp = reasons;
 mktemp_2_0:
 mktemp_2_1:
  switch (ptagof(a0)) {
 case FUNCTOR:
 mktemp_2_2:
  if (!isgobj(a0)) goto mktemp_2_interrupt;
  if (!isclass(a0,byte__string)) goto mktemp_2_interrupt;
  
{
  char *template = KLIC2C(a0);
  char *real_template = (char *)malloc_check(strlen(template)+7);
  char *mktemp();
  int result;
  (void)strcpy(real_template, template);
  (void)strcat(real_template, "XXXXXX");
  result = mkstemp(real_template);
  x0 = C2KLIC(real_template, allocp);
  allocp = heapp;
  free(template);
  free(real_template);
}
  unify_value(a1, x0);
  proceed();
 case VARREF:
  deref_and_jump(a0,mktemp_2_1);
  *reasonp++ =  a0;
  goto mktemp_2_interrupt;
  };
  goto mktemp_2_interrupt;
 mktemp_2_ext_interrupt:
  reasonp = 0l;
 mktemp_2_interrupt:
  toppred = &predicate_unix_xmktemp_2;
  goto interrupt_2;
 }

 access_3_top: {
  q x0;
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  qp = qp->next;
 access_3_clear_reason:
  reasonp = reasons;
 access_3_0:
 access_3_1:
  switch (ptagof(a0)) {
 case FUNCTOR:
 access_3_2:
  if (!isgobj(a0)) goto access_3_interrupt;
  if (!isclass(a0,byte__string)) goto access_3_interrupt;
 access_3_3:
  if (!isint(a1)) goto access_3_4;
  
{
  char *path = KLIC2C(a0);
  x0 = makeint(access(path, intval(a1)));
  free(path);
}
  unify_value(a2, x0);
  proceed();
 access_3_4:
  if (!isref(a1)) goto access_3_interrupt;
  deref_and_jump(a1,access_3_3);
  *reasonp++ =  a1;
  goto access_3_interrupt;
 case VARREF:
  deref_and_jump(a0,access_3_1);
  *reasonp++ =  a0;
  goto access_3_interrupt;
  };
  goto access_3_interrupt;
 access_3_ext_interrupt:
  reasonp = 0l;
 access_3_interrupt:
  toppred = &predicate_unix_xaccess_3;
  goto interrupt_3;
 }

 chmod_3_top: {
  q x0;
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  qp = qp->next;
 chmod_3_clear_reason:
  reasonp = reasons;
 chmod_3_0:
 chmod_3_1:
  switch (ptagof(a0)) {
 case FUNCTOR:
 chmod_3_2:
  if (!isgobj(a0)) goto chmod_3_interrupt;
  if (!isclass(a0,byte__string)) goto chmod_3_interrupt;
 chmod_3_3:
  if (!isint(a1)) goto chmod_3_4;
  
{
  char *path = KLIC2C(a0);
  x0 = makeint(chmod(path, intval(a1)));
  free(path);
}
  unify_value(a2, x0);
  proceed();
 chmod_3_4:
  if (!isref(a1)) goto chmod_3_interrupt;
  deref_and_jump(a1,chmod_3_3);
  *reasonp++ =  a1;
  goto chmod_3_interrupt;
 case VARREF:
  deref_and_jump(a0,chmod_3_1);
  *reasonp++ =  a0;
  goto chmod_3_interrupt;
  };
  goto chmod_3_interrupt;
 chmod_3_ext_interrupt:
  reasonp = 0l;
 chmod_3_interrupt:
  toppred = &predicate_unix_xchmod_3;
  goto interrupt_3;
 }

 umask_1_top: {
  q x0;
  a0 = qp->args[0];
  qp = qp->next;
 umask_1_clear_reason:
  reasonp = reasons;
 umask_1_0:
  
{
  x0 = makeint(umask(0));
  (void) umask(intval(x0));
}
  unify_value(a0, x0);
  proceed();
 umask_1_ext_interrupt:
  reasonp = 0l;
 umask_1_interrupt:
  toppred = &predicate_unix_xumask_1;
  goto interrupt_1;
 }

 umask_2_top: {
  q x0;
  a0 = qp->args[0];
  a1 = qp->args[1];
  qp = qp->next;
 umask_2_clear_reason:
  reasonp = reasons;
 umask_2_0:
 umask_2_1:
  if (!isint(a1)) goto umask_2_2;
  
{
  x0 = makeint(umask(0));
  (void) umask(intval(a1));
}
  unify_value(a0, x0);
  proceed();
 umask_2_2:
  if (!isref(a1)) goto umask_2_interrupt;
  deref_and_jump(a1,umask_2_1);
  *reasonp++ =  a1;
  goto umask_2_interrupt;
 umask_2_ext_interrupt:
  reasonp = 0l;
 umask_2_interrupt:
  toppred = &predicate_unix_xumask_2;
  goto interrupt_2;
 }

 getenv_2_top: {
  q x0;
  a0 = qp->args[0];
  a1 = qp->args[1];
  qp = qp->next;
 getenv_2_clear_reason:
  reasonp = reasons;
 getenv_2_0:
 getenv_2_1:
  switch (ptagof(a0)) {
 case FUNCTOR:
 getenv_2_2:
  if (!isgobj(a0)) goto getenv_2_interrupt;
  if (!isclass(a0,byte__string)) goto getenv_2_interrupt;
  
{
  extern char *getenv();
  char *str = KLIC2C(a0);
  char *value = getenv(str);
  free(str);
  if (value==0) {
    x0 = makeint(0);
  } else {
    x0 = C2KLIC(value, allocp);
    allocp = heapp;
  }
}
  unify_value(a1, x0);
  proceed();
 case VARREF:
  deref_and_jump(a0,getenv_2_1);
  *reasonp++ =  a0;
  goto getenv_2_interrupt;
  };
  goto getenv_2_interrupt;
 getenv_2_ext_interrupt:
  reasonp = 0l;
 getenv_2_interrupt:
  toppred = &predicate_unix_xgetenv_2;
  goto interrupt_2;
 }

 putenv_2_top: {
  q x0;
  a0 = qp->args[0];
  a1 = qp->args[1];
  qp = qp->next;
 putenv_2_clear_reason:
  reasonp = reasons;
 putenv_2_0:
 putenv_2_1:
  switch (ptagof(a0)) {
 case FUNCTOR:
 putenv_2_2:
  if (!isgobj(a0)) goto putenv_2_interrupt;
  if (!isclass(a0,byte__string)) goto putenv_2_interrupt;
  
{
  char *str = KLIC2C(a0);
  x0 = makeint(putenv(str));
}
  unify_value(a1, x0);
  proceed();
 case VARREF:
  deref_and_jump(a0,putenv_2_1);
  *reasonp++ =  a0;
  goto putenv_2_interrupt;
  };
  goto putenv_2_interrupt;
 putenv_2_ext_interrupt:
  reasonp = 0l;
 putenv_2_interrupt:
  toppred = &predicate_unix_xputenv_2;
  goto interrupt_2;
 }

 kill_3_top: {
  q x0;
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  qp = qp->next;
 kill_3_clear_reason:
  reasonp = reasons;
 kill_3_0:
 kill_3_1:
  if (!isint(a0)) goto kill_3_4;
 kill_3_2:
  if (!isint(a1)) goto kill_3_3;
  
{
  x0 = makeint(kill(intval(a0),intval(a1)));
}
  unify_value(a2, x0);
  proceed();
 kill_3_3:
  if (!isref(a1)) goto kill_3_interrupt;
  deref_and_jump(a1,kill_3_2);
  *reasonp++ =  a1;
  goto kill_3_interrupt;
 kill_3_4:
  if (!isref(a0)) goto kill_3_interrupt;
  deref_and_jump(a0,kill_3_1);
  *reasonp++ =  a0;
  goto kill_3_interrupt;
 kill_3_ext_interrupt:
  reasonp = 0l;
 kill_3_interrupt:
  toppred = &predicate_unix_xkill_3;
  goto interrupt_3;
 }

 fork_1_top: {
  q x0;
  a0 = qp->args[0];
  qp = qp->next;
 fork_1_clear_reason:
  reasonp = reasons;
 fork_1_0:
  x0 = makeint(fork());
  unify_value(a0, x0);
  proceed();
 fork_1_ext_interrupt:
  reasonp = 0l;
 fork_1_interrupt:
  toppred = &predicate_unix_xfork_1;
  goto interrupt_1;
 }

 fork__with__pipes_1_top: {
  q x0, x1, x2, x3, x4;
  a0 = qp->args[0];
  qp = qp->next;
 fork__with__pipes_1_clear_reason:
  reasonp = reasons;
 fork__with__pipes_1_0:
  
{
  int fd1[2], fd2[2];
  long pid;
  if (pipe(fd1) != 0) goto fork__with__pipes_1_1;
  Fdopen(fd1[0],x1,fork__with__pipes_1_1,"r"); Fdopen(fd1[1],x2,fork__with__pipes_1_1,"w");
  if (pipe(fd2) != 0) goto fork__with__pipes_1_1;
  Fdopen(fd2[0],x3,fork__with__pipes_1_1,"r"); Fdopen(fd2[1],x4,fork__with__pipes_1_1,"w");
  pid = fork();
  if (pid==0) {
    fclose(FilePointer(x2));
    fclose(FilePointer(x3));
  } else {
    fclose(FilePointer(x1));
    fclose(FilePointer(x4));
  }
  x0 = makeint(pid);
}
  a1 = a0;
  a2 = x1;
  a3 = x4;
  a4 = x3;
  a5 = x2;
  a0 = x0;
  execute(fork__with__pipes_2F1_240_6_clear_reason);
  goto fork__with__pipes_2F1_240_6_ext_interrupt;
 fork__with__pipes_1_1:
  otherwise(fork__with__pipes_1_interrupt);
  x0 = makesym(atom_abnormal);
  unify_value(a0, x0);
  proceed();
 fork__with__pipes_1_ext_interrupt:
  reasonp = 0l;
 fork__with__pipes_1_interrupt:
  toppred = &predicate_unix_xfork__with__pipes_1;
  goto interrupt_1;
 }

 fork__with__pipes_2F1_240_6_top: {
  q x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12;
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  a3 = qp->args[3];
  a4 = qp->args[4];
  a5 = qp->args[5];
  qp = qp->next;
 fork__with__pipes_2F1_240_6_clear_reason:
  reasonp = reasons;
 fork__with__pipes_2F1_240_6_0:
 fork__with__pipes_2F1_240_6_1:
  if (!isint(a0)) goto fork__with__pipes_2F1_240_6_3;
  x0 = makeint(0L);
  gblt_not_eq(a0,x0,fork__with__pipes_2F1_240_6_2);
  allocp[0] = makesym(functor_parent_3);
  allocp[1] = a0;
  allocp[2] = x2 = makeref(&allocp[2]);
  allocp[3] = x3 = makeref(&allocp[3]);
  x1 = makefunctor(&allocp[0]);
  allocp += 4;
  unify_value(a1, x1);
  generic_arg[0] = a2;
  x4 = makefunctor(&string_const_2);
  generic_arg[1] = x4;
  x5 = NILATOM;
  generic_arg[2] = x5;
  x6 = makefunctor(&string_const_1);
  generic_arg[3] = x6;
  new_generic(file__io_g_new, 4, x7, 0);
  unify(x2, x7);
  x8 = NILATOM;
  generic_arg[0] = x8;
  x9 = makefunctor(&string_const_1);
  generic_arg[1] = x9;
  generic_arg[2] = a3;
  x10 = makefunctor(&string_const_3);
  generic_arg[3] = x10;
  new_generic(file__io_g_new, 4, x11, 0);
  unify(x3, x11);
  proceed();
 fork__with__pipes_2F1_240_6_2:
  x1 = makeint(0L);
  gblt_eq(a0,x1,fork__with__pipes_2F1_240_6_interrupt);
  allocp[0] = makesym(functor_child_2);
  allocp[1] = x3 = makeref(&allocp[1]);
  allocp[2] = x4 = makeref(&allocp[2]);
  x2 = makefunctor(&allocp[0]);
  allocp += 3;
  unify_value(a1, x2);
  generic_arg[0] = a4;
  x5 = makefunctor(&string_const_2);
  generic_arg[1] = x5;
  x6 = NILATOM;
  generic_arg[2] = x6;
  x7 = makefunctor(&string_const_1);
  generic_arg[3] = x7;
  new_generic(file__io_g_new, 4, x8, 0);
  unify(x3, x8);
  x9 = NILATOM;
  generic_arg[0] = x9;
  x10 = makefunctor(&string_const_1);
  generic_arg[1] = x10;
  generic_arg[2] = a5;
  x11 = makefunctor(&string_const_3);
  generic_arg[3] = x11;
  new_generic(file__io_g_new, 4, x12, 0);
  unify(x4, x12);
  proceed();
 fork__with__pipes_2F1_240_6_3:
  if (!isref(a0)) goto fork__with__pipes_2F1_240_6_interrupt;
  deref_and_jump(a0,fork__with__pipes_2F1_240_6_1);
  *reasonp++ =  a0;
  goto fork__with__pipes_2F1_240_6_interrupt;
 fork__with__pipes_2F1_240_6_ext_interrupt:
  reasonp = 0l;
 fork__with__pipes_2F1_240_6_interrupt:
  toppred = &predicate_unix_xfork__with__pipes_2F1_240_6;
  goto interrupt_6;
 }

 argc_1_top: {
  q x0;
  a0 = qp->args[0];
  qp = qp->next;
 argc_1_clear_reason:
  reasonp = reasons;
 argc_1_0:
  x0 = makeint(command_argc);
  unify_value(a0, x0);
  proceed();
 argc_1_ext_interrupt:
  reasonp = 0l;
 argc_1_interrupt:
  goto interrupt_1;
 }

 argv_1_top: {
  q x0;
  a0 = qp->args[0];
  qp = qp->next;
 argv_1_clear_reason:
  reasonp = reasons;
 argv_1_0:
  x0 = makeint(command_argc);
  a1 = x0;
  a2 = a0;
  a0 = makeint(0L);
  execute(make__argv__list_3_clear_reason);
  goto make__argv__list_3_ext_interrupt;
 argv_1_ext_interrupt:
  reasonp = 0l;
 argv_1_interrupt:
  goto interrupt_1;
 }

 make__argv__list_3_top: {
  q x0, x1, x2, x3, x4;
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  qp = qp->next;
 make__argv__list_3_clear_reason:
  reasonp = reasons;
 make__argv__list_3_0:
 make__argv__list_3_1:
  switch (ptagof(a0)) {
 case ATOMIC:
  if (!isint(a0)) goto make__argv__list_3_5;
 make__argv__list_3_2:
 make__argv__list_3_3:
  if (!isint(a1)) goto make__argv__list_3_4;
  gblt_less(a0,a1,make__argv__list_3_5);
  
{
  x0 = C2KLIC(command_argv[intval(a0)], allocp);
  allocp = heapp;
}
  allocp[0] = x2 = makeref(&allocp[0]);
  allocp[1] = x0;
  x1 = makecons(&allocp[0]);
  allocp += 2;
  unify_value(a2, x1);
  x3 = makeint(1L);
  bblt_add_no_check(a0,x3,x4);
  a0 = x4;
  a2 = x2;
  execute(make__argv__list_3_clear_reason);
  goto make__argv__list_3_ext_interrupt;
 make__argv__list_3_4:
  if (!isref(a1)) goto make__argv__list_3_5;
  deref_and_jump(a1,make__argv__list_3_3);
  *reasonp++ =  a1;
  goto make__argv__list_3_5;
 case VARREF:
  deref_and_jump(a0,make__argv__list_3_1);
  *reasonp++ =  a0;
  goto make__argv__list_3_interrupt;
  };
  goto make__argv__list_3_5;
 make__argv__list_3_5:
 make__argv__list_3_6:
  if (isref(a0)) goto make__argv__list_3_7;
  if_not_equal(a0, a1, make__argv__list_3_interrupt);
  x0 = NILATOM;
  unify_value(a2, x0);
  proceed();
 make__argv__list_3_7:
  deref_and_jump(a0,make__argv__list_3_6);
  *reasonp++ =  a0;
  goto make__argv__list_3_interrupt;
 make__argv__list_3_ext_interrupt:
  reasonp = 0l;
 make__argv__list_3_interrupt:
  toppred = &predicate_unix_xmake__argv__list_3;
  goto interrupt_3;
 }

 times_4_top: {
  q x0, x1, x2, x3;
  a0 = qp->args[0];
  a1 = qp->args[1];
  a2 = qp->args[2];
  a3 = qp->args[3];
  qp = qp->next;
 times_4_clear_reason:
  reasonp = reasons;
 times_4_0:
  
{
  /* Let's assume 60Hz clock if HZ is left undefined */
#ifndef HZ
#define HZ 60
#endif
  struct tms buf;
  (void) times(&buf);
  x0 = makeint((int)(buf.tms_utime*1000.0/HZ));
  x1 = makeint((int)(buf.tms_stime*1000.0/HZ));
  x2 = makeint((int)(buf.tms_cutime*1000.0/HZ));
  x3 = makeint((int)(buf.tms_cstime*1000.0/HZ));
}
  unify_value(a0, x0);
  unify_value(a1, x1);
  unify_value(a2, x2);
  unify_value(a3, x3);
  proceed();
 times_4_ext_interrupt:
  reasonp = 0l;
 times_4_interrupt:
  goto interrupt_4;
 }

 dummy_0_top: {
  q x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10;
  qp = qp->next;
 dummy_0_clear_reason:
  reasonp = reasons;
 dummy_0_0:
  allocp[0] = makesym(functor_fseek_3);
  allocp[1] = x1 = makeref(&allocp[1]);
  allocp[2] = x2 = makeref(&allocp[2]);
  allocp[3] = x3 = makeref(&allocp[3]);
  x0 = makefunctor(&allocp[0]);
  allocp[4] = makesym(functor_ftell_1);
  allocp[5] = x5 = makeref(&allocp[5]);
  x4 = makefunctor(&allocp[4]);
  allocp[6] = makesym(functor_fclose_1);
  allocp[7] = x7 = makeref(&allocp[7]);
  x6 = makefunctor(&allocp[6]);
  allocp[8] = NILATOM;
  allocp[9] = x6;
  x8 = makecons(&allocp[8]);
  allocp[10] = x8;
  allocp[11] = x4;
  x9 = makecons(&allocp[10]);
  allocp[12] = x9;
  allocp[13] = x0;
  x10 = makecons(&allocp[12]);
  allocp += 14;
  proceed();
 dummy_0_ext_interrupt:
  reasonp = 0l;
 dummy_0_interrupt:
  goto interrupt_0;
 }
 interrupt_6:
  allocp[7] = a5;
 interrupt_5:
  allocp[6] = a4;
 interrupt_4:
  allocp[5] = a3;
 interrupt_3:
  allocp[4] = a2;
 interrupt_2:
  allocp[3] = a1;
 interrupt_1:
  allocp[2] = a0;
 interrupt_0:
  allocp = interrupt_goal(allocp, toppred, reasonp);
 proceed_label:
  loop_within_module(module_unix);
}
