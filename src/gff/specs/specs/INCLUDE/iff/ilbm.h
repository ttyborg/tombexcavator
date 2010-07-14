#ifndef ILBM_H
#define ILBM_H
/*----------------------------------------------------------------------*
 * ILBM.H  Definitions for InterLeaved BitMap raster image.     1/23/86
 *   09/88 - added CAMG, CCRT, and CRNG typedefs and macros  (cs)
 *
 * By Jerry Morrison and Steve Shaw, Electronic Arts.
 * This software is in the public domain.
 *
 * This version for the Commodore-Amiga computer.
 *----------------------------------------------------------------------*/
#ifndef COMPILER_H
#include "iff/compiler.h"
#endif

#ifndef GRAPHICS_GFX_H
#include "graphics/gfx.h"
#endif

#include "iff/iff.h"

#define ID_ILBM MakeID('I','L','B','M')
#define ID_BMHD MakeID('B','M','H','D')
#define ID_CMAP MakeID('C','M','A','P')
#define ID_GRAB MakeID('G','R','A','B')
#define ID_DEST MakeID('D','E','S','T')
#define ID_SPRT MakeID('S','P','R','T')
#define ID_CAMG MakeID('C','A','M','G')
#define ID_CRNG MakeID('C','R','N','G')
#define ID_CCRT MakeID('C','C','R','T')
#define ID_BODY MakeID('B','O','D','Y')



/* ---------- BitMapHeader ---------------------------------------------*/

typedef UBYTE Masking;      /* Choice of masking technique.*/
#define mskNone                 0
#define mskHasMask              1
#define mskHasTransparentColor  2
#define mskLasso                3

typedef UBYTE Compression;   /* Choice of compression algorithm applied to
     * each row of the source and mask planes. "cmpByteRun1" is the byte run
     * encoding generated by Mac's PackBits. See Packer.h . */
#define cmpNone      0
#define cmpByteRun1  1

/* Aspect ratios: The proper fraction xAspect/yAspect represents the pixel
 * aspect ratio pixel_width/pixel_height.
 *
 * For the 4 Amiga display modes:
 *   320 x 200: 10/11  (these pixels are taller than they are wide)
 *   320 x 400: 20/11
 *   640 x 200:  5/11
 *   640 x 400: 10/11      */
#define x320x200Aspect 10
#define y320x200Aspect 11
#define x320x400Aspect 20
#define y320x400Aspect 11
#define x640x200Aspect  5
#define y640x200Aspect 11
#define x640x400Aspect 10
#define y640x400Aspect 11

/* A BitMapHeader is stored in a BMHD chunk. */
typedef struct {
    UWORD w, h;         /* raster width & height in pixels */
    WORD  x, y;         /* position for this image */
    UBYTE nPlanes;      /* # source bitplanes */
    Masking masking;      /* masking technique */
    Compression compression;   /* compression algoithm */
    UBYTE pad1;         /* UNUSED.  For consistency, put 0 here.*/
    UWORD transparentColor;   /* transparent "color number" */
    UBYTE xAspect, yAspect;   /* aspect ratio, a rational number x/y */
    WORD  pageWidth, pageHeight;  /* source "page" size in pixels */
    } BitMapHeader;

/* RowBytes computes the number of bytes in a row, from the width in pixels.*/
#define RowBytes(w)   (((w) + 15) >> 4 << 1)


/* ---------- ColorRegister --------------------------------------------*/
/* A CMAP chunk is a packed array of ColorRegisters (3 bytes each). */
typedef struct {
    UBYTE red, green, blue;   /* MUST be UBYTEs so ">> 4" won't sign extend.*/
    } ColorRegister;

/* Use this constant instead of sizeof(ColorRegister). */
#define sizeofColorRegister  3

typedef WORD Color4;   /* Amiga RAM version of a color-register,
          * with 4 bits each RGB in low 12 bits.*/

/* Maximum number of bitplanes in RAM. Current Amiga max w/dual playfield. */
#define MaxAmDepth 6

/* ---------- Point2D --------------------------------------------------*/
/* A Point2D is stored in a GRAB chunk. */
typedef struct {
    WORD x, y;      /* coordinates (pixels) */
    } Point2D;

/* ---------- DestMerge ------------------------------------------------*/
/* A DestMerge is stored in a DEST chunk. */
typedef struct {
    UBYTE depth;   /* # bitplanes in the original source */
    UBYTE pad1;      /* UNUSED; for consistency store 0 here */
    UWORD planePick;   /* how to scatter source bitplanes into destination */
    UWORD planeOnOff;   /* default bitplane data for planePick */
    UWORD planeMask;   /* selects which bitplanes to store into */
    } DestMerge;

