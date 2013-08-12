
/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/ppd.h>
#include <cups/raster.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "kc.h"

/*
 * For Debuging...
 */

#if 0
#define DEBUG
#endif

#ifdef DEBUG
	#define debug(format, args...) printf(format, ##args)
#else
	#define debug(format, args...)
#endif


void debugFunc(int argc, char *argv[]);
void debugFunc(int argc, char *argv[])
{
#ifdef DEBUG
  
  int i;
  cups_dest_t *dests;
  int num_dests = cupsGetDests(&dests);
  cups_dest_t *dest;

  /*
   * Print main() args...
   */
  debug("driver: rastertokc.\n");
  for(i = 0; i < argc; i++)
    debug("argv[%d]: %s\n", i, argv[i]);


  /*
   * Print printer options
   */
  if ((dest = cupsGetDest(argv[0], NULL, num_dests, dests)) == NULL)
    debug("Unable to get dest %s.\n", argv[0]);
  else
    debug("Printer: %s.\n", argv[0]);

  for(i = 0; i < dest->num_options; i++)
    debug("%s: %s\n", dest->options[i].name,dest->options[i].value);

  cupsFreeDests(num_dests, dests);

#endif
}


/*
 * Macros...
 */
#define pwrite(s,n) fwrite((s), 1, (n), stdout)


/*
 * Globals...
 */
int 		Model = 0;			/* Model number */
int		Canceled;		/* Has the current job been canceled? */

int Floyd16x16[16][16] = /* Traditional Floyd ordered dither */
{ {   0, 128,  32, 160,   8, 136,  40, 168,  2,  130,  34, 162,  10, 138,  42, 170 },
  { 192,  64, 224,  96, 200,  72, 232, 104, 194,  66, 226,  98, 202,  74, 234, 106 },
  {  48, 176,  16, 144,  56, 184,  24, 152,  50, 178,  18, 146,  58, 186,  26, 154 },
  { 240, 112, 208,  80, 248, 120, 216,  88, 242, 114, 210,  82, 250, 122, 218,  90 },
  {  12, 140,  44, 172,   4, 132,  36, 164,  14, 142,  46, 174,   6, 134,  38, 166 },
  { 204,  76, 236, 108, 196,  68, 228, 100, 206,  78, 238, 110, 198,  70, 230, 102 },
  {  60, 188,  28, 156,  52, 180,  20, 148,  62, 190,  30, 158,  54, 182,  22, 150 },
  { 252, 124, 220,  92, 244, 116, 212,  84, 254, 126, 222,  94, 246, 118, 214,  86 },
  {   3, 131,  35, 163,  11, 139,  43, 171,   1, 129,  33, 161,   9, 137,  41, 169 },
  { 195,  67, 227,  99, 203,  75, 235, 107, 193,  65, 225,  97, 201,  73, 233, 105 },
  {  51, 179,  19, 147,  59, 187,  27, 155,  49, 177,  17, 145,  57, 185,  25, 153 },
  { 243, 115, 211,  83, 251, 123, 219,  91, 241, 113, 209,  81, 249, 121, 217,  89 },
  {  15, 143,  47, 175,   7, 135,  39, 167,  13, 141,  45, 173,   5, 133,  37, 165 },
  { 207,  79, 239, 111, 199,  71, 231, 103, 205,  77, 237, 109, 197,  69, 229, 101 },
  {  63, 191,  31, 159,  55, 183,  23, 151,  61, 189,  29, 157,  53, 181,  21, 149 },
  { 254, 127, 223,  95, 247, 119, 215,  87, 253, 125, 221,  93, 245, 117, 213,  85 } };

int Floyd8x8[8][8] = 
{ { 0, 32, 8, 40, 2, 34, 10, 42 },
  { 48, 16, 56, 24, 50, 18, 58, 26 },
  { 12, 44, 4, 36, 14, 46, 6, 38 },
  { 60, 28, 52, 20, 62, 30, 54, 22 },
  { 3, 35, 11, 43, 1, 33, 9, 41 },
  { 51, 19, 59, 27, 49, 17, 57, 25 },
  { 15, 47, 7, 39, 13, 45, 5, 37 },
  { 63, 31, 55, 23, 61, 29, 53, 21 } };

