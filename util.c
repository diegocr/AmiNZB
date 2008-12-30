/* ***** BEGIN LICENSE BLOCK *****
 * Version: MIT/X11 License
 * 
 * Copyright (c) 2007 Diego Casorran
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 
 * Contributor(s):
 *   Diego Casorran <dcasorran@gmail.com> (Original Author)
 * 
 * ***** END LICENSE BLOCK ***** */


#include <proto/exec.h>
#include <proto/dos.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "util.h"
#include "debug.h"

struct SignalSemaphore gSemaphore;

#define BADMEMORYAREA	0xdfEBADFa

//------------------------------------------------------------------------------

STATIC APTR __private_memory_pool = NULL;

BOOL InitMemoryPool( VOID )
{
	InitSemaphore(&gSemaphore);
	
	__private_memory_pool = CreatePool(MEMF_PUBLIC|MEMF_CLEAR, 4096, 512);
	
	return((__private_memory_pool != NULL) ? TRUE : FALSE);
}

VOID ClearMemoryPool( VOID )
{
	if(__private_memory_pool != NULL)
		DeletePool( __private_memory_pool );
}

APTR Malloc( ULONG size )
{
	ULONG *mem;
	
	if(((long)size) <= 0)
		return NULL;
	
	ObtainSemaphore(&gSemaphore);
	
	size += sizeof(ULONG) + MEM_BLOCKMASK;
	size &= ~MEM_BLOCKMASK;
	
	if((mem = AllocPooled(__private_memory_pool, size)))
		*mem++=size;
	
	ReleaseSemaphore(&gSemaphore);
	
	return mem;
}

VOID Free(APTR mem)
{
	ULONG size,*omem=mem;
	
	if( ! mem ) return;
	
	ObtainSemaphore(&gSemaphore);
	
	size = *(--omem);
	
	if( size != BADMEMORYAREA )
	{
		*omem = BADMEMORYAREA;
		FreePooled(__private_memory_pool, omem, size );
	}
	else {
		DBG("********** Tryed to free a memory which is already freed\a!\n");
	}
	
	ReleaseSemaphore(&gSemaphore);
}

//------------------------------------------------------------------------------

void request(const char *fmt, ...)
{
	char buf[2048];
	va_list args;
	
	ENTER();
	ObtainSemaphore(&gSemaphore);
	
	va_start( args, fmt );
	vsnprintf( buf, sizeof(buf)-1, fmt, args );
	va_end(args);
	
	DBG_STRING(buf);
	__request( buf );
	
	ReleaseSemaphore(&gSemaphore);
	LEAVE();
}

//------------------------------------------------------------------------------

char *ioerrstr( char *prompt )
{
	static char buffer[512];
	
	Fault( IoErr(), prompt, buffer, sizeof(buffer) - 1 );
	
	return (char *) buffer;
}

//------------------------------------------------------------------------------

VOID OutOfMemory(STRPTR where)
{
	request("Out of memory! (%s)", where);
}

//------------------------------------------------------------------------------

VOID TimeToDS(ULONG time, struct DateStamp * ds)
{
	ds->ds_Days   = (time / 86400);
	ds->ds_Minute = ((time % 86400) / 60);
	ds->ds_Tick   = (((time % 86400) % 60) * TICKS_PER_SECOND);
}

//------------------------------------------------------------------------------

ULONG DsToTime(struct DateStamp * ds)
{
	return ((ds->ds_Days * 86400) + (ds->ds_Minute * 60) + (ds->ds_Tick / TICKS_PER_SECOND));
}

//------------------------------------------------------------------------------

ULONG Time(ULONG *t)
{
	struct DateStamp ds;
	ULONG rc;
	
	DateStamp(&ds);
	
	rc = ((ds.ds_Days * 86400) + (ds.ds_Minute * 60) + (ds.ds_Tick / TICKS_PER_SECOND));
	
	if(t) *t = rc;
	
	return rc;
}

//------------------------------------------------------------------------------

ULONG FileSize( STRPTR filename )
{
	BPTR lock;
	ULONG size = 0;
	char __fib[sizeof(struct FileInfoBlock) + 3];
	struct FileInfoBlock *fib = 
		(struct FileInfoBlock *)(((long)__fib + 3L) & ~3L);
	
	ENTER();
	
	if((lock = Lock( filename, SHARED_LOCK)))
	{
		if(Examine(lock,fib)!=DOSFALSE)
			size = fib->fib_Size;
		
		UnLock( lock );
	}
	
	RETURN(size);
	return size;
}

