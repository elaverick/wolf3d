// ID_CA.C

// this has been customized for WOLF

/*
=============================================================================

Id Software Caching Manager
---------------------------

Must be started BEFORE the memory manager, because it needs to get the headers
loaded into the data segment

=============================================================================
*/

#include "ID_HEADS.H"
#ifdef WOLF3D_NATIVE
#include "native_map_codec.h"
#endif
#pragma hdrstop

#pragma warn -pro
#pragma warn -use

#define THREEBYTEGRSTARTS

/*
=============================================================================

						 LOCAL CONSTANTS

=============================================================================
*/

typedef struct
{
#ifdef WOLF3D_NATIVE
  word bit0, bit1;
#else
  unsigned bit0, bit1;
#endif
} huffnode;


#ifdef WOLF3D_NATIVE
#pragma pack(push, 2)
#endif
typedef struct
{
#ifdef WOLF3D_NATIVE
    word RLEWtag;
    int32_t headeroffsets[100];
#else
    unsigned RLEWtag;
    long headeroffsets[100];
#endif
    byte tileinfo[];
} mapfiletype;
#ifdef WOLF3D_NATIVE
#pragma pack(pop)
#endif
#ifdef WOLF3D_NATIVE
_Static_assert(sizeof(maptype) == 38u,
    "Native maptype must retain the original 38-byte disk layout");
_Static_assert(sizeof(mapfiletype) == 402u,
    "Native MAPHEAD prefix must retain its 2-byte-packed layout");
#endif


/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/

byte 		_seg	*tinf;
int			mapon;

unsigned _seg *mapsegs[MAPPLANES];
maptype		_seg	*mapheaderseg[NUMMAPS];
byte		_seg	*audiosegs[NUMSNDCHUNKS];
void		_seg	*grsegs[NUMCHUNKS];

byte		far	grneeded[NUMCHUNKS];
byte		ca_levelbit,ca_levelnum;

int			profilehandle,debughandle;

char		audioname[13]="AUDIO.";

/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/

extern	long	far	CGAhead;
extern	long	far	EGAhead;
extern	byte	CGAdict;
extern	byte	EGAdict;
extern	byte	far	maphead;
extern	byte	mapdict;
extern	byte	far	audiohead;
extern	byte	audiodict;


char extension[5],	// Need a string, not constant to change cache files
     gheadname[10]=GREXT"HEAD.",
     gfilename[10]=GREXT"GRAPH.",
     gdictname[10]=GREXT"DICT.",
     mheadname[10]="MAPHEAD.",
     mfilename[10]="MAPTEMP.",
     aheadname[10]="AUDIOHED.",
     afilename[10]="AUDIOT.";

void CA_CannotOpen(char *string);

#ifdef WOLF3D_NATIVE
int32_t _seg *grstarts;
int32_t _seg *audiostarts;
#else
long *_seg *grstarts;
long *_seg *audiostarts;
#endif

#ifdef GRHEADERLINKED
huffnode	*grhuffman;
#else
huffnode	grhuffman[255];
#endif

#ifdef AUDIOHEADERLINKED
huffnode	*audiohuffman;
#else
huffnode	audiohuffman[255];
#endif


int			grhandle;		// handle to EGAGRAPH
int			maphandle;		// handle to MAPTEMP / GAMEMAPS
int			audiohandle;	// handle to AUDIOT / AUDIO

#ifdef WOLF3D_NATIVE
int32_t chunkcomplen, chunkexplen;
#else
long chunkcomplen, chunkexplen;
#endif

SDMode		oldsoundmode;



#ifdef WOLF3D_NATIVE
void CAL_CarmackExpand (word far *source, word far *dest, unsigned length);
#else
void CAL_CarmackExpand (unsigned far *source, unsigned far *dest, unsigned length);
#endif


#ifdef THREEBYTEGRSTARTS
#define FILEPOSSIZE	3
//#define	GRFILEPOS(c) (*(long far *)(((byte far *)grstarts)+(c)*3)&0xffffff)
long GRFILEPOS(int c)
{
#ifdef WOLF3D_NATIVE
    const byte *table = (const byte *)grstarts;
    size_t offset;
    uint32_t value;
    if (c < 0 || table == NULL) return -1;
    offset = (size_t)c * 3u;
    value = (uint32_t)table[offset] |
        ((uint32_t)table[offset + 1u] << 8) |
        ((uint32_t)table[offset + 2u] << 16);
    return value == 0x00ffffffu ? -1 : (long)value;
#else
    long value;
    int offset = c * 3;
    value = *(long far *)(((byte far *)grstarts) + offset);
    value &= 0x00ffffffl;
    if (value == 0xffffffl) value = -1;
    return value;
#endif
}
#else
#define FILEPOSSIZE	4
#define	GRFILEPOS(c) (grstarts[c])
#endif

/*
=============================================================================

					   LOW LEVEL ROUTINES

=============================================================================
*/

/*
============================
=
= CA_OpenDebug / CA_CloseDebug
=
= Opens a binary file with the handle "debughandle"
=
============================
*/

void CA_OpenDebug (void)
{
	unlink ("DEBUG.TXT");
	debughandle = open("DEBUG.TXT", O_CREAT | O_WRONLY | O_TEXT, S_IREAD | S_IWRITE);
}

void CA_CloseDebug (void)
{
	close (debughandle);
}



/*
============================
=
= CAL_GetGrChunkLength
=
= Gets the length of an explicit length chunk (not tiles)
= The file pointer is positioned so the compressed data can be read in next.
=
============================
*/

void CAL_GetGrChunkLength (int chunk)
{
	lseek(grhandle,GRFILEPOS(chunk),SEEK_SET);
	read(grhandle,&chunkexplen,sizeof(chunkexplen));
	chunkcomplen = GRFILEPOS(chunk+1)-GRFILEPOS(chunk)-4;
}


/*
==========================
=
= CA_FarRead
=
= Read from a file to a far pointer
=
==========================
*/

boolean CA_FarRead (int handle, byte far *dest, long length)
{
#ifdef WOLF3D_NATIVE
    byte *cursor = dest;
    long remaining = length;
    if (handle < 0 || dest == NULL || length < 0) { errno = EINVAL; return false; }
    while (remaining > 0) {
        unsigned request = remaining > 0x7fffffffL ? 0x7fffffffu : (unsigned)remaining;
        int count = (int)read(handle, cursor, request);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { if (count == 0) errno = EIO; return false; }
        cursor += count;
        remaining -= count;
    }
    return true;
#else
    if (length>0xffffl) Quit ("CA_FarRead doesn't support 64K reads yet!");
asm push ds
asm mov bx,[handle]
asm mov cx,[WORD PTR length]
asm mov dx,[WORD PTR dest]
asm mov ds,[WORD PTR dest+2]
asm mov ah,0x3f
asm int 21h
asm pop ds
asm jnc good
    errno = _AX; return false;
good:
asm cmp ax,[WORD PTR length]
asm je done
    errno = EINVFMT; return false;
done:
    return true;
#endif
}

/*
==========================
=
= CA_SegWrite
=
= Write from a file to a far pointer
=
==========================
*/