int Floyd4x4[4][4] =
{ { 0, 8, 2, 10 },
  { 12, 4, 14, 6 },
  { 3, 11, 1, 9 },
  { 15, 7, 13, 5 } };

int p0[2] = { 0x80, 0 }; /* for kimage to pos command*/
int p1[2] = { 0x40, 0 };
int p2[2] = { 0x20, 0 };
int p3[2] = { 0x10, 0 };
int p4[2] = { 0x08, 0 };
int p5[2] = { 0x04, 0 };
int p6[2] = { 0x02, 0 };
int p7[2] = { 0x01, 0 };

/*
 * Prototypes...
 */

void Setup(int argc, char *argv[]);
void StartPage(int fd);
void EndPage(int argc, char *argv[]);

void CancelJob(int sig);


/*
 * Image functions
 */
void format_K_dither(unsigned char *orgpixels, int xsize, int ysize, unsigned char *despixels);


/*
 * Pos functions
 */
void pixToCmd(unsigned char *src, int nWidth, int nHeight, int nMode, unsigned char *sendbuf);

/*
 * 'Setup()' - Prepare the printer for printing.
 */
void
Setup(int argc, char *argv[])
{
  /* for correct status */
  signal(SIGPIPE, SIG_IGN);
  Canceled = 0;

#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGTERM, CancelJob);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  action.sa_handler = CancelJob;
  sigaction(SIGTERM, &action, NULL);
#else
  signal(SIGTERM, CancelJob);
#endif /* HAVE_SIGSET */

  /* the reset command */
  const char	cmd_reset[] = { 0x1b, 0x40 };
  pwrite(cmd_reset, 2);
  
  char *optstr;

  char cupsDarknessSupport = 1;
  char cmd_darkness[] = { 0x12, 0x23, 0x00};
  const char *cupsDarkness = "cupsDarkness=";
  if ( (optstr = strstr(argv[5], cupsDarkness)) != NULL)
  {
    switch(*(optstr + strlen(cupsDarkness)))
    {
      case 'V':
        cmd_darkness[2] = 0x00;
        break;
      case 'L':
        cmd_darkness[2] = 0x04;
        break;
      case 'S':
        cmd_darkness[2] = 0x08;
        break;
      case 'D':
        cmd_darkness[2] = 0x10;
        break;
      case 'M':
        cmd_darkness[2] = 0x17;
        break;
      case 'H':
        cmd_darkness[2] = 0x1f;
        break;
      case 'N':
      default:
        cupsDarknessSupport = 0;
        break;
    }
    if (cupsDarknessSupport) {
      debug("Darkness Support!\n");
      pwrite(cmd_darkness, 3);
    } else {
      debug("Don't Support Darkness!\n");
    }
  }

  char fontSelectSupport = 1;
  char cmd_selectFont[] = {0x1b, 0x4d, 0x00};
  const char *font = "fontSet=";
  if ( (optstr = strstr(argv[5], font)) != NULL)
  {
    switch(*(optstr + strlen(font)))
    {
      /* 12x24 */
      case '1':
        cmd_selectFont[2] = 0x00;
        break;
      /* 9x17*/
      case '9':
        cmd_selectFont[2] = 0x01;
        break;
      case 'N':
      default:
        fontSelectSupport = 0;
        break;
    }
    if (fontSelectSupport) {
      debug("font Set Support!\n");
      pwrite(cmd_selectFont, 3);
    } else {
      debug("No Font Set Support!\n");
    }
  }
  fflush(stdout);

  cups_dest_t *dests;
  int num_dests = cupsGetDests(&dests);
  cups_dest_t *dest;
  const char *value;

  /*
   * Print printer options
   */
  if ((dest = cupsGetDest(argv[0], NULL, num_dests, dests)) == NULL) {
    debug("Unable to get dest %s.\n", argv[0]);
    return;
  } else {
    debug("Printer: %s.\n", argv[0]);
  }

  value = cupsGetOption("printer-make-and-model", dest->num_options, dest->options);
  if (value) {
    if (strstr(value, "RP80")) {
      Model = KC_RP80;
    } else if (strstr(value, "RP58")) {
      Model = KC_RP58;
    }
  }
  cupsFreeDests(num_dests, dests);
}


/*
 * 'StartPage()' - Start all page of graphics.
 */
