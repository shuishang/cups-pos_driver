

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


/*
 * 'main()' - Main entry and processing of driver.
 */
int
main(int argc,
     char *argv[])
{
  if (argc != 6)
  {
    fprintf (stderr, "ERROR: Unknow Command!\n");
    while(--argc >= 0)
    {
      debug("argv[%d]: %s\n", argc, argv[argc]);
    }
    return (1);
  }
  return (0);
}