boolean CA_FarWrite (int handle, byte far *source, long length)
{
#ifdef WOLF3D_NATIVE
    const byte *cursor = source;
    long remaining = length;
    if (handle < 0 || source == NULL || length < 0) { errno = EINVAL; return false; }
    while (remaining > 0) {
        unsigned request = remaining > 0x7fffffffL ? 0x7fffffffu : (unsigned)remaining;
        int count = (int)write(handle, cursor, request);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { if (count == 0) errno = EIO; return false; }
        cursor += count;
        remaining -= count;
    }
    return true;
#else
    if (length>0xffffl) Quit ("CA_FarWrite doesn't support 64K reads yet!");
asm push ds
asm mov bx,[handle]
asm mov cx,[WORD PTR length]
asm mov dx,[WORD PTR source]
asm mov ds,[WORD PTR source+2]
asm mov ah,0x40
asm int 21h
asm pop ds
asm jnc good
    errno = _AX; return false;
good:
asm cmp ax,[WORD PTR length]
asm je done
    errno = ENOMEM; return false;
done:
    return true;
#endif
}

/*
==========================
=
= CA_ReadFile
=
= Reads a file into an allready allocated buffer
=
==========================
*/

boolean CA_ReadFile (char *filename, memptr *ptr)
{
	int handle;
	long size;

	if ((handle = open(filename,O_RDONLY | O_BINARY, S_IREAD)) == -1)
		return false;

	size = filelength (handle);
	if (!CA_FarRead (handle,*ptr,size))
	{
		close (handle);
		return false;
	}
	close (handle);
	return true;
}


/*
==========================
=
= CA_WriteFile
=
= Writes a file from a memory buffer
=
==========================
*/

boolean CA_WriteFile (char *filename, void far *ptr, long length)
{
	int handle;
	long size;

	handle = open(filename,O_CREAT | O_BINARY | O_WRONLY,
				S_IREAD | S_IWRITE | S_IFREG);

	if (handle == -1)
		return false;

	if (!CA_FarWrite (handle,ptr,length))
	{
		close (handle);
		return false;
	}
	close (handle);
	return true;
}



/*
==========================
=
= CA_LoadFile
=
= Allocate space for and load a file
=
==========================
*/

boolean CA_LoadFile (char *filename, memptr *ptr)
{
	int handle;
	long size;

	if ((handle = open(filename,O_RDONLY | O_BINARY, S_IREAD)) == -1)
		return false;

	size = filelength (handle);
	MM_GetPtr (ptr,size);
	if (!CA_FarRead (handle,*ptr,size))
	{
		close (handle);
		return false;
	}
	close (handle);
	return true;
}

/*
============================================================================

		COMPRESSION routines, see JHUFF.C for more

============================================================================
*/



/*
===============
=
= CAL_OptimizeNodes
=
= Goes through a huffman table and changes the 256-511 node numbers to the
= actular address of the node.  Must be called before CAL_HuffExpand
=
===============
*/

void CAL_OptimizeNodes (huffnode *table)
{
#ifdef WOLF3D_NATIVE
    /* Encoded node indices remain offsets in the portable C decoder. */
    (void)table;
#else
    huffnode *node = table;
    int i;
    for (i = 0; i < 255; ++i, ++node) {
        if (node->bit0 >= 256)
            node->bit0 = (unsigned)(table + (node->bit0 - 256));
        if (node->bit1 >= 256)
            node->bit1 = (unsigned)(table + (node->bit1 - 256));
    }
#endif
}



/*
======================
=
= CAL_HuffExpand
=
= Length is the length of the EXPANDED data
= If screenhack, the data is decompressed in four planes directly
= to the screen
=
======================
*/

#ifdef WOLF3D_NATIVE
void CAL_HuffExpand (byte huge *source, byte huge *dest,
    long length, huffnode *hufftable, boolean screenhack)
{
    size_t output_length;
    size_t output_index = 0;
    unsigned node_index = 254;
    unsigned bit_index = 0;

    (void)screenhack;
    if (source == NULL || dest == NULL || hufftable == NULL || length < 0) {
        Quit("CAL_HuffExpand: invalid native arguments");
        return;
    }
    output_length = (size_t)length;

    while (output_index < output_length) {
        unsigned encoded;
        if (bit_index == 8) {
            ++source;
            bit_index = 0;
        }

        encoded = (source[0] & (byte)(1u << bit_index)) != 0
            ? hufftable[node_index].bit1
            : hufftable[node_index].bit0;
        ++bit_index;

        if (encoded < 256u) {
            dest[output_index++] = (byte)encoded;
            node_index = 254;
        } else {
            node_index = encoded - 256u;
            if (node_index >= 255u) {
                Quit("CAL_HuffExpand: invalid Huffman node");
                return;
            }
        }
    }
}
#else
void CAL_HuffExpand (byte huge *source, byte huge *dest,
  long length,huffnode *hufftable, boolean screenhack)
{
//  unsigned bit,byte,node,code;
  unsigned sourceseg,sourceoff,destseg,destoff,endoff;
  huffnode *headptr;
  byte		mapmask;
//  huffnode *nodeon;

  headptr = hufftable+254;	// head node is allways node 254

  source++;	// normalize
  source--;
  dest++;
  dest--;

  if (screenhack)
  {
	mapmask = 1;
asm	mov	dx,SC_INDEX
asm	mov	ax,SC_MAPMASK + 256
asm	out	dx,ax
	length >>= 2;
  }

  sourceseg = FP_SEG(source);
  sourceoff = FP_OFF(source);
  destseg = FP_SEG(dest);
  destoff = FP_OFF(dest);
  endoff = destoff+length;

//
// ds:si source
// es:di dest
// ss:bx node pointer
//

	if (length <0xfff0)
	{

//--------------------------
// expand less than 64k of data
//--------------------------

asm mov	bx,[headptr]

asm	mov	si,[sourceoff]
asm	mov	di,[destoff]
asm	mov	es,[destseg]
asm	mov	ds,[sourceseg]
asm	mov	ax,[endoff]

asm	mov	ch,[si]				// load first byte
asm	inc	si
asm	mov	cl,1

expandshort:
asm	test	ch,cl			// bit set?
asm	jnz	bit1short
asm	mov	dx,[ss:bx]			// take bit0 path from node
asm	shl	cl,1				// advance to next bit position
asm	jc	newbyteshort
asm	jnc	sourceupshort

bit1short:
asm	mov	dx,[ss:bx+2]		// take bit1 path
asm	shl	cl,1				// advance to next bit position
asm	jnc	sourceupshort

newbyteshort:
asm	mov	ch,[si]				// load next byte
asm	inc	si
asm	mov	cl,1				// back to first bit

sourceupshort:
asm	or	dh,dh				// if dx<256 its a byte, else move node
asm	jz	storebyteshort
asm	mov	bx,dx				// next node = (huffnode *)code
asm	jmp	expandshort

storebyteshort:
asm	mov	[es:di],dl
asm	inc	di					// write a decopmpressed byte out
asm	mov	bx,[headptr]		// back to the head node for next bit

asm	cmp	di,ax				// done?
asm	jne	expandshort

//
// perform screenhack if needed
//
asm	test	[screenhack],1
asm	jz	notscreen
asm	shl	[mapmask],1
asm	mov	ah,[mapmask]
asm	cmp	ah,16
asm	je	notscreen			// all four planes done
asm	mov	dx,SC_INDEX
asm	mov	al,SC_MAPMASK
asm	out	dx,ax
asm	mov	di,[destoff]
asm	mov	ax,[endoff]
asm	jmp	expandshort

notscreen:;
	}
	else
	{

//--------------------------
// expand more than 64k of data
//--------------------------

  length--;

asm mov	bx,[headptr]
asm	mov	cl,1

asm	mov	si,[sourceoff]
asm	mov	di,[destoff]
asm	mov	es,[destseg]
asm	mov	ds,[sourceseg]

asm	lodsb			// load first byte

expand:
asm	test	al,cl		// bit set?
asm	jnz	bit1
asm	mov	dx,[ss:bx]	// take bit0 path from node
asm	jmp	gotcode
bit1:
asm	mov	dx,[ss:bx+2]	// take bit1 path

gotcode:
asm	shl	cl,1		// advance to next bit position
asm	jnc	sourceup
asm	lodsb
asm	cmp	si,0x10		// normalize ds:si
asm  	jb	sinorm
asm	mov	cx,ds
asm	inc	cx
asm	mov	ds,cx
asm	xor	si,si
sinorm:
asm	mov	cl,1		// back to first bit

sourceup:
asm	or	dh,dh		// if dx<256 its a byte, else move node
asm	jz	storebyte
asm	mov	bx,dx		// next node = (huffnode *)code
asm	jmp	expand

storebyte:
asm	mov	[es:di],dl
asm	inc	di		// write a decopmpressed byte out
asm	mov	bx,[headptr]	// back to the head node for next bit

asm	cmp	di,0x10		// normalize es:di
asm  	jb	dinorm
asm	mov	dx,es
asm	inc	dx
asm	mov	es,dx
asm	xor	di,di
dinorm:

asm	sub	[WORD PTR ss:length],1
asm	jnc	expand
asm  	dec	[WORD PTR ss:length+2]
asm	jns	expand		// when length = ffff ffff, done

	}

asm	mov	ax,ss
asm	mov	ds,ax

}
#endif