void
StartPage(int fd)
{
  int y,i;
  int rasterpage = 0;
  int coping;
  cups_page_header2_t header;
  unsigned char *rasbuf = NULL;
  const unsigned char *rasbufconst;
  int xsizealigned = 0;
  int bytesPerLine = 0;
  unsigned char *sendbuf = NULL;
  cups_raster_t *ras = cupsRasterOpen(fd, CUPS_RASTER_READ);

  while((!Canceled) && cupsRasterReadHeader2(ras, &header))
  {
    /* setup this page */
    rasterpage++;

    if(header.cupsBitsPerColor == 1)
    {

      switch (Model) {
      case KC_RP58:
        bytesPerLine = header.cupsBytesPerLine > 48 ? 48 : header.cupsBytesPerLine;
        break;
      case KC_RP80:
        bytesPerLine = header.cupsBytesPerLine > 72 ? 72 : header.cupsBytesPerLine;
      break;
      }

      if( (rasbufconst = rasbuf = malloc(8 + header.cupsBytesPerLine * header.cupsHeight)) == NULL)
      {
        fprintf(stderr, "ERROR: Unable to allocate memory\n");
        break;
      }

      fprintf(stderr, "INFO: Printing page %d. Copies %d\n", 
              rasterpage, header.NumCopies);
      for (y = 0;(!Canceled) && (y < header.cupsHeight); y ++)
      {
        if (cupsRasterReadPixels(ras, 
                                 rasbuf + 8 + y * header.cupsBytesPerLine, 
                                 header.cupsBytesPerLine) == 0)
          break;
      }
      if (bytesPerLine != header.cupsBytesPerLine) {
        for (y = 1; y < header.cupsHeight; y++) {
          for (i = 0; i < bytesPerLine; i++) {
            *(rasbuf + 8 + y * bytesPerLine + i) = *(rasbufconst + 8 + y * header.cupsBytesPerLine + i);
          }
        }
      }
      rasbuf[0] = 0x1D;
      rasbuf[1] = 0x76;
      rasbuf[2] = 0x30;
      rasbuf[3] = 0x00;
      rasbuf[4] = (unsigned char)(bytesPerLine & 0xff);
      rasbuf[5] = (unsigned char)((bytesPerLine & 0xff00) >> 8);
      rasbuf[6] = (unsigned char)(header.cupsHeight & 0xff);
      rasbuf[7] = (unsigned char)((header.cupsHeight & 0xff00) >> 8);

      for (coping = 1; (!Canceled) && (coping <= header.NumCopies); coping++)
      {
        pwrite(rasbuf, 8 + bytesPerLine * header.cupsHeight);
        fflush(stdout);
      }

    }else if(header.cupsBitsPerColor == 8){

      xsizealigned = (header.cupsBytesPerLine + 7) / 8 * 8;
      switch (Model) {
      case KC_RP58:
        bytesPerLine = xsizealigned > 384 ? 384 : xsizealigned;
        break;
      case KC_RP80:
        bytesPerLine = xsizealigned > 576 ? 576 : xsizealigned;
      break;
      }

      if( ((rasbufconst = rasbuf = malloc(xsizealigned * header.cupsHeight)) == NULL) ||
          ((sendbuf = malloc(8 + bytesPerLine / 8 * header.cupsHeight )) == NULL) )
      {
        fprintf(stderr, "ERROR: Unable to allocate memory\n");
        break;
      }
      memset(rasbuf, 0, sizeof(rasbuf));
      memset(sendbuf, 0, sizeof(sendbuf));

      fprintf(stderr, "INFO: Printing page %d. Copies %d in %d\n", 
              rasterpage, coping, header.NumCopies);
      for (y = 0;(!Canceled) && (y < header.cupsHeight); y ++)
      {
        if (cupsRasterReadPixels(ras, 
                                 rasbuf + y * xsizealigned, 
                                 header.cupsBytesPerLine) == 0)
          break;
      }
      if (bytesPerLine != xsizealigned) {
        for (y = 1; y < header.cupsHeight; y++) {
          for (i = 0; i < bytesPerLine; i++) {
            *(rasbuf + y * bytesPerLine + i) = *(rasbufconst + y * xsizealigned + i);
          }
        }
      }
       /* 256 to 2 */
      format_K_dither(rasbuf, bytesPerLine, header.cupsHeight, rasbuf);
       /* image to cmd */
      pixToCmd(rasbuf, bytesPerLine, header.cupsHeight, 0, sendbuf);
      for (coping = 1; (!Canceled) && (coping <= header.NumCopies); coping++)
      {
        pwrite(sendbuf, 8 + bytesPerLine / 8 * header.cupsHeight);
        fflush(stdout);
      }

    }

    /* page finished */
    if(rasbuf != NULL)
      free(rasbuf);
    if(sendbuf != NULL)
      free(sendbuf);
  }
  cupsRasterClose(ras);
}