/* ---------- SpritePrecedence -----------------------------------------*/
/* A SpritePrecedence is stored in a SPRT chunk. */
typedef UWORD SpritePrecedence;

/* ---------- Camg Amiga Viewport Mode ---------------------------------*/
/* A Commodore Amiga ViewPort->Modes is stored in a CAMG chunk. */
/* The chunk's content is declared as a LONG. */
typedef struct {
   ULONG ViewModes;
   } CamgChunk;

/* ---------- CRange cycling chunk -------------------------------------*/
/* A CRange is store in a CRNG chunk. */
typedef struct {
    WORD  pad1;              /* reserved for future use; store 0 here */
    WORD  rate;      /* 60/sec=16384, 30/sec=8192, 1/sec=16384/60=273 */
    WORD  active;     /* bit0 set = active, bit 1 set = reverse */
    UBYTE low, high;   /* lower and upper color registers selected */
    } CRange;

/* ---------- Ccrt (Graphicraft) cycling chunk -------------------------*/
/* A Ccrt is stored in a CCRT chunk. */
typedef struct {
   WORD  direction;  /* 0=don't cycle, 1=forward, -1=backwards */
   UBYTE start;      /* range lower */
   UBYTE end;        /* range upper */
   LONG  seconds;    /* seconds between cycling */
   LONG  microseconds; /* msecs between cycling */
   WORD  pad;        /* future exp - store 0 here */
   } CcrtChunk;


/* ---------- ILBM Writer Support Routines -----------------------------*/

/* Note: Just call PutCk to write a BMHD, GRAB, DEST, SPRT, or CAMG
 * chunk. As below. */
#define PutBMHD(context, bmHdr)  \
    PutCk(context, ID_BMHD, sizeof(BitMapHeader), (BYTE *)bmHdr)
#define PutGRAB(context, point2D)  \
    PutCk(context, ID_GRAB, sizeof(Point2D), (BYTE *)point2D)
#define PutDEST(context, destMerge)  \
    PutCk(context, ID_DEST, sizeof(DestMerge), (BYTE *)destMerge)
#define PutSPRT(context, spritePrec)  \
    PutCk(context, ID_SPRT, sizeof(SpritePrecedence), (BYTE *)spritePrec)
#define PutCAMG(context, camg)  \
    PutCk(context, ID_CAMG, sizeof(CamgChunk),(BYTE *)camg)
#define PutCRNG(context, crng)  \
    PutCk(context, ID_CRNG, sizeof(CRange),(BYTE *)crng)
#define PutCCRT(context, ccrt)  \
    PutCk(context, ID_CCRT, sizeof(CcrtChunk),(BYTE *)ccrt)

#ifdef FDwAT

/* Initialize a BitMapHeader record for a full-BitMap ILBM picture.
 * This gets w, h, and nPlanes from the BitMap fields BytesPerRow, Rows, and
 * Depth. It assumes you want  w = bitmap->BytesPerRow * 8 .
 * CLIENT_ERROR if bitmap->BytesPerRow isn't even, as required by ILBM format.
 *
 * If (pageWidth, pageHeight) is (320, 200), (320, 400), (640, 200), or
 * (640, 400) this sets (xAspect, yAspect) based on those 4 Amiga display
 * modes. Otherwise, it sets them to (1, 1).
 * 
 * After calling this, store directly into the BitMapHeader if you want to
 * override any settings, e.g. to make nPlanes smaller, to reduce w a little,
 * or to set a position (x, y) other than (0, 0).*/
extern IFFP InitBMHdr(BitMapHeader *, struct BitMap *,
        /*  bmHdr,          bitmap  */
     int,     int,         int,              WORD,      WORD);
 /*  masking, compression, transparentColor, pageWidth, pageHeight */
 /*  Masking, Compression, UWORD -- are the desired types, but get
  *  compiler warnings if use them.               */

/* Output a CMAP chunk to an open FORM ILBM write context. */
extern IFFP PutCMAP(GroupContext *, WORD *,   UBYTE);
      /*  context,        colorMap, depth  */

/* This procedure outputs a BitMap as an ILBM's BODY chunk with
 * bitplane and mask data. Compressed if bmHdr->compression == cmpByteRun1.
 * If the "mask" argument isn't NULL, it merges in the mask plane, too.
 * (A fancier routine could write a rectangular portion of an image.)
 * This gets Planes (bitplane ptrs) from "bitmap".
 *
 * CLIENT_ERROR if bitmap->Rows != bmHdr->h, or if
 * bitmap->BytesPerRow != RowBytes(bmHdr->w), or if
 * bitmap->Depth < bmHdr->nPlanes, or if bmHdr->nPlanes > MaxAmDepth, or if
 * bufsize < MaxPackedSize(bitmap->BytesPerRow), or if
 * bmHdr->compression > cmpByteRun1. */