/*
======================
=
= CAL_CarmackExpand
=
= Length is the length of the EXPANDED data
=
======================
*/

#define NEARTAG	0xa7
#define FARTAG	0xa8

#ifdef WOLF3D_NATIVE
void CAL_CarmackExpand (word far *source, word far *dest, unsigned length)
{
    const byte *input = (const byte *)source;
    size_t output_words = (size_t)length / 2u;
    size_t produced = 0;

    while (produced < output_words) {
        uint16_t value = (uint16_t)input[0] | ((uint16_t)input[1] << 8);
        unsigned tag = value >> 8;
        unsigned count = value & 0xffu;
        input += 2;

        if (tag == NEARTAG || tag == FARTAG) {
            if (count == 0) {
                dest[produced++] = (word)((tag << 8) | input[0]);
                ++input;
                continue;
            }
            if (produced + count > output_words) {
                Quit("CAL_CarmackExpand: run exceeds destination");
                return;
            }
            if (tag == NEARTAG) {
                size_t offset = input[0];
                size_t copy_index;
                ++input;
                if (offset == 0 || offset > produced) {
                    Quit("CAL_CarmackExpand: invalid near offset");
                    return;
                }
                copy_index = produced - offset;
                while (count-- != 0)
                    dest[produced++] = dest[copy_index++];
            } else {
                size_t copy_index = (size_t)input[0] | ((size_t)input[1] << 8);
                input += 2;
                if (copy_index >= produced) {
                    Quit("CAL_CarmackExpand: invalid far offset");
                    return;
                }
                while (count-- != 0) {
                    if (copy_index >= produced) {
                        Quit("CAL_CarmackExpand: far copy outside output");
                        return;
                    }
                    dest[produced++] = dest[copy_index++];
                }
            }
        } else {
            dest[produced++] = (unsigned)value;
        }
    }
}
#else
void CAL_CarmackExpand (unsigned far *source, unsigned far *dest, unsigned length)
{
	unsigned	ch,chhigh,count,offset;
	unsigned	far *copyptr, far *inptr, far *outptr;

	length/=2;

	inptr = source;
	outptr = dest;

	while (length)
	{
		ch = *inptr++;
		chhigh = ch>>8;
		if (chhigh == NEARTAG)
		{
			count = ch&0xff;
			if (!count)
			{				// have to insert a word containing the tag byte
				ch |= *((unsigned char far *)inptr)++;
				*outptr++ = ch;
				length--;
			}
			else
			{
				offset = *((unsigned char far *)inptr)++;
				copyptr = outptr - offset;
				length -= count;
				while (count--)
					*outptr++ = *copyptr++;
			}
		}
		else if (chhigh == FARTAG)
		{
			count = ch&0xff;
			if (!count)
			{				// have to insert a word containing the tag byte
				ch |= *((unsigned char far *)inptr)++;
				*outptr++ = ch;
				length --;
			}
			else
			{
				offset = *inptr++;
				copyptr = dest + offset;
				length -= count;
				while (count--)
					*outptr++ = *copyptr++;
			}
		}
		else
		{
			*outptr++ = ch;
			length --;
		}
	}
}
#endif

/*
======================
=
= CA_RLEWcompress
=
======================
*/

long CA_RLEWCompress (unsigned huge *source, long length, unsigned huge *dest,
  unsigned rlewtag)
{
  long complength;
  unsigned value,count,i;
  unsigned huge *start,huge *end;

  start = dest;

  end = source + (length+1)/2;

//
// compress it
//
  do
  {
	count = 1;
	value = *source++;
	while (*source == value && source<end)
	{
	  count++;
	  source++;
	}
	if (count>3 || value == rlewtag)
	{
    //
    // send a tag / count / value string
    //
      *dest++ = rlewtag;
      *dest++ = count;
      *dest++ = value;
    }
    else
    {
    //
    // send word without compressing
    //
      for (i=1;i<=count;i++)
	*dest++ = value;
	}

  } while (source<end);

  complength = 2*(dest-start);
  return complength;
}


/*
======================
=
= CA_RLEWexpand
= length is EXPANDED length
=
======================
*/

