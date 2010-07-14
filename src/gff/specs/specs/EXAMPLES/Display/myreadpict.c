/** myReadPict.c ************************************************************
 *
 * Read an ILBM raster image file.               23-Jan-86.
 *
 * Modified version of ReadPict.c 
 *   by Jerry Morrison, Steve Shaw, and Steve Hayes, Electronic Arts.
 *   This software is in the public domain.
 *
 * Modified by C. Scheppner  11/86
 *   Handles CAMG chunks for HAM, etc.
 *   Calls user defined routine getBitMap(ilbmFramePtr) when it
 *      reaches the BODY.
 *   getBitMap() can open a screen of the correct size using
 *      information this rtn places in the ilbmFrame, and returns
 *      a pointer to a BitMap structure.  The BitMap structure
 *      tells myReadPicture where it should load the bit planes.
 *
 * Modified by C. Scheppner  12/86
 *   Loads in CCRT or CRNG chunks (converts CCRT to CRNG)
 * Modified 11-88 to use CCRT, CAMG defs and macros added to ilbm.h
 *                 and existing CRange (not CrngChunk) def in ilbm.h
 ****************************************************************************/

#define LOCAL   static

#include "intuition/intuition.h"
#include "libraries/dos.h"
#include "libraries/dosextens.h"
#include "iff/ilbm.h"
#include "myreadpict.h"    /* cs */

/* Define size of a temporary buffer used in unscrambling the ILBM rows.*/
#define bufSz 512

/*------------ ILBM reader -----------------------------------------------*/
/* ILBMFrame is our "client frame" for reading FORMs ILBM in an IFF file.
 * We allocate one of these on the stack for every LIST or FORM encountered
 * in the file and use it to hold BMHD & CMAP properties. We also allocate
 * an initial one for the whole file.
 * We allocate a new GroupContext (and initialize it by OpenRIFF or
 * OpenRGroup) for every group (FORM, CAT, LIST, or PROP) encountered. It's
 * just a context for reading (nested) chunks.
 *
 * If we were to scan the entire example file outlined below:
 *    reading          proc(s)                new               new
 *
 * --whole file--   myReadPicture+ReadIFF GroupContext        ILBMFrame
 * CAT              ReadICat                GroupContext
 *   LIST           GetLiILBM+ReadIList       GroupContext        ILBMFrame
 *     PROP ILBM    GetPrILBM                   GroupContext
 *       CMAP       GetCMAP
 *       BMHD       GetBMHD
 *     FORM ILBM    GetFoILBM                   GroupContext        ILBMFrame
 *       BODY       GetBODY
 *     FORM ILBM    GetFoILBM                   GroupContext        ILBMFrame
 *       BODY       GetBODY
 *   FORM ILBM      GetFoILBM                 GroupContext        ILBMFrame
 */

/* NOTE: For a small version of this program, set Fancy to 0.
 * That'll compile a program that reads a single FORM ILBM in a file, which
 * is what DeluxePaint produces. It'll skip all LISTs and PROPs in the input
 * file. It will, however, look inside a CAT for a FORM ILBM.
 * That's suitable for 90% of the uses.
 *
 * For a fancier version that handles LISTs and PROPs, set Fancy to 1.
 * That'll compile a program that dives into a LIST, if present, to read
 * the first FORM ILBM. E.g. a DeluxePrint library of images is a LIST of
 * FORMs ILBM.
 *
 * For an even fancier version, set Fancy to 2. That'll compile a program
 * that dives into non-ILBM FORMs, if present, looking for a nested FORM ILBM.
 * E.g. a DeluxeVideo C.S. animated object file is a FORM ANBM containing a
 * FORM ILBM for each image frame. */
#define Fancy  0

/* Global access to client-provided pointers.*/
LOCAL ILBMFrame *giFrame = NULL;   /* "client frame".*/



IFFP handleCAMG(context,frame)
GroupContext *context;
ILBMFrame    *frame;
   {
   IFFP iffp = IFF_OKAY;

   frame->foundCAMG = TRUE;
   iffp = GetCAMG(context, &frame->camgChunk);
   return(iffp);
   }

IFFP handleCRNG(context,frame)
GroupContext *context;
ILBMFrame    *frame;
   {
   IFFP iffp = IFF_OKAY;

   if(frame->cycleCnt < maxCycles)
      {
      iffp = GetCRNG(context,&(frame->crngChunks[frame->cycleCnt]));
      frame->cycleCnt++;
      }
   return(iffp);
   }


