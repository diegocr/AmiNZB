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
#include "decoders.h"
#include "util.h"
#include "debug.h"
#include "analyzer.h" /* AddNodeListItem() */
#include <proto/alib.h> /* NewList() */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//------------------------------------------------------------------------------
//--- yEnc stuff --------------------------------------------------------------+

// keyword of yenc in begin line
#define YENC_KEY_BEGIN       "=ybegin " // the blank is NO bug!
#define YENC_BEGIN_PARM_NAME "name="
#define YENC_BEGIN_PARM_SIZE "size="
#define YENC_BEGIN_PARM_LINE "line="
#define YENC_BEGIN_PARM_PART "part="

// keyword of yenc in part line
#define YENC_KEY_PART        "=ypart " // the blank is NO bug!
#define YENC_PART_PARM_BEGIN "begin="
#define YENC_PART_PARM_END   "end="

// keyword of yenc in end line
#define YENC_KEY_END         "=yend " // the blank is NO bug!
#define YENC_END_PARM_SIZE   "size="
#define YENC_END_PARM_CRC    "crc32="
#define YENC_END_PARM_PCRC   "pcrc32="

#define UNINITIALIZED -1

// annoying that C does not define it :-)
typedef enum bool      { false=FALSE, true=TRUE } bool;

// special bool type, capable of signalling that it is not set
typedef enum triBool   { uninitialized=UNINITIALIZED, setFalse=false, setTrue=true } triBool;

// struct describing a processed part
struct yencPartEntry {
  char                   fileName[1024]; // path to encoded part file
  unsigned long          startLine;      // starting line of part in file

  bool                   isCorrupted;    // indicates wheter the part is corrupted

  unsigned int           partNum;        // yenc part number

  unsigned long          posBegin;       // yenc begin position in dest file
  unsigned long          posEnd;         // yenc end position in dest file

  unsigned int           yLine;          // yenc line length

  ULONG                  crcExpected;    // yenc crc sum
  ULONG                  crcCalculated;  // actual crc sum

  unsigned long          sizeExpected;   // yenc size of part in UNencoded part
  unsigned long          sizeWritten;    // actual size

  struct yencPartEntry   *nextPart;      // pointer to next part, NULL if none
};

// struct describing a processed outfile
struct yencFileEntry {
  UBYTE                  destFile[4096]; // path to destination file
  BPTR                   fileStream;     // output stream associated to output file

  triBool                isMultipart;    // flag indicating wheter is multi part encoded

  bool                   isCorrupted;    // indicates wheter the file is corrupted
  bool                   wasIgnored;     // indicates wheter the file (and all related parts) was ignored

  unsigned long          size;           // yenc size of UNencoded file

  struct yencPartEntry   *partList;      // list of parts found for this file

  struct yencFileEntry   *nextFile;      // next dest file, NULL if none
};

// struct for part range checking
struct rangeEntry {
  unsigned long      start;        // start position of range
  unsigned long      end;          // end position of range

  bool               isCorrupted;  // flag wheter range was corrupted