APTR FileToMem( STRPTR FileName, ULONG * SizePtr )
{
	BPTR fh;
	APTR buf;
	
	if((fh = Open( FileName, MODE_OLDFILE)))
	{
		ULONG size = 0;
		char __fib[sizeof(struct FileInfoBlock) + 3];
		struct FileInfoBlock *fib = 
			(struct FileInfoBlock *)(((long)__fib + 3L) & ~3L);
		
		if(ExamineFH( fh, fib))
			size = fib->fib_Size;
		
		if(size) {
			
			if( SizePtr )
				*SizePtr = size;
			
			if((buf = AllocVec( size+1, MEMF_PUBLIC )))
			{
				ULONG rEad = Read(fh, buf, size);
				
				Close(fh);
				fh = 0;
				
				((char *)buf) [size] = '\0';
				
				if(rEad == size)
					return buf;
				
				FreeVec(buf);
			}
		}
		
		if(fh) {
			
			Close(fh);
		}
	}
	
	return NULL;
}

//------------------------------------------------------------------------------

unsigned int FindPos( unsigned char * pajar, unsigned char * aguja )
{
	const unsigned char *src = (const unsigned char *) pajar;
	
	if(!(pajar && *pajar) || !(aguja && *aguja))
		return 0;
	
	while(*src)
	{
		const unsigned char * a = (const unsigned char *) aguja;
		
		while(*src && (*src++ == *a++))
		{
			if(!(*a))
				return ~(pajar-++src);
		}
	}
	
	return 0;
}

//------------------------------------------------------------------------------

#define toLower(src_ch) \
({ \
	unsigned char ch = src_ch; \
	if(((ch > 64) && (ch < 91)) || ((ch > 191) && (ch < 224))) ch += 32; \
	ch; \
})

unsigned int FindPosNoCase( unsigned char * pajar, unsigned char * aguja )
{
	register const unsigned char *src=(const unsigned char *) pajar;
	
	if(!(pajar && *pajar) || !(aguja && *aguja))
		return 0;
	
	while(*src)
	{
		register const unsigned char * a=(const unsigned char *) aguja;
		
		while(*src && (toLower(*src++) == toLower(*a++)))
		{
			if(!(*a))
				return ~(pajar-++src);
		}
	}
	
	return 0;
}

//------------------------------------------------------------------------------

STRPTR Strip( STRPTR string, UBYTE ch, UBYTE ch2 )
{
	// strip leading and trailing 'ch's from '*string'
	
	STRPTR ptr = string;
	
	ptr += strlen(ptr) - 1;
	
	while(*ptr == ch || *ptr == ch2)
		*ptr-- = 0;
	
	while(*string == ch || *string == ch2)
		string++;
	
	return string;
}

//------------------------------------------------------------------------------

STRPTR Trim( STRPTR string )
{
	// strip spaces and tabs
	
	return Strip( string, 0x20, 0x09 );
}

//------------------------------------------------------------------------------

// Hex String TO ULong
ULONG hstoul( STRPTR hex_string, int * len )
{
	UBYTE ch;
	ULONG num = 0;
	int l = 0;
	
	while((ch = *hex_string++))
	{
		l++;
		switch( ch )
		{
			case '0': case '1': case '2':
			case '3': case '4': case '5':
			case '6': case '7': case '8':
			case '9':
				num = num * 16 + (ch - '0');
				break;
				
			case 'a': case 'b': case 'c':
			case 'd': case 'e': case 'f': 
				num = num * 16 + (ch - 'a') + 10;
				break;
				
			case 'A': case 'B': case 'C':
			case 'D': case 'E': case 'F':
				num = num * 16 + (ch - 'A') + 10;
				break;
				
			default:
				num = (ULONG) -1 ;
				*hex_string = 0;
				break;
		}
	}
	
	if(len)
		*len = l;
	
	return num;
}

//------------------------------------------------------------------------------

//  Function that calculates a 32bit crc checksum for a provided buffer.
//  See http://www.4d.com/ACIDOC/CMU/CMU79909.HTM for more information about
//  the CRC32 algorithm.
//  This implementation allows the usage of more than one persistant calls of
//  the crc32 function. This allows to calculate a valid crc32 checksum over
//  an unlimited amount of buffers.
ULONG CRC32( APTR buffer, ULONG buffer_length, ULONG crc )
{
  /* table generated with the following code:
   *
   * #define CRC32_POLYNOMIAL 0xEDB88320L
   *
   * int i, j;
   *
   * for (i = 0; i <= 255; i++) {
   *   unsigned long crc = i;
   *   for (j = 8; j > 0; j--) {
   *     if (crc & 1)
   *       crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
   *     else
   *       crc >>= 1;
   *   }
   *   CRCTable[i] = crc;
   * }
   */
  static const unsigned long CRCTable[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
  };
  unsigned char *p = (unsigned char *)buffer;

  // we calculate the crc32 now.
  while(buffer_length-- != 0)
  {
    ULONG temp1 = (crc >> 8) & 0x00FFFFFFL;
    ULONG temp2 = CRCTable[((int)crc ^ *p++) & 0xFF];
    crc = temp1 ^ temp2;
  }

  return crc;
}

//------------------------------------------------------------------------------