IFFP handleCCRT(context,frame)
GroupContext *context;
ILBMFrame    *frame;
   {
   CcrtChunk  ccrtTmp;
   CRange  *ptCrng;
   IFFP iffp = IFF_OKAY;

   if(frame->cycleCnt < maxCycles)
      {
      iffp = GetCCRT(context, &ccrtTmp);
      ptCrng = &(frame->crngChunks[frame->cycleCnt]);
      if(ccrtTmp.direction)  ccrtTmp.direction = -ccrtTmp.direction;
      ptCrng->active = ccrtTmp.direction & 0x03;
      ptCrng->low = ccrtTmp.start;
      ptCrng->high = ccrtTmp.end;

      /* Convert  CCRT secs/msecs to CRNG timimg
       * 0x4000 = max CRNG rate  (cycle every 1 60th sec)
       * This must be divided by # 60th's between cycles
       * seconds to 60th's is easy
       * msecs to 60th's requires division by 16667
       * this is int math so I add 8334 (half 16667) first for rounding
       */
      ptCrng->rate = 0x4000 /
         ((ccrtTmp.seconds * 60)+((ccrtTmp.microseconds+8334)/16667));
      frame->cycleCnt++;
      }
   return(iffp);
   }


/** GetFoILBM() *************************************************************
 *
 * Called via myReadPictureto handle every FORM encountered in an IFF file.
 * Reads FORMs ILBM and skips all others.
 * Inside a FORM ILBM, it stops once it reads a BODY. It complains if it
 * finds no BODY or if it has no BMHD to decode the BODY.
 *
 * Once we find a BODY chunk, we'll call user rtn getBitMap() to
 *    allocate the bitmap and planes (or screen) and then read
 *    the BODY into the planes.
 *
 ****************************************************************************/
LOCAL BYTE bodyBuffer[bufSz];
IFFP GetFoILBM(parent)  GroupContext *parent;
   {
   /*compilerBug register*/ IFFP iffp;
   GroupContext formContext;
   ILBMFrame ilbmFrame;      /* only used for non-clientFrame fields.*/
   struct BitMap *destBitMap;   /* cs */

   /* Handle a non-ILBM FORM. */
   if (parent->subtype != ID_ILBM)
      {
#if Fancy >= 2
      /* Open a non-ILBM FORM and recursively scan it for ILBMs.*/
      iffp = OpenRGroup(parent, &formContext);
      CheckIFFP();
      do {
         iffp = GetF1ChunkHdr(&formContext);
         } while (iffp >= IFF_OKAY);
      if (iffp == END_MARK)
         {
         iffp = IFF_OKAY;   /* then continue scanning the file */
         }
      CloseRGroup(&formContext);
      return(iffp);
#else
      return(IFF_OKAY); /* Just skip this FORM and keep scanning the file.*/
#endif
      }

   ilbmFrame = *(ILBMFrame *)parent->clientFrame;
   iffp = OpenRGroup(parent, &formContext);
   CheckIFFP();

   do switch (iffp = GetFChunkHdr(&formContext)) {
      case ID_BMHD: {
         ilbmFrame.foundBMHD = TRUE;
         iffp = GetBMHD(&formContext, &ilbmFrame.bmHdr);
         break; }
      case ID_CAMG: {       /* cs */
         iffp = handleCAMG(&formContext, &ilbmFrame);
         break; }
      case ID_CRNG: {       /* cs */
         iffp = handleCRNG(&formContext, &ilbmFrame);
         break; }
      case ID_CCRT: {       /* cs */
         iffp = handleCCRT(&formContext, &ilbmFrame);
         break; }
      case ID_CMAP: {
         ilbmFrame.nColorRegs = maxColorReg; /* room for this many */
         iffp = GetCMAP(&formContext, (WORD *)ilbmFrame.colorMap,
                           &ilbmFrame.nColorRegs);
         break; }
      case ID_BODY: {       /* cs */
         if (!ilbmFrame.foundBMHD)
            {
            iffp = BAD_FORM;   /* No BMHD chunk! */
            }
         else
            {
            if(destBitMap=(struct BitMap *)getBitMap(&ilbmFrame))
               {
               iffp = GetBODY( &formContext,
                               destBitMap,
                               NULL,
                               &ilbmFrame.bmHdr,
                               bodyBuffer,
                               bufSz);
               if (iffp == IFF_OKAY) iffp = IFF_DONE;   /* Eureka */
               *giFrame = ilbmFrame; /* copy fields to client frame */
               }
            else
               {
               iffp = CLIENT_ERROR;   /* not enough RAM for the bitmap */
               }
            }
         break; }

      case END_MARK: {
         iffp = BAD_FORM;
         break; }

      } while (iffp >= IFF_OKAY);
      /* loop if valid ID of ignored chunk or a
       * subroutine returned IFF_OKAY (no errors).*/

   if (iffp != IFF_DONE)  return(iffp);

   CloseRGroup(&formContext);
   return(iffp);
   }