  struct rangeEntry  *nextEntry;   // next range, NULL if none
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

DEC_T GuessDecoder( STRPTR data, ULONG data_len )
{
	int y_len = strlen(YENC_KEY_BEGIN);
	
	while(data_len-- > 0)
	{
		if(!strncmp( data, YENC_KEY_BEGIN, y_len))
			return DEC_YENC;
		
		data++;
	}
	
	return DEC_UNKNOWN;
}

DEC_T GuessDecoderFromFile( STRPTR filename )
{
	DEC_T rc = DEC_UNKNOWN;
	
	STRPTR file;
	ULONG size;
	
	ENTER();
	
	// first, try to load the whole file in mem
	file = FileToMem( filename, &size );
	
	if( file != NULL )
	{
		DBG("file loaded into mem, guessing decoder...\n");
		
		rc = GuessDecoder( file, size );
		
		FreeVec( file );
	}
	else
	{
		// well, cannot load the file into mem, let see if opening it...
		BPTR fd;
		
		if((fd = Open( filename, MODE_OLDFILE)))
		{
			UBYTE buf[4096];
			LONG readed;
			
			readed = Read( fd, buf, sizeof(buf)-1);
			DBG_VALUE(readed);
			
			if( readed > 0 )
				rc = GuessDecoder( buf, readed );
			
			Close(fd);
		}
	}
	
	RETURN(rc);
	return(rc);
}

//------------------------------------------------------------------------------
//--- yEnc decoder ------------------------------------------------------------+

#define hexToUlong(str)	hstoul( str, NULL)

STATIC STRPTR FGetsAtom(BPTR fd, STRPTR buf, ULONG buf_len)
{
	if(FGets( fd, buf, buf_len) != NULL)
	{
		char *ptr;
		
		if((ptr=strrchr((char*)buf, '\r'))) *ptr=0;
		else if((ptr=strrchr((char*)buf, '\n'))) *ptr=0;
		
		return buf;
	}
	
	return NULL;
}

#ifdef DEBUG
#define ERRMSG_LENGTH 4096
char *getPosDescr( const unsigned long inLineNum, 
                   const struct yencPartEntry *inPartEntry, 
                   const struct yencFileEntry *inFileEntry)
{ // this function isn't thread-safe at all
  static char theMsg[ERRMSG_LENGTH];  // a really dirty way for permanently allocating mem

  if ( inPartEntry==NULL ) {
    DBG( "Unexpected internal error in getPosDescr(): NULL parameter given\n");
    return NULL;
  }

  if ( inFileEntry == NULL ) {
    snprintf( theMsg, ERRMSG_LENGTH, "infile: '%s', line: %ld",
              inPartEntry->fileName, inLineNum);
  }
  else if ( inFileEntry->isMultipart == false ) {  // check for false explicitely, it is triBool!
    snprintf( theMsg, ERRMSG_LENGTH, "infile: '%s', line: %ld, outfile: '%s'",
              inPartEntry->fileName, inLineNum, inFileEntry->destFile);
  }
  else {
    snprintf( theMsg, ERRMSG_LENGTH, "infile: '%s', line: %ld, outfile: '%s', part: %d", 
              inPartEntry->fileName, inLineNum, inFileEntry->destFile, inPartEntry->partNum);
  }

  return theMsg;
}
#endif

STATIC struct yencPartEntry * newPartEntry( STRPTR file, ULONG LineNum)
{
	struct yencPartEntry *yPart;
	
	if(!(yPart = Malloc(sizeof(struct yencPartEntry))))
		return NULL;
	
	yPart->crcCalculated	= -1L;
	yPart->startLine	= LineNum;
	
	CopyMem( file, yPart->fileName, sizeof(yPart->fileName)-1);
	
	return yPart;
}

STATIC struct yencFileEntry *
getDestFileEntry( STRPTR inDestFile, STRPTR g_outputDir, struct yencFileEntry **g_yencFileList)
{
	struct yencFileEntry **yFilePtr;
	UBYTE destPath[1024];
	BPTR lock;
	
	*destPath = 0;
	
	if( g_outputDir != NULL )
	{
		if(!AddPart( destPath, g_outputDir, sizeof(destPath)-1))
			return NULL;
	}
	
	if(!AddPart( destPath, inDestFile, sizeof(destPath)-1))
		return NULL;
	
	yFilePtr = g_yencFileList;
	while( *yFilePtr != NULL )
	{
		if(!strcmp( (*yFilePtr)->destFile, destPath))
			return( *yFilePtr);  // found in list
		
		yFilePtr = &((*yFilePtr)->nextFile);
	}
	
	// dest file does not exist yet in list
	if(!((*yFilePtr) = Malloc(sizeof(struct yencFileEntry))))
		return NULL;
	
	CopyMem( destPath, (*yFilePtr)->destFile, sizeof((*yFilePtr)->destFile)-1);
	
	(*yFilePtr)->isMultipart = uninitialized;
	(*yFilePtr)->size        = (ULONG)UNINITIALIZED;
	
	// check if dest file exists
	(*yFilePtr)->wasIgnored = true;
	if((lock = Lock( destPath, SHARED_LOCK)))
	{
		UnLock(lock);
		
		(*yFilePtr)->fileStream     = NULL;
		(*yFilePtr)->isCorrupted    = true;
		
		DBG("Output file '%s' already exists, will skip all related parts\n", destPath);
	}
	else // if not exists => open for writing
	{
		if(!((*yFilePtr)->fileStream = Open( destPath, MODE_NEWFILE)))
		{
			(*yFilePtr)->isCorrupted    = true;
			
			DBG("Unable to open destination file '%s' for writing, will skip all related parts\n", destPath);
		}
		(*yFilePtr)->wasIgnored = false;
	}
	
	return( *yFilePtr);
}

STATIC void addPartEntryToFileEntry( struct yencFileEntry *inFileEntry, struct yencPartEntry *inPartEntry)
{
	struct yencPartEntry **partEntryPtr;
	
	// append at the end of the list
	// this is REALLY vital for later correct corruption  checking
	partEntryPtr = &(inFileEntry->partList);
	while ( *partEntryPtr != NULL )
	{
		partEntryPtr = &((*partEntryPtr)->nextPart);
	}
	*partEntryPtr = inPartEntry;
}

STATIC void setFileCorrupted( struct yencFileEntry *inFileEntry)
{
	//char *newName;
	
	inFileEntry->isCorrupted    = true;
	
	if( inFileEntry->fileStream )
	{
		Close( inFileEntry->fileStream );
		inFileEntry->fileStream = 0;
	}
	
/**	if( g_keepCorrupted )
	{
		newName = calloc_check( 1, strlen( inFileEntry->destFile)+strlen( BOGUS_FILE_SUFFIX)+1);
		strcpy( newName, inFileEntry->destFile);
		strcat( newName, BOGUS_FILE_SUFFIX);
		rename( inFileEntry->destFile, newName);
		printfError( "Output file '%s' is corrupted, saved as '%s'\n", inFileEntry->destFile, newName);
		free( newName);
	}
	else
****/	{
		DeleteFile( inFileEntry->destFile );
		DBG( "Output file '%s' is corrupted, will be deleted\n", inFileEntry->destFile);
	}
}

STATIC struct yencFileEntry *
parseYencBegin( STRPTR inLineBuffer, ULONG inLineNum, STRPTR g_outputDir, 
	struct yencPartEntry *inPartEntry, struct yencFileEntry **g_yencFileList)
{
	int pos;
	ULONG size;
	struct yencFileEntry *fileEntry;
	LONG stl;
	
