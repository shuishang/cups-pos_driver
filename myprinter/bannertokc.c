
/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/ppd.h>
#include <cups/raster.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>


/*
 * For Debuging...
 */
#define DEBUG

#ifdef DEBUG
	#define debug(format, args...) printf(format, ##args)
#else
	#define debug(format, args...)
#endif


void debugArgs(int argc, char *argv[]);
void debugArgs(int argc, char *argv[])
{
#ifdef DEBUG
  debug("driver: bannertokc.\n");
  while(--argc >= 0)
    {
      debug("argv[%d]: %s\n", argc, argv[argc]);
    }
#endif
}


/*
 * 'main()' - Main entry and processing of driver.
 */
int
main(int argc,
     char *argv[])
{
  debugArgs(argc, argv);
  if ((argc != 6) && (argc != 7))
  {
    fprintf (stderr, "ERROR: Unknow Command in driver bannertokc!\n");
    return (1);
  }

  return (0);
}