/** Notes on extending GetFoILBM ********************************************
 *
 * To read more kinds of chunks, just add clauses to the switch statement.
 * To read more kinds of property chunks (GRAB, CAMG, etc.) add clauses to
 * the switch statement in GetPrILBM, too.
 *
 * To read a FORM type that contains a variable number of data chunks--e.g.
 * a FORM FTXT with any number of CHRS chunks--replace the ID_BODY case with
 * an ID_CHRS case that doesn't set iffp = IFF_DONE, and make the END_MARK
 * case do whatever cleanup you need.
 *
 ****************************************************************************/


/** GetPrILBM() *************************************************************
 *
 * Called via myReadPicture to handle every PROP encountered in an IFF file.
 * Reads PROPs ILBM and skips all others.
 *
 ****************************************************************************/
#if Fancy
IFFP GetPrILBM(parent)  GroupContext *parent;  {
   /*compilerBug register*/ IFFP iffp;
   GroupContext propContext;
   ILBMFrame *ilbmFrame = (ILBMFrame *)parent->clientFrame;

   if (parent->subtype != ID_ILBM)
      return(IFF_OKAY);   /* just continue scaning the file */

   iffp = OpenRGroup(parent, &propContext);
   CheckIFFP();

   do switch (iffp = GetPChunkHdr(&propContext)) {
      case ID_BMHD: {
         ilbmFrame->foundBMHD = TRUE;
         iffp = GetBMHD(&propContext, &ilbmFrame->bmHdr);
         break; }
      case ID_CAMG: {       /* cs */
         iffp = handleCAMG(&propContext, ilbmFrame);
         break; }
      case ID_CRNG: {       /* cs */
         iffp = handleCRNG(&propContext, ilbmFrame);
         break; }
      case ID_CCRT: {       /* cs */
         iffp = handleCCRT(&propContext, ilbmFrame);
         break; }
      case ID_CMAP: {
         ilbmFrame->nColorRegs = maxColorReg; /* room for this many */
         iffp = GetCMAP(&propContext,
                          (WORD *)&ilbmFrame->colorMap,
                             &ilbmFrame->nColorRegs);
         break; }
      } while (iffp >= IFF_OKAY);
        /* loop if valid ID of ignored chunk or a
         * subroutine returned IFF_OKAY (no errors).*/

   CloseRGroup(&propContext);
   return(iffp == END_MARK ? IFF_OKAY : iffp);
   }
#endif

/** GetLiILBM() *************************************************************
 *
 * Called via myReadPicture to handle every LIST encountered in an IFF file.
 *
 ****************************************************************************/
#if Fancy
IFFP GetLiILBM(parent)  GroupContext *parent;  {
    ILBMFrame newFrame;   /* allocate a new Frame */

    newFrame = *(ILBMFrame *)parent->clientFrame;  /* copy parent frame */

    return( ReadIList(parent, (ClientFrame *)&newFrame) );
    }
#endif

/** myReadPicture() ********************************************************/
IFFP myReadPicture(file,iFrame)
   LONG file;
   ILBMFrame *iFrame;   /* Top level "client frame".*/
   {
   IFFP iffp = IFF_OKAY;

#if Fancy
   iFrame->clientFrame.getList = GetLiILBM;
   iFrame->clientFrame.getProp = GetPrILBM;
#else
   iFrame->clientFrame.getList = SkipGroup;
   iFrame->clientFrame.getProp = SkipGroup;
#endif
   iFrame->clientFrame.getForm = GetFoILBM;
   iFrame->clientFrame.getCat  = ReadICat ;

   /* Initialize the top-level client frame's property settings to the
    * program-wide defaults. This example just records that we haven't read
    * any BMHD property or CMAP color registers yet. For the color map, that
    * means the default is to leave the machine's color registers alone.
    * If you want to read a property like GRAB, init it here to (0, 0). */

   iFrame->foundBMHD  = FALSE;
   iFrame->nColorRegs = 0;
   iFrame->foundCAMG  = FALSE;    /* cs */
   iFrame->cycleCnt   = 0;        /* cs */
   giFrame = iFrame;

  /* Store a pointer to the client's frame in a global variable so that
   * GetFoILBM can update client's frame when done.  Why do we have so
   * many frames & frame pointers floating around causing confusion?
   * Because IFF supports PROPs which apply to all FORMs in a LIST,
   * unless a given FORM overrides some property.  
   * When you write code to read several FORMs,
   * it is ssential to maintain a frame at each level of the syntax
   * so that the properties for the LIST don't get overwritten by any
   * properties specified by individual FORMs.
   * We decided it was best to put that complexity into this one-FORM example,
   * so that those who need it later will have a useful starting place.
   */

   iffp = ReadIFF(file, (ClientFrame *)iFrame);
   return(iffp);
   }