	// parse "name=" parameter
	if((pos=FindPos( inLineBuffer, YENC_BEGIN_PARM_NAME)) == 0)
	{
		DBG("'%s' wasn't found\n", YENC_BEGIN_PARM_NAME);
		return NULL;
	}
	
	// look for file entry in our list
	fileEntry = getDestFileEntry(Trim(&inLineBuffer[pos]),g_outputDir,g_yencFileList);
	*((char *)&inLineBuffer[pos]) = 0; // throw away the filename
	if( !fileEntry || fileEntry->isCorrupted )
	{
		if( fileEntry ) DBG_VALUE(fileEntry->isCorrupted);
		return NULL;
	}
	
	addPartEntryToFileEntry( fileEntry, inPartEntry);
	
	// check if this is a multipart message
	// parse this one first for better error messages
	if((pos = FindPos( inLineBuffer, YENC_BEGIN_PARM_PART)) != 0)
	{
		// this is a part of a multipart message
		
		if( fileEntry->isMultipart == false )
		{
			DBG("Inconsistecy in beeing single/multi part file (%s)\n", getPosDescr( inLineNum, inPartEntry, fileEntry));
			
			// this one is really fatal, so give up this ouput file
			setFileCorrupted( fileEntry);
			inPartEntry->isCorrupted = true;
			
			Free(fileEntry);
			return NULL;
		}
		fileEntry->isMultipart = true;
		
		stl = StrToLong( &inLineBuffer[pos], (LONG *)&inPartEntry->partNum);
		DBG_ASSERT(stl != -1);
	}
	else
	{
		if( fileEntry->isMultipart == true )
		{
			DBG("Inconsistecy in beeing single/multi part file (%s)\n", getPosDescr( inLineNum, inPartEntry, fileEntry));
			
			// this one is really fatal, so give up this ouput file
			setFileCorrupted( fileEntry);
			inPartEntry->isCorrupted = true;
			
			Free(fileEntry);
			return NULL;
		}
		
		fileEntry->isMultipart = false;
	}
	
	// parse "size=" parameter
	if(!(pos = FindPos(inLineBuffer, YENC_BEGIN_PARM_SIZE)))
	{
		DBG("'%s' not found in begin line (%s)\n", YENC_BEGIN_PARM_SIZE, getPosDescr( inLineNum, inPartEntry, fileEntry));
		
		// we have not written anything yet, so only set 
		// the part to corrupted and not the whole file
		inPartEntry->isCorrupted = true;
		
		Free(fileEntry);
		return NULL;
	}
	
	stl = StrToLong( &inLineBuffer[pos], &size );
	DBG_ASSERT(stl != -1);
	