/*
 * 'EndPage()' - Finish all page of graphics
 */
void
EndPage(int argc, char *argv[])
{
  
  char cmd_cutmedia[] = { 0x1d, 0x56, 0x42, 0x00};
  const char *noCutMedia = "noCutMedia";
  const char *CutMedia = "CutMedia";

  if ( (strstr(argv[5], CutMedia) != NULL) && 
       (strstr(argv[5], noCutMedia) == NULL) )
  {
    debug("CutMedia Support!\n");
    pwrite(cmd_cutmedia, 4);
  }

  if (Canceled)
    fprintf(stderr, "INFO: Printing Canceled!\n");
  else
    fprintf(stderr, "INFO: Printing Completed!\n");
}


/*
 * 'CancelJob()' - Cancel the current job...
 */

void
CancelJob(int sig)			/* I - Signal */
{
  (void)sig;

  Canceled = 1;
}


/*
 * 'format_K_dither' - iamge to k
 */
void
format_K_dither(unsigned char *orgpixels,  /* 256 gray */
                int xsize,
                int ysize,
                unsigned char *despixels)  /* 2 gray (0, 255)*/
{
  int k = 0;
  int y = 0;
  int x = 0;
  for (y = 0; y < ysize; y++)
  {
    for (x = 0; x < xsize; x++)
    {
      if (orgpixels[k] > Floyd16x16[x & 15][y & 15])
        despixels[k] = 0;/* black */
      else
        despixels[k] = 255;
      k++;
    }
  }

}


/*
 * 'pixToCmd' - image pixels to pos command
 */
void pixToCmd(unsigned char *src, /* org image format 0-black 255-white */
              int nWidth, int nHeight, int nMode, unsigned char *sendbuf)
{
  sendbuf[0] = 0x1d;
  sendbuf[1] = 0x76;
  sendbuf[2] = 0x30;
  sendbuf[3] = (unsigned char) (nMode & 0xff);
  sendbuf[4] = (unsigned char) ((nWidth / 8) & 0xff);/* (xl+xh*256)*8 = nWidth */
  sendbuf[5] = (unsigned char) (((nWidth / 8) & 0xff00) >> 8 );
  sendbuf[6] = (unsigned char) (nHeight & 0xff);/* (yl+yh*256) = nHeight */
  sendbuf[7] = (unsigned char) ((nHeight & 0xff00) >> 8);

  int k;
  int i;
  int srclen = nWidth * nHeight;
  for (i = 8, k = 0; k < srclen; i++ , k += 8)
  {
    sendbuf[i] = p0[src[k] & 0x01] +
                 p1[src[k + 1] & 0x01] +
                 p2[src[k + 2] & 0x01] +
                 p3[src[k + 3] & 0x01] +
                 p4[src[k + 4] & 0x01] +
                 p5[src[k + 5] & 0x01] +
                 p6[src[k + 6] & 0x01] +
                 p7[src[k + 7] & 0x01] ;
  }

}


/*
 * 'main()' - Main entry and processing of driver.
 */
int
main(int argc,
     char *argv[])
{

  int fd;
  debugFunc(argc, argv);
  if ((argc != 6) && (argc != 7))
  {
    fprintf (stderr, "ERROR: Unknow Command in driver rastertokc!\n");
    return (1);
  }

  if (argc == 6)
    fd = 0;
  else if ((fd = open(argv[6], O_RDONLY)) == -1)
  {
    fprintf (stderr, "ERROR: Unable to open raster file!\n");
    sleep(1);
    return (1);
  }
  Setup(argc, argv);
  StartPage(fd);

  if (fd != 0)
    close(fd);

  EndPage(argc, argv);
  return (0);
}
