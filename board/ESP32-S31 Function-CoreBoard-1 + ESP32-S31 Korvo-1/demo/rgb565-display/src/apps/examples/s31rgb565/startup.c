/****************************************************************************
 * apps/examples/s31rgb565/startup.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/utsname.h>

#include <errno.h>
#include <sched.h>
#include <stdio.h>

int s31rgb565_main(int argc, char *argv[]);
int nsh_main(int argc, char *argv[]);

int s31rgb565_start_main(int argc, char *argv[])
{
  struct utsname info;
  pid_t pid;

  if (uname(&info) == 0)
    {
      printf("OPENVELA_RGB565_START sysname=%s release=%s "
             "version=%s machine=%s\n",
             info.sysname, info.release, info.version, info.machine);
    }
  else
    {
      printf("OPENVELA_RGB565_START uname_failed=%d\n", errno);
    }

  fflush(stdout);

  /* Keep the init task available for the interactive NSH console. */

  pid = task_create("s31rgb565", CONFIG_EXAMPLES_S31RGB565_PRIORITY,
                    CONFIG_EXAMPLES_S31RGB565_STACKSIZE,
                    s31rgb565_main, NULL);
  if (pid < 0)
    {
      printf("s31rgb565: background start failed: %d; entering NSH\n",
             errno);
    }
  else
    {
      printf("s31rgb565: background pid=%ld; entering NSH\n", (long)pid);
    }

  return nsh_main(argc, argv);
}