extern IFFP PutBODY(
    GroupContext *, struct BitMap *, BYTE *, BitMapHeader *, BYTE *, LONG);
    /*  context,           bitmap,   mask,   bmHdr,         buffer, bufsize */

#else /*not FDwAT*/

extern IFFP InitBMHdr();
extern IFFP PutCMAP();
extern IFFP PutBODY();

#endif FDwAT

/* ---------- ILBM Reader Support Routines -----------------------------*/

/* Note: Just call IFFReadBytes to read a BMHD, GRAB, DEST, SPRT, or CAMG
 * chunk. As below. */
#define GetBMHD(context, bmHdr)  \
    IFFReadBytes(context, (BYTE *)bmHdr, sizeof(BitMapHeader))

#define GetGRAB(context, point2D)  \
    IFFReadBytes(context, (BYTE *)point2D, sizeof(Point2D))
#define GetDEST(context, destMerge)  \
    IFFReadBytes(context, (BYTE *)destMerge, sizeof(DestMerge))
#define GetSPRT(context, spritePrec)  \
    IFFReadBytes(context, (BYTE *)spritePrec, sizeof(SpritePrecedence))
#define GetCAMG(context, camg)  \
    IFFReadBytes(context, (BYTE *)camg, sizeof(CamgChunk))
#define GetCRNG(context, crng)  \
    IFFReadBytes(context, (BYTE *)crng, sizeof(CRange))
#define GetCCRT(context, ccrt)  \
    IFFReadBytes(context, (BYTE *)ccrt, sizeof(CcrtChunk))


/* GetBODY can handle a file with up to 16 planes plus a mask.*/
#define MaxSrcPlanes 16+1

#ifdef FDwAT

/* Input a CMAP chunk from an open FORM ILBM read context.
 * This converts to an Amiga color map: 4 bits each of red, green, blue packed
 * into a 16 bit color register.
 * pNColorRegs is passed in as a pointer to a UBYTE variable that holds
 * the number of ColorRegisters the caller has space to hold. GetCMAP sets
 * that variable to the number of color registers actually read.*/
extern IFFP GetCMAP(GroupContext *, WORD *,   UBYTE *);
      /*  context,        colorMap, pNColorRegs  */

/* GetBODY reads an ILBM's BODY into a client's bitmap, de-interleaving and
 * decompressing.
 *
 * Caller should first compare bmHdr dimensions (rowWords, h, nPlanes) with
 * bitmap dimensions, and consider reallocating the bitmap.
 * If file has more bitplanes than bitmap, this reads first few planes (low
 * order ones). If bitmap has more bitplanes, the last few are untouched.
 * This reads the MIN(bmHdr->h, bitmap->Rows) rows, discarding the bottom
 * part of the source or leaving the bottom part of the bitmap untouched.
 *
 * GetBODY returns CLIENT_ERROR if asked to perform a conversion it doesn't
 * handle. It only understands compression algorithms cmpNone and cmpByteRun1.
 * The filed row width (# words) must agree with bitmap->BytesPerRow.
 *
 * Caller should use bmHdr.w; GetBODY only uses it to compute the row width
 * in words. Pixels to the right of bmHdr.w are not defined.
 *
 * [TBD] In the future, GetBODY could clip the stored image horizontally or
 * fill (with transparentColor) untouched parts of the destination bitmap.
 *
 * GetBODY stores the mask plane, if any, in the buffer pointed to by mask.
 * If mask == NULL, GetBODY will skip any mask plane. If
 * (bmHdr.masking != mskHasMask) GetBODY just leaves the caller's mask alone.
 *
 * GetBODY needs a buffer large enough for two compressed rows.
 * It returns CLIENT_ERROR if bufsize < 2 * MaxPackedSize(bmHdr.rowWords * 2).
 *
 * GetBODY can handle a file with up to MaxSrcPlanes planes. It returns
 * CLIENT_ERROR if the file has more. (Could be due to a bum file, though.)
 * If GetBODY fails, itt might've modified the client's bitmap. Sorry.*/
extern IFFP GetBODY(
    GroupContext *, struct BitMap *, BYTE *, BitMapHeader *, BYTE *, LONG);
    /*  context,           bitmap,   mask,   bmHdr,         buffer, bufsize */

/* [TBD] Add routine(s) to create masks when reading ILBMs whose
 * masking != mskHasMask. For mskNone, create a rectangular mask. For
 * mskHasTransparentColor, create a mask from transparentColor. For mskLasso,
 * create an "auto mask" by filling transparent color from the edges. */

#else /*not FDwAT*/

extern IFFP GetCMAP();
extern IFFP GetBODY();

#endif FDwAT

#endif ILBM_H