	if( fileEntry->size == (ULONG)UNINITIALIZED )
	{
		fileEntry->size = size;
	}
	else if( size != fileEntry->size )
	{
		DBG("Inconsistent size to previous part (%s)\n", getPosDescr( inLineNum, inPartEntry, fileEntry));
		
		// we have not written anything yet, so only set 
		// the part to corrupted and not the whole file
		inPartEntry->isCorrupted = true;
		
		Free(fileEntry);
		return NULL;
	}
	
	// parse "line=" parameter
	if(!(pos = FindPos(inLineBuffer, YENC_BEGIN_PARM_LINE)))
	{
		DBG( "'%s' not found in begin line (%s)\n", YENC_BEGIN_PARM_LINE, getPosDescr( inLineNum, inPartEntry, fileEntry));
		
		// we have not written anything yet, so only set 
		// the part to corrupted and not the whole file
		inPartEntry->isCorrupted = true;
		
		Free(fileEntry);
		return NULL;
	}
	
	stl = StrToLong( &inLineBuffer[pos], (LONG *)&inPartEntry->yLine);
	DBG_ASSERT(stl != -1);
	
	return fileEntry;
}

STATIC bool parseYencPart( STRPTR inLineBuffer, ULONG inLineNum,
                    struct yencPartEntry *inPartEntry,
                    struct yencFileEntry *inFileEntry)
{
	int pos;
	
	if(strncmp((char*)inLineBuffer, YENC_KEY_PART, strlen( YENC_KEY_PART)))
	{
		DBG("'%s' not found, but part line was expected (%s)\n", YENC_KEY_PART, getPosDescr( inLineNum, inPartEntry, inFileEntry));
		
		// we have not written anything yet, so only set 
		// the part to corrupted and not the whole file
		inPartEntry->isCorrupted = true;
		
		return false;
	}
	
	if(!(pos = FindPos( inLineBuffer, YENC_PART_PARM_END)))
	{
		DBG("'%s' not found in part line (%s)\n", YENC_PART_PARM_END, getPosDescr( inLineNum, inPartEntry, inFileEntry));
		
		// we have not written anything yet, so only set 
		// the part to corrupted and not the whole file
		inPartEntry->isCorrupted = true;
		
		return false;
	}
	
	if(StrToLong( &inLineBuffer[pos], &inPartEntry->posEnd) != -1)
		inPartEntry->posEnd--; // storing postion 1 based is sooo ....
	
	if(!(pos = FindPos( inLineBuffer, YENC_PART_PARM_BEGIN)))
	{
		DBG("'%s' not found in part line (%s)\n", YENC_PART_PARM_BEGIN, getPosDescr( inLineNum, inPartEntry, inFileEntry));
		
		// we have not written anything yet, so only set 
		// the part to corrupted and not the whole file
		inPartEntry->isCorrupted = true;
		
		return false;
	}
	
	if(StrToLong( &inLineBuffer[pos], &inPartEntry->posBegin) != -1)
		inPartEntry->posBegin--; // storing postion 1 based is sooo ....
	
	return true;
}

STATIC bool parseYencEnd( STRPTR inLineBuffer, ULONG inLineNum,
                   struct yencPartEntry *inPartEntry,
                   struct yencFileEntry *inFileEntry)
{
	char *crcParamName;
	ULONG crc32;
	int pos, stl;
	
	// parse size parameter
	if(!(pos = FindPos( inLineBuffer, YENC_END_PARM_SIZE)))
	{
		DBG("'%s' not found in end line (%s)\n", YENC_END_PARM_SIZE, getPosDescr( inLineNum, inPartEntry, inFileEntry));
		
		// we have not written anything yet, so only set 
		// the part to corrupted and not the whole file
		inPartEntry->isCorrupted = true;
		
		return false;
	}
	
	stl = StrToLong( &inLineBuffer[pos], &inPartEntry->sizeExpected);
	DBG_ASSERT(stl != -1);
	
	// size of part must always match
	if( inPartEntry->sizeExpected != inPartEntry->sizeWritten )
	{
		DBG("Part size mismatch (%s)\n", getPosDescr( inLineNum, inPartEntry, inFileEntry));
		
		// we have not written anything yet, so only set 
		// the part to corrupted and not the whole file
		inPartEntry->isCorrupted = true;
		
		return false;
	}
	