#ifdef WOLF3D_NATIVE
void CA_RLEWexpand (word huge *source, unsigned huge *dest, long length, word rlewtag)
{
    size_t output_words;
    size_t produced = 0;
    const word *input = source;

    if (source == NULL || dest == NULL || length < 0 || (length % (long)sizeof(unsigned)) != 0) {
        Quit("CA_RLEWexpand: invalid native arguments");
        return;
    }
    output_words = (size_t)length / sizeof(unsigned);
    while (produced < output_words) {
        word value = *input++;
        if (value == rlewtag) {
            word count = *input++;
            value = *input++;
            if (count == 0 || produced + count > output_words) {
                Quit("CA_RLEWexpand: invalid run length");
                return;
            }
            while (count-- != 0)
                dest[produced++] = value;
        } else {
            dest[produced++] = value;
        }
    }
}
#else
void CA_RLEWexpand (unsigned huge *source, unsigned huge *dest,long length,
  unsigned rlewtag)
{
//  unsigned value,count,i;
  unsigned huge *end;
  unsigned sourceseg,sourceoff,destseg,destoff,endseg,endoff;


//
// expand it
//
#if 0
  do
  {
	value = *source++;
	if (value != rlewtag)
	//
	// uncompressed
	//
	  *dest++=value;
	else
	{
	//
	// compressed string
	//
	  count = *source++;
	  value = *source++;
	  for (i=1;i<=count;i++)
	*dest++ = value;
	}
  } while (dest<end);
#endif

  end = dest + (length)/2;
  sourceseg = FP_SEG(source);
  sourceoff = FP_OFF(source);
  destseg = FP_SEG(dest);
  destoff = FP_OFF(dest);
  endseg = FP_SEG(end);
  endoff = FP_OFF(end);


//
// ax = source value
// bx = tag value
// cx = repeat counts
// dx = scratch
//
// NOTE: A repeat count that produces 0xfff0 bytes can blow this!
//

asm	mov	bx,rlewtag
asm	mov	si,sourceoff
asm	mov	di,destoff
asm	mov	es,destseg
asm	mov	ds,sourceseg

expand:
asm	lodsw
asm	cmp	ax,bx
asm	je	repeat
asm	stosw
asm	jmp	next

repeat:
asm	lodsw
asm	mov	cx,ax		// repeat count
asm	lodsw			// repeat value
asm	rep stosw

next:

asm	cmp	si,0x10		// normalize ds:si
asm  	jb	sinorm
asm	mov	ax,si
asm	shr	ax,1
asm	shr	ax,1
asm	shr	ax,1
asm	shr	ax,1
asm	mov	dx,ds
asm	add	dx,ax
asm	mov	ds,dx
asm	and	si,0xf
sinorm:
asm	cmp	di,0x10		// normalize es:di
asm  	jb	dinorm
asm	mov	ax,di
asm	shr	ax,1
asm	shr	ax,1
asm	shr	ax,1
asm	shr	ax,1
asm	mov	dx,es
asm	add	dx,ax
asm	mov	es,dx
asm	and	di,0xf
dinorm:

asm	cmp     di,ss:endoff
asm	jne	expand
asm	mov	ax,es
asm	cmp	ax,ss:endseg
asm	jb	expand

asm	mov	ax,ss
asm	mov	ds,ax

}
#endif


/*
=============================================================================

					 CACHE MANAGER ROUTINES

=============================================================================
*/


/*
======================
=
= CAL_SetupGrFile
=
======================
*/