	// with single part while file size must match
	if( ! inFileEntry->isMultipart && inPartEntry->sizeExpected != inFileEntry->size )
	{
		DBG("Output file size mismatch (%s)\n", getPosDescr( inLineNum, inPartEntry, inFileEntry));
		
		// we have not written anything yet, so only set 
		// the part to corrupted and not the whole file
		inPartEntry->isCorrupted = true;
		
		return false;
	}
	
	// do crc checks
	if( inFileEntry->isMultipart )
		crcParamName = YENC_END_PARM_PCRC;
	else
		crcParamName = YENC_END_PARM_CRC;
	if(!(pos = FindPos( inLineBuffer, crcParamName)))
	{
		DBG("'%s' not found in end line (%s)\n", crcParamName, getPosDescr( inLineNum, inPartEntry, inFileEntry));
		
		// we have not written anything yet, so only set 
		// the part to corrupted and not the whole file
		inPartEntry->isCorrupted = true;
		
		return false;
	}
	
	crc32=hexToUlong( &inLineBuffer[pos]);
	if( crc32 != (inPartEntry->crcCalculated ^ 0xFFFFFFFFl))
	{
		SetIoErr(ERROR_BAD_NUMBER);
		
		DBG("CRC mismatch (%s)\n",getPosDescr( inLineNum, inPartEntry, inFileEntry));
		
		// we have not written anything yet, so only set 
		// the part to corrupted and not the whole file
		inPartEntry->isCorrupted = true;
		
		return false;
	}
	
	return true;
}


LONG yEnc_Decoder( STRPTR file, STRPTR OutputDir, struct MinList * decoded_files )
{
	BPTR fd;
	LONG rc = 0, __saved_ioerr=0;
	BOOL error = TRUE;
	
	ENTER();
	DBG_STRING(file);
	
	if((fd = Open( file, MODE_OLDFILE)))
	{
		UBYTE line[2048];
		ULONG line_cnt = 0;
		BOOL fatal = FALSE;
		struct yencPartEntry   *partEntry = NULL;
		struct yencFileEntry   *fileEntry = NULL;
		struct yencFileEntry *g_yencFileList = NULL;
		struct yencFileEntry * YfePtr = NULL, *YfePtrN;
		int y_len = strlen(YENC_KEY_BEGIN);
		int yend_len = strlen( YENC_KEY_END);
		
		while(FGetsAtom( fd, line, sizeof(line)-1) != NULL)
		{
			line_cnt++;
			
			if(strncmp( line, YENC_KEY_BEGIN, y_len))
				continue;
			error = TRUE;
			
			// start of a section found => new part begins
			if(!(partEntry = newPartEntry( file, line_cnt )))
				break;
			
			fileEntry = parseYencBegin(line,line_cnt,OutputDir,partEntry,&g_yencFileList);
			if( fileEntry == NULL )
				continue;
			
			// when its multipart, lets parse the corresponding ypart line
			if( fileEntry->isMultipart )
			{
				if(FGetsAtom( fd, line, sizeof(line)-1)==NULL)
				{
					DBG("Unexpected end of file (%s)\n", getPosDescr( line_cnt, partEntry, fileEntry));
					
					// we have not written anything yet, so only set 
					// the part to corrupted and not the whole file
					partEntry->isCorrupted = true;
					
					break;
				}
				line_cnt++;
				
				if(parseYencPart( line, line_cnt, partEntry, fileEntry) == false)
					continue;
			}
			else
			{
				// the parts end must be set manually with single part!
				partEntry->posEnd = fileEntry->size;
			}
			
			if( partEntry->isCorrupted || fileEntry->isCorrupted ) continue;
			
			/// Original code used a temp file to write, we'll write directly...
			
			do {
				register unsigned char *lPtr, *lDst, ch;
				LONG lPtrLen;
				
				if(FGetsAtom( fd, line, sizeof(line)-1)==NULL)
				{
					DBG("Unexpected end of file (%s)\n", getPosDescr( line_cnt, partEntry, fileEntry));
					
					partEntry->isCorrupted = true;
					fatal = TRUE;
					break;
				}
				
				if(!strncmp( line, YENC_KEY_END, yend_len)) break;
				
				lPtrLen=0;
				lDst = lPtr = line;
				while(( ch = *lPtr++ ))
				{
					if(ch == '=') ch = *lPtr++ - 64;  // The escape character comes in
					ch -= 42; // Subtract the secret number
					*lDst++ = ch;
					lPtrLen++;
				}
				
				partEntry->crcCalculated = CRC32( line, lPtrLen, partEntry->crcCalculated);
				
				if(FWrite( fileEntry->fileStream, line, 1, lPtrLen) != lPtrLen)
				{
					DBG("Error writing to file (%s)\n", getPosDescr( line_cnt, partEntry, fileEntry));
					fatal = TRUE;
					break;
				}
				
				partEntry->sizeWritten += lPtrLen;
				
			} while(1);
			
			if( fatal )
				break;
			
			if(parseYencEnd( line, line_cnt, partEntry, fileEntry) == false)
			{
				// we have not written anything yet, so only set 
				// the part to corrupted and not the whole file
				partEntry->isCorrupted = true;
				continue; // search for new beginning
			}
			
			error = FALSE;
		}
		
		__saved_ioerr = IoErr();
		DBG_VALUE(__saved_ioerr);
		
		Close(fd);
		
		for( YfePtr = g_yencFileList ; YfePtr ; YfePtr = YfePtrN )
		{
			struct yencPartEntry * part, *partNext;
			
			YfePtrN = YfePtr->nextFile;
			
			DBG_VALUE(YfePtr->isCorrupted);
			DBG_VALUE(YfePtr->wasIgnored);
			
			if( YfePtr->fileStream )
			{
				// it should be closed (and deleted/renamed) previously if corrupted
				DBG_ASSERT(YfePtr->isCorrupted == false);
				
				Close( YfePtr->fileStream );
			}
			
			for( part = YfePtr->partList ; part ; part = partNext )
			{
				partNext = part->nextPart;
				
				if( part->isCorrupted )
				{
					__saved_ioerr = ERROR_BAD_NUMBER; // same error as for crc checksum error
					
					// the file should be marked as currupt as well
					if( YfePtr->isCorrupted == false )
					{
						DBG("WARNING: the file isn't marked as corrupt, WHILE a part is!\n");
						
						YfePtr->isCorrupted = true;
					}
				}
				
				Free( part );
			}
			
			if( YfePtr->isCorrupted == false )
			{
				ULONG size = FileSize( YfePtr->destFile );
				
				DBG_ASSERT( YfePtr->size == size );
				
				if( YfePtr->size != size )
				{
					YfePtr->isCorrupted = true;
				}
				else
				{
					/**
					 * Esto es algo ambiguo, en el caso de que se produzca
					 * un error por falta de memoria... 
					 */
					if(!AddNodeListItem( decoded_files, YfePtr->destFile))
					{
						__saved_ioerr = ERROR_NO_FREE_STORE;
						fatal = TRUE;
					}
				}
			}
			
			if( YfePtr->isCorrupted == true )
			{
				DeleteFile( YfePtr->destFile );
				
				DBG_ASSERT(fatal == TRUE);
				DBG_ASSERT(__saved_ioerr != 0);
				
				if(__saved_ioerr == 0)
					__saved_ioerr = ERROR_BAD_NUMBER;
			}
			
			Free( YfePtr );
		}
		
		if( fatal == TRUE )
		{
			FreeNodeListItem( decoded_files );
			NewList((struct List *) decoded_files );
			DBG_ASSERT(__saved_ioerr != 0);
			error = TRUE;
		}
		
		SetIoErr(__saved_ioerr);
	}
	
	if( rc == 0 )
	{
		rc = IoErr();
		
		if(error && rc == 0)
			rc = ERROR_OBJECT_WRONG_TYPE;
	}
	
	RETURN(rc);
	return(rc);
}

#ifdef TEST_YENC
# include "util.c" /* CRC32() and hstoul() and Malloc() */
# include "debug.c"
long __stack = 65535;

int main( int argc, char * argv[] )
{
	int rc = -1;
	
	if( argc != 2 ) {
		printf("give me just an y-encoded file!\n");
		exit(1);
	}
	
	if(InitMemoryPool())
	{
		rc = yEnc_Decoder( argv[1], NULL );
		
		printf("\n%ld = yEnc_Decoder( \"%s\" );\n\r\n", rc, argv[1] );
		
		ClearMemoryPool();
	}
	
	return rc;
}

#endif /* TEST_YENC */

//--- yEnc decoder ------------------------------------------------------------+
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
//--- Base64 decoder ----------------------------------------------------------+





//--- Base64 decoder ----------------------------------------------------------+
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------



//------------------------------------------------------------------------------
//--- UUencode decoder --------------------------------------------------------+




//--- UUencode decoder --------------------------------------------------------+
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