void CAL_SetupGrFile (void)
{
	char fname[13];
	int handle;
	memptr compseg;

#ifdef GRHEADERLINKED

	grhuffman = (huffnode *)&EGAdict;
	grstarts = (long _seg *)FP_SEG(&EGAhead);

	CAL_OptimizeNodes (grhuffman);

#else

//
// load ???dict.ext (huffman dictionary for graphics files)
//

	strcpy(fname,gdictname);
	strcat(fname,extension);

	if ((handle = open(fname,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		CA_CannotOpen(fname);

	read(handle, &grhuffman, sizeof(grhuffman));
	close(handle);
	CAL_OptimizeNodes (grhuffman);
//
// load the data offsets from ???head.ext
//
	MM_GetPtr ((memptr *)&grstarts,(NUMCHUNKS+1)*FILEPOSSIZE);

	strcpy(fname,gheadname);
	strcat(fname,extension);

	if ((handle = open(fname,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		CA_CannotOpen(fname);

	CA_FarRead(handle, (memptr)grstarts, (NUMCHUNKS+1)*FILEPOSSIZE);

	close(handle);


#endif

//
// Open the graphics file, leaving it open until the game is finished
//
	strcpy(fname,gfilename);
	strcat(fname,extension);

	grhandle = open(fname, O_RDONLY | O_BINARY);
	if (grhandle == -1)
		CA_CannotOpen(fname);


//
// load the pic and sprite headers into the arrays in the data segment
//
	MM_GetPtr((memptr *)&pictable,NUMPICS*sizeof(pictabletype));
	CAL_GetGrChunkLength(STRUCTPIC);		// position file pointer
	MM_GetPtr(&compseg,chunkcomplen);
	CA_FarRead (grhandle,compseg,chunkcomplen);
	CAL_HuffExpand (compseg, (byte huge *)pictable,NUMPICS*sizeof(pictabletype),grhuffman,false);
	MM_FreePtr(&compseg);
}

//==========================================================================


/*
======================
=
= CAL_SetupMapFile
=
======================
*/

void CAL_SetupMapFile (void)
{
#ifdef WOLF3D_NATIVE
    int header_handle;
    int map_index;
    long header_length;
    long map_file_size;
    long header_required;
    int32_t header_offset;
    uint64_t header_end;
    char fname[13];

    maphandle = -1;
    strcpy(fname, mheadname);
    strcat(fname, extension);
    header_handle = open(fname, O_RDONLY | O_BINARY, S_IREAD);
    if (header_handle < 0) {
        CA_CannotOpen(fname);
        return;
    }

    header_length = filelength(header_handle);
    header_required = (long)(sizeof(word) + 100u * sizeof(int32_t));
    if (header_length < header_required) {
        close(header_handle);
        Quit("CAL_SetupMapFile: truncated MAPHEAD file");
        return;
    }

    MM_GetPtr((memptr *)&tinf, (unsigned long)header_length);
    if (tinf == NULL ||
        !CA_FarRead(header_handle, tinf, header_length)) {
        if (tinf != NULL) MM_FreePtr((memptr *)&tinf);
        tinf = NULL;
        close(header_handle);
        Quit("CAL_SetupMapFile: unable to read MAPHEAD file");
        return;
    }
    close(header_handle);

#ifdef CARMACIZED
    strcpy(fname, "GAMEMAPS.");
#else
    strcpy(fname, mfilename);
#endif
    strcat(fname, extension);
    maphandle = open(fname, O_RDONLY | O_BINARY, S_IREAD);
    if (maphandle < 0) {
        CA_CannotOpen(fname);
        return;
    }

    map_file_size = filelength(maphandle);
    if (map_file_size < 0) {
        close(maphandle);
        maphandle = -1;
        Quit("CAL_SetupMapFile: unable to determine map file size");
        return;
    }

    for (map_index = 0; map_index < NUMMAPS; ++map_index) {
        header_offset =
            ((mapfiletype _seg *)tinf)->headeroffsets[map_index];

        /* -1 is the format's sparse-map sentinel. Other negatives are invalid. */
        if (header_offset == -1) {
            continue;
        }
        if (header_offset < 0) {
            Quit("CAL_SetupMapFile: invalid negative map-header offset");
            return;
        }

        header_end = (uint64_t)(uint32_t)header_offset + sizeof(maptype);
        if (header_end > (uint64_t)map_file_size ||
            lseek(maphandle, (long)header_offset, SEEK_SET) != (long)header_offset) {
            Quit("CAL_SetupMapFile: map header lies outside GAMEMAPS file");
            return;
        }

        MM_GetPtr((memptr *)&mapheaderseg[map_index], sizeof(maptype));
        if (mapheaderseg[map_index] == NULL ||
            !CA_FarRead(
                maphandle,
                (byte far *)mapheaderseg[map_index],
                (long)sizeof(maptype))) {
            if (mapheaderseg[map_index] != NULL) {
                MM_FreePtr((memptr *)&mapheaderseg[map_index]);
            }
            Quit("CAL_SetupMapFile: unable to read map header");
            return;
        }
        MM_SetLock((memptr *)&mapheaderseg[map_index], true);
    }

    for (map_index = 0; map_index < MAPPLANES; ++map_index) {
        MM_GetPtr(
            (memptr *)&mapsegs[map_index],
            64u * 64u * (unsigned long)sizeof(unsigned));
        if (mapsegs[map_index] == NULL) {
            Quit("CAL_SetupMapFile: unable to allocate map plane");
            return;
        }
        MM_SetLock((memptr *)&mapsegs[map_index], true);
    }
#else

	int	i;
	int handle;
	long length,pos;
	char fname[13];

//
// load maphead.ext (offsets and tileinfo for map file)
//
#ifndef MAPHEADERLINKED
	strcpy(fname,mheadname);
	strcat(fname,extension);

	if ((handle = open(fname,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		CA_CannotOpen(fname);

	length = filelength(handle);
	MM_GetPtr ((memptr *)&tinf,length);
	CA_FarRead(handle, tinf, length);
	close(handle);
#else

	tinf = (byte _seg *)FP_SEG(&maphead);

#endif

//
// open the data file
//
#ifdef CARMACIZED
	strcpy(fname,"GAMEMAPS.");
	strcat(fname,extension);

	if ((maphandle = open(fname,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		CA_CannotOpen(fname);
#else
	strcpy(fname,mfilename);
	strcat(fname,extension);

	if ((maphandle = open(fname,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		CA_CannotOpen(fname);
#endif

//
// load all map header
//
	for (i=0;i<NUMMAPS;i++)
	{
		pos = ((mapfiletype	_seg *)tinf)->headeroffsets[i];
		if (pos<0)						// $FFFFFFFF start is a sparse map
			continue;

		MM_GetPtr((memptr *)&mapheaderseg[i],sizeof(maptype));
		MM_SetLock((memptr *)&mapheaderseg[i],true);
		lseek(maphandle,pos,SEEK_SET);
		CA_FarRead (maphandle,(memptr)mapheaderseg[i],sizeof(maptype));
	}

//
// allocate space for 3 64*64 planes
//
	for (i=0;i<MAPPLANES;i++)
	{
		#ifdef WOLF3D_NATIVE
        MM_GetPtr ((memptr *)&mapsegs[i],64*64*sizeof(unsigned));
#else
		MM_GetPtr ((memptr *)&mapsegs[i],64*64*2);
#endif
		MM_SetLock ((memptr *)&mapsegs[i],true);
	}

#endif
}


//==========================================================================


/*
======================
=
= CAL_SetupAudioFile
=
======================
*/

void CAL_SetupAudioFile (void)
{
#ifdef WOLF3D_NATIVE
    int header_handle;
    long header_length;
    long required_length;
    char fname[13];

    audiohandle = -1;
    audiostarts = NULL;

    strcpy(fname, aheadname);
    strcat(fname, extension);
    header_handle = open(fname, O_RDONLY | O_BINARY, S_IREAD);
    if (header_handle < 0) {
        fprintf(stderr, "Audio data not found (%s); continuing without sound effects.\n", fname);
        return;
    }

    header_length = filelength(header_handle);
    required_length = (long)(NUMSNDCHUNKS + 1) * (long)sizeof(int32_t);
    if (header_length != required_length) {
        close(header_handle);
        fprintf(stderr, "Invalid audio header (%s); continuing silently.\n", fname);
        return;
    }

    MM_BombOnError(false);
    MM_GetPtr((memptr *)&audiostarts, (unsigned long)header_length);
    MM_BombOnError(true);
    if (mmerror || audiostarts == NULL ||
        !CA_FarRead(header_handle, (byte far *)audiostarts, header_length)) {
        if (audiostarts != NULL) {
            MM_FreePtr((memptr *)&audiostarts);
        }
        audiostarts = NULL;
        close(header_handle);
        fprintf(stderr, "Unable to read audio header (%s); continuing silently.\n", fname);
        return;
    }
    close(header_handle);

    strcpy(fname, afilename);
    strcat(fname, extension);
    audiohandle = open(fname, O_RDONLY | O_BINARY, S_IREAD);
    if (audiohandle < 0) {
        MM_FreePtr((memptr *)&audiostarts);
        audiostarts = NULL;
        fprintf(stderr, "Audio data file not found (%s); continuing silently.\n", fname);
        return;
    }
#else
	int handle;
	long length;
	char fname[13];

//
// load maphead.ext (offsets and tileinfo for map file)
//
#ifndef AUDIOHEADERLINKED
	strcpy(fname,aheadname);
	strcat(fname,extension);

	if ((handle = open(fname,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		CA_CannotOpen(fname);

	length = filelength(handle);
	MM_GetPtr ((memptr *)&audiostarts,length);
	CA_FarRead(handle, (byte far *)audiostarts, length);
	close(handle);
#else
	audiohuffman = (huffnode *)&audiodict;
	CAL_OptimizeNodes (audiohuffman);
	audiostarts = (long _seg *)FP_SEG(&audiohead);
#endif

//
// open the data file
//
#ifndef AUDIOHEADERLINKED
	strcpy(fname,afilename);
	strcat(fname,extension);

	if ((audiohandle = open(fname,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		CA_CannotOpen(fname);
#else
	if ((audiohandle = open("AUDIO."EXTENSION,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		Quit ("Can't open AUDIO."EXTENSION"!");
#endif
#endif
}

//==========================================================================


/*
======================
=
= CA_Startup
=
= Open all files and load in headers
=
======================
*/

void CA_Startup (void)
{
#ifdef PROFILE
	unlink ("PROFILE.TXT");
	profilehandle = open("PROFILE.TXT", O_CREAT | O_WRONLY | O_TEXT, S_IREAD | S_IWRITE);
#endif

	CAL_SetupMapFile ();
	CAL_SetupGrFile ();
    CAL_SetupAudioFile ();

	mapon = -1;
	ca_levelbit = 1;
	ca_levelnum = 0;

}

//==========================================================================


/*
======================
=
= CA_Shutdown
=
= Closes all files
=
======================
*/

void CA_Shutdown (void)
{
#ifdef PROFILE
	close (profilehandle);
#endif

    if (maphandle >= 0) close(maphandle);
    if (grhandle >= 0) close(grhandle);
    if (audiohandle >= 0) close(audiohandle);
}

//===========================================================================

/*
======================
=
= CA_CacheAudioChunk
=
======================
*/

void CA_CacheAudioChunk (int chunk)
{
	long	pos,compressed;
#ifdef WOLF3D_NATIVE
    int64_t start_offset;
    int64_t end_offset;
    long audio_file_size;
#endif
#ifdef AUDIOHEADERLINKED
	long	expanded;
	memptr	bigbufferseg;
	byte	far *source;
#endif

    if (audiohandle < 0 || audiostarts == NULL ||
        chunk < 0 || chunk >= NUMSNDCHUNKS) {
        return;
    }
	if (audiosegs[chunk])
	{
		MM_SetPurge ((memptr *)&audiosegs[chunk],0);
		return;							// allready in memory
	}

//
// load the chunk into a buffer, either the miscbuffer if it fits, or allocate
// a larger buffer
//
#ifdef WOLF3D_NATIVE
    /*
     * AUDIOHED contains signed 32-bit file offsets. Validate both endpoints
     * before subtracting or allocating so corrupt offsets cannot overflow
     * the signed length or trigger an unbounded allocation.
     */
    start_offset = audiostarts[chunk];
    end_offset = audiostarts[chunk + 1];
    audio_file_size = filelength(audiohandle);
    if (audio_file_size < 0 || start_offset < 0 ||
        end_offset <= start_offset || end_offset > audio_file_size) {
        return;
    }
    pos = (long)start_offset;
    compressed = (long)(end_offset - start_offset);
#else
	pos = audiostarts[chunk];
	compressed = audiostarts[chunk+1]-pos;
    if (pos < 0 || compressed <= 0) {
        return;
    }
#endif

	if (lseek(audiohandle,pos,SEEK_SET) != pos) {
        return;
    }

#ifndef AUDIOHEADERLINKED

	MM_GetPtr ((memptr *)&audiosegs[chunk],compressed);
	if (mmerror)
		return;

    if (!CA_FarRead(audiohandle,audiosegs[chunk],compressed)) {
        MM_FreePtr((memptr *)&audiosegs[chunk]);
    }

#else

	if (compressed<=BUFFERSIZE)
	{
		CA_FarRead(audiohandle,bufferseg,compressed);
		source = bufferseg;
	}
	else
	{
		MM_GetPtr(&bigbufferseg,compressed);
		if (mmerror)
			return;
		MM_SetLock (&bigbufferseg,true);
		CA_FarRead(audiohandle,bigbufferseg,compressed);
		source = bigbufferseg;
	}

	expanded = *(long far *)source;
	source += 4;			// skip over length
	MM_GetPtr ((memptr *)&audiosegs[chunk],expanded);
	if (mmerror)
		goto done;
	CAL_HuffExpand (source,audiosegs[chunk],expanded,audiohuffman,false);

done:
	if (compressed>BUFFERSIZE)
		MM_FreePtr(&bigbufferseg);
#endif
}

//===========================================================================

/*
======================
=
= CA_LoadAllSounds
=
= Purges all sounds, then loads all new ones (mode switch)
=
======================
*/

void CA_LoadAllSounds (void)
{
	unsigned	start,i;

	switch (oldsoundmode)
	{
	case sdm_Off:
		goto cachein;
	case sdm_PC:
		start = STARTPCSOUNDS;
		break;
	case sdm_AdLib:
		start = STARTADLIBSOUNDS;
		break;
	}

	for (i=0;i<NUMSOUNDS;i++,start++)
		if (audiosegs[start])
			MM_SetPurge ((memptr *)&audiosegs[start],3);		// make purgable

cachein:

	switch (SoundMode)
	{
	case sdm_Off:
		return;
	case sdm_PC:
		start = STARTPCSOUNDS;
		break;
	case sdm_AdLib:
		start = STARTADLIBSOUNDS;
		break;
	}

	for (i=0;i<NUMSOUNDS;i++,start++)
		CA_CacheAudioChunk (start);

	oldsoundmode = SoundMode;
}

//===========================================================================


/*
======================
=
= CAL_ExpandGrChunk
=
= Does whatever is needed with a pointer to a compressed chunk
=
======================
*/

void CAL_ExpandGrChunk (int chunk, byte far *source)
{
	long	expanded;


	if (chunk >= STARTTILE8 && chunk < STARTEXTERNS)
	{
	//
	// expanded sizes of tile8/16/32 are implicit
	//

#define BLOCK		64
#define MASKBLOCK	128

		if (chunk<STARTTILE8M)			// tile 8s are all in one chunk!
			expanded = BLOCK*NUMTILE8;
		else if (chunk<STARTTILE16)
			expanded = MASKBLOCK*NUMTILE8M;
		else if (chunk<STARTTILE16M)	// all other tiles are one/chunk
			expanded = BLOCK*4;
		else if (chunk<STARTTILE32)
			expanded = MASKBLOCK*4;
		else if (chunk<STARTTILE32M)
			expanded = BLOCK*16;
		else
			expanded = MASKBLOCK*16;
	}
	else
	{
	//
	// everything else has an explicit size longword
	//
#ifdef WOLF3D_NATIVE
        expanded = (long)((uint32_t)source[0] |
            ((uint32_t)source[1] << 8) |
            ((uint32_t)source[2] << 16) |
            ((uint32_t)source[3] << 24));
#else
        expanded = *(long far *)source;
#endif
        source += 4;
	}

//
// allocate final space, decompress it, and free bigbuffer
// Sprites need to have shifts made and various other junk
//
	MM_GetPtr (&grsegs[chunk],expanded);
	if (mmerror)
		return;
	CAL_HuffExpand (source,grsegs[chunk],expanded,grhuffman,false);
}


/*
======================
=
= CA_CacheGrChunk
=
= Makes sure a given chunk is in memory, loadiing it if needed
=
======================
*/

void CA_CacheGrChunk (int chunk)
{
	long	pos,compressed;
	memptr	bigbufferseg;
	byte	far *source;
	int		next;

	grneeded[chunk] |= ca_levelbit;		// make sure it doesn't get removed
	if (grsegs[chunk])
	{
		MM_SetPurge (&grsegs[chunk],0);
		return;							// allready in memory
	}

//
// load the chunk into a buffer, either the miscbuffer if it fits, or allocate
// a larger buffer
//
	pos = GRFILEPOS(chunk);
	if (pos<0)							// $FFFFFFFF start is a sparse tile
	  return;

	next = chunk +1;
	while (GRFILEPOS(next) == -1)		// skip past any sparse tiles
		next++;

	compressed = GRFILEPOS(next)-pos;

	lseek(grhandle,pos,SEEK_SET);

	if (compressed<=BUFFERSIZE)
	{
		CA_FarRead(grhandle,bufferseg,compressed);
		source = bufferseg;
	}
	else
	{
		MM_GetPtr(&bigbufferseg,compressed);
		MM_SetLock (&bigbufferseg,true);
		CA_FarRead(grhandle,bigbufferseg,compressed);
		source = bigbufferseg;
	}

	CAL_ExpandGrChunk (chunk,source);

	if (compressed>BUFFERSIZE)
		MM_FreePtr(&bigbufferseg);
}



//==========================================================================

/*
======================
=
= CA_CacheScreen
=
= Decompresses a chunk from disk straight onto the screen
=
======================
*/

#ifdef WOLF3D_NATIVE
void CA_CacheScreen (int chunk)
{
    long pos, compressed, expanded;
    memptr compressed_buffer = NULL;
    memptr decoded_buffer = NULL;
    byte *source;
    int next;

    pos = GRFILEPOS(chunk);
    next = chunk + 1;
    while (GRFILEPOS(next) == -1)
        ++next;
    compressed = GRFILEPOS(next) - pos;

    if (pos < 0 || compressed <= 4 || lseek(grhandle, pos, SEEK_SET) != pos)
        Quit("CA_CacheScreen: invalid graphics chunk range");

    MM_GetPtr(&compressed_buffer, (unsigned long)compressed);
    if (mmerror) return;
    MM_SetLock(&compressed_buffer, true);
    if (!CA_FarRead(grhandle, (byte *)compressed_buffer, compressed)) {
        MM_FreePtr(&compressed_buffer);
        Quit("CA_CacheScreen: graphics read failed");
        return;
    }

    source = (byte *)compressed_buffer;
    expanded = (long)((uint32_t)source[0] |
        ((uint32_t)source[1] << 8) |
        ((uint32_t)source[2] << 16) |
        ((uint32_t)source[3] << 24));
    source += 4;

    MM_GetPtr(&decoded_buffer, (unsigned long)expanded);
    if (mmerror) {
        MM_FreePtr(&compressed_buffer);
        return;
    }
    CAL_HuffExpand(source, (byte *)decoded_buffer, expanded, grhuffman, false);
    if (expanded < (long)(320u * 200u)) {
        MM_FreePtr(&decoded_buffer);
        MM_FreePtr(&compressed_buffer);
        Quit("CA_CacheScreen: decoded screen is not 320x200 planar data");
        return;
    }

    VL_MemToScreen((byte *)decoded_buffer, 320, 200, 0, 0);
    VW_MarkUpdateBlock(0, 0, 319, 199);
    MM_FreePtr(&decoded_buffer);
    MM_FreePtr(&compressed_buffer);
}
#else
void CA_CacheScreen (int chunk)
{
	long	pos,compressed,expanded;
	memptr	bigbufferseg;
	byte	far *source;
	int		next;

//
// load the chunk into a buffer
//
	pos = GRFILEPOS(chunk);
	next = chunk +1;
	while (GRFILEPOS(next) == -1)		// skip past any sparse tiles
		next++;
	compressed = GRFILEPOS(next)-pos;

	lseek(grhandle,pos,SEEK_SET);

	MM_GetPtr(&bigbufferseg,compressed);
	MM_SetLock (&bigbufferseg,true);
	CA_FarRead(grhandle,bigbufferseg,compressed);
	source = bigbufferseg;

	expanded = *(long far *)source;
	source += 4;			// skip over length

//
// allocate final space, decompress it, and free bigbuffer
// Sprites need to have shifts made and various other junk
//
	CAL_HuffExpand (source,MK_FP(SCREENSEG,bufferofs),expanded,grhuffman,true);
	VW_MarkUpdateBlock (0,0,319,199);
	MM_FreePtr(&bigbufferseg);
}
#endif

//==========================================================================

/*
======================
=
= CA_CacheMap
=
= WOLF: This is specialized for a 64*64 map size
=
======================
*/

#ifdef WOLF3D_NATIVE
static boolean native_decode_map_plane(int mapnum, int plane)
{
    int32_t file_offset;
    uint16_t compressed_length;
    long file_size;
    uint64_t file_end;
    uint8_t *compressed_data;
    boolean decoded;

    if (mapnum < 0 || mapnum >= NUMMAPS ||
        plane < 0 || plane >= MAPPLANES ||
        mapheaderseg[mapnum] == NULL || mapsegs[plane] == NULL ||
        maphandle < 0 || tinf == NULL) {
        return false;
    }

    file_offset = mapheaderseg[mapnum]->planestart[plane];
    compressed_length = mapheaderseg[mapnum]->planelength[plane];
    file_size = filelength(maphandle);
    if (file_offset < 0 || compressed_length < sizeof(word) ||
        file_size < 0) {
        return false;
    }

    file_end = (uint64_t)(uint32_t)file_offset + compressed_length;
    if (file_end > (uint64_t)file_size ||
        lseek(maphandle, (long)file_offset, SEEK_SET) != (long)file_offset) {
        return false;
    }

    compressed_data = (uint8_t *)malloc(compressed_length);
    if (compressed_data == NULL) {
        return false;
    }
    if (!CA_FarRead(maphandle, compressed_data, (long)compressed_length)) {
        free(compressed_data);
        return false;
    }

#ifdef CARMACIZED
    decoded = wolf_native_map_decode_plane(
        compressed_data, compressed_length, (unsigned *)mapsegs[plane],
        64u * 64u, ((mapfiletype _seg *)tinf)->RLEWtag, true);
#else
    decoded = wolf_native_map_decode_plane(
        compressed_data, compressed_length, (unsigned *)mapsegs[plane],
        64u * 64u, ((mapfiletype _seg *)tinf)->RLEWtag, false);
#endif

    free(compressed_data);
    return decoded;
}
#endif

void CA_CacheMap (int mapnum)
{
#ifdef WOLF3D_NATIVE
    int plane;

    if (mapnum < 0 || mapnum >= NUMMAPS ||
        mapheaderseg[mapnum] == NULL || tinf == NULL) {
        Quit("CA_CacheMap: invalid map index or missing map header");
        return;
    }

    mapon = mapnum;
    for (plane = 0; plane < MAPPLANES; ++plane) {
        if (!native_decode_map_plane(mapnum, plane)) {
            Quit("CA_CacheMap: invalid or truncated map plane");
            return;
        }
    }
#else

	long	pos,compressed;
	int		plane;
	memptr	*dest,bigbufferseg;
	unsigned	size;
#ifdef WOLF3D_NATIVE
    word *source;
#else
    unsigned far *source;
#endif
#ifdef CARMACIZED
	memptr	buffer2seg;
	long	expanded;
#endif

	mapon = mapnum;

//
// load the planes into the allready allocated buffers
//
#ifdef WOLF3D_NATIVE
    size = 64 * 64 * (unsigned)sizeof(unsigned);
#else
	size = 64*64*2;
#endif

	for (plane = 0; plane<MAPPLANES; plane++)
	{
		pos = mapheaderseg[mapnum]->planestart[plane];
		compressed = mapheaderseg[mapnum]->planelength[plane];

		dest = (memptr *)&mapsegs[plane];

		lseek(maphandle,pos,SEEK_SET);
		if (compressed<=BUFFERSIZE)
			source = bufferseg;
		else
		{
			MM_GetPtr(&bigbufferseg,compressed);
			MM_SetLock (&bigbufferseg,true);
			source = bigbufferseg;
		}

		CA_FarRead(maphandle,(byte far *)source,compressed);
#ifdef CARMACIZED
		//
		// Carmack-expand the map plane, then RLEW-expand it into map memory.
		// The compressed plane starts with a word containing the Carmack
		// output byte count; decoded data starts with an RLEW size word.
		//
		expanded = *source;
		source++;
		MM_GetPtr (&buffer2seg,expanded);
#ifdef WOLF3D_NATIVE
        if (compressed < (long)sizeof(word) ||
            expanded < (long)sizeof(word) ||
            (expanded & 1) != 0 ||
            !wolf_native_carmack_expand(
                (const byte *)source,
                (size_t)(compressed - (long)sizeof(word)),
                (uint16_t *)buffer2seg,
                (size_t)expanded)) {
            MM_FreePtr(&buffer2seg);
            if (compressed > BUFFERSIZE) MM_FreePtr(&bigbufferseg);
            Quit("CA_CacheMap: invalid Carmack-compressed plane");
            return;
        }
        if (!wolf_native_rlew_expand(
                ((const uint16_t *)buffer2seg) + 1,
                (size_t)expanded / sizeof(uint16_t) - 1u,
                (unsigned *)*dest,
                (size_t)size / sizeof(unsigned),
                ((mapfiletype _seg *)tinf)->RLEWtag)) {
            MM_FreePtr(&buffer2seg);
            if (compressed > BUFFERSIZE) MM_FreePtr(&bigbufferseg);
            Quit("CA_CacheMap: invalid RLEW-compressed plane");
            return;
        }
#else
		CAL_CarmackExpand (source, (unsigned far *)buffer2seg,expanded);
		CA_RLEWexpand (((unsigned far *)buffer2seg)+1,(unsigned *)*dest,size,
		((mapfiletype _seg *)tinf)->RLEWtag);
#endif
		MM_FreePtr (&buffer2seg);

#else
		//
		// unRLEW, skipping expanded length
		//
#ifdef WOLF3D_NATIVE
        CA_RLEWexpand(source + 1, (unsigned *)*dest, size,
            ((mapfiletype _seg *)tinf)->RLEWtag);
#else
		CA_RLEWexpand (source+1, (unsigned *)*dest,size,
		((mapfiletype _seg *)tinf)->RLEWtag);
#endif
#endif

		if (compressed>BUFFERSIZE)
			MM_FreePtr(&bigbufferseg);
	}

#endif
}

//===========================================================================

/*
======================
=
= CA_UpLevel
=
= Goes up a bit level in the needed lists and clears it out.
= Everything is made purgable
=
======================
*/

void CA_UpLevel (void)
{
	int	i;

	if (ca_levelnum==7)
		Quit ("CA_UpLevel: Up past level 7!");

	for (i=0;i<NUMCHUNKS;i++)
		if (grsegs[i])
			MM_SetPurge ((memptr *)&grsegs[i],3);
	ca_levelbit<<=1;
	ca_levelnum++;
}

//===========================================================================

/*
======================
=
= CA_DownLevel
=
= Goes down a bit level in the needed lists and recaches
= everything from the lower level
=
======================
*/

void CA_DownLevel (void)
{
	if (!ca_levelnum)
		Quit ("CA_DownLevel: Down past level 0!");
	ca_levelbit>>=1;
	ca_levelnum--;
	CA_CacheMarks();
}

//===========================================================================

/*
======================
=
= CA_ClearMarks
=
= Clears out all the marks at the current level
=
======================
*/

void CA_ClearMarks (void)
{
	int i;

	for (i=0;i<NUMCHUNKS;i++)
		grneeded[i]&=~ca_levelbit;
}


//===========================================================================

/*
======================
=
= CA_ClearAllMarks
=
= Clears out all the marks on all the levels
=
======================
*/

void CA_ClearAllMarks (void)
{
	_fmemset (grneeded,0,sizeof(grneeded));
	ca_levelbit = 1;
	ca_levelnum = 0;
}


//===========================================================================


/*
======================
=
= CA_FreeGraphics
=
======================
*/


void CA_SetGrPurge (void)
{
	int i;

//
// free graphics
//
	CA_ClearMarks ();

	for (i=0;i<NUMCHUNKS;i++)
		if (grsegs[i])
			MM_SetPurge ((memptr *)&grsegs[i],3);
}



/*
======================
=
= CA_SetAllPurge
=
= Make everything possible purgable
=
======================
*/

void CA_SetAllPurge (void)
{
	int i;


//
// free sounds
//
	for (i=0;i<NUMSNDCHUNKS;i++)
		if (audiosegs[i])
			MM_SetPurge ((memptr *)&audiosegs[i],3);

//
// free graphics
//
	CA_SetGrPurge ();
}


//===========================================================================

/*
======================
=
= CA_CacheMarks
=
======================
*/
#define MAXEMPTYREAD	1024

void CA_CacheMarks (void)
{
	int 	i,next,numcache;
	long	pos,endpos,nextpos,nextendpos,compressed;
	long	bufferstart,bufferend;	// file position of general buffer
	byte	far *source;
	memptr	bigbufferseg;

	numcache = 0;
//
// go through and make everything not needed purgable
//
	for (i=0;i<NUMCHUNKS;i++)
		if (grneeded[i]&ca_levelbit)
		{
			if (grsegs[i])					// its allready in memory, make
				MM_SetPurge(&grsegs[i],0);	// sure it stays there!
			else
				numcache++;
		}
		else
		{
			if (grsegs[i])					// not needed, so make it purgeable
				MM_SetPurge(&grsegs[i],3);
		}

	if (!numcache)			// nothing to cache!
		return;


//
// go through and load in anything still needed
//
	bufferstart = bufferend = 0;		// nothing good in buffer now

	for (i=0;i<NUMCHUNKS;i++)
		if ( (grneeded[i]&ca_levelbit) && !grsegs[i])
		{
			pos = GRFILEPOS(i);
			if (pos<0)
				continue;

			next = i +1;
			while (GRFILEPOS(next) == -1)		// skip past any sparse tiles
				next++;

			compressed = GRFILEPOS(next)-pos;
			endpos = pos+compressed;

			if (compressed<=BUFFERSIZE)
			{
				if (bufferstart<=pos
				&& bufferend>= endpos)
				{
				// data is allready in buffer
					source = (byte _seg *)bufferseg+(pos-bufferstart);
				}
				else
				{
				// load buffer with a new block from disk
				// try to get as many of the needed blocks in as possible
					while ( next < NUMCHUNKS )
					{
						while (next < NUMCHUNKS &&
						!(grneeded[next]&ca_levelbit && !grsegs[next]))
							next++;
						if (next == NUMCHUNKS)
							continue;

						nextpos = GRFILEPOS(next);
						while (GRFILEPOS(++next) == -1)	// skip past any sparse tiles
							;
						nextendpos = GRFILEPOS(next);
						if (nextpos - endpos <= MAXEMPTYREAD
						&& nextendpos-pos <= BUFFERSIZE)
							endpos = nextendpos;
						else
							next = NUMCHUNKS;			// read pos to posend
					}

					lseek(grhandle,pos,SEEK_SET);
					CA_FarRead(grhandle,bufferseg,endpos-pos);
					bufferstart = pos;
					bufferend = endpos;
					source = bufferseg;
				}
			}
			else
			{
			// big chunk, allocate temporary buffer
				MM_GetPtr(&bigbufferseg,compressed);
				if (mmerror)
					return;
				MM_SetLock (&bigbufferseg,true);
				lseek(grhandle,pos,SEEK_SET);
				CA_FarRead(grhandle,bigbufferseg,compressed);
				source = bigbufferseg;
			}

			CAL_ExpandGrChunk (i,source);
			if (mmerror)
				return;

			if (compressed>BUFFERSIZE)
				MM_FreePtr(&bigbufferseg);

		}
}

void CA_CannotOpen(char *string)
{
 char str[30];

 strcpy(str,"Can't open ");
 strcat(str,string);
 strcat(str,"!\n");
 Quit (str);
}