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
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/alib.h>
#include <proto/utility.h>
#include <proto/xml2.h>
#include <xml2/parser.h>
#include <xml2/tree.h>
#include <SDI_hook.h>
#include <string.h>
#include "muiface.h"
#include "thread.h"
#include "debug.h"
#include "util.h"
#include "analyzer.h"
#include "downler.h"
#include <MUI/BetterBalance_mcc.h>
#include <MUI/NListview_mcc.h>

#define UNIX_TIME_OFFSET	252460800

//------------------------------------------------------------------------------

//STATIC struct Library * PRIVATE_MUIMasterBase = NULL;
//STATIC struct SignalSemaphore AnalyzerSemaphore;

//------------------------------------------------------------------------------

enum { FiLE, SIZE, DATE, POST, SEGM, GROU };

struct NList_Entry
{
	UBYTE file[SUBJECT_MAXLEN]; // should be enought (which size to use exactly?)
	ULONG size;
	ULONG date;
	UBYTE poster[POSTER_MAXLEN];
	ULONG segments;
	// 
	UBYTE size_string[32];
	UBYTE date_string[32];
	UBYTE segments_string[32];
	struct DateStamp date_stamp;
	// 
	struct MinList mlSegments;
	struct MinList mlGroups;
};

struct NList_Single
{
	ULONG MagicID;
	#define NLSMID	0x55478932
	
	struct Xml2Base * Xml2Base;
	xmlNodePtr file_node;
};

//------------------------------------------------------------------------------

BOOL AddNodeListItem( struct MinList * list, char * item )
{
	BOOL rc = FALSE;
	struct NodeListItem * new;
	
	ENTER();
	
	if((new = AllocMem( sizeof(struct NodeListItem), MEMF_PUBLIC|MEMF_CLEAR)))
	{
		new->mItemLength = (UWORD) strlen(item);
		
		if((new->mItem = AllocMem((ULONG) new->mItemLength+1, MEMF_PUBLIC)))
		{
			CopyMem( item, new->mItem, new->mItemLength );
			new->mItem[new->mItemLength] = '\0';
			
			AddTail((struct List *) list, (struct Node *) new );
			
			rc = TRUE;
		}
		else FreeMem( new, sizeof(struct NodeListItem));
	}
	
	RETURN(rc);
	return rc;
}

VOID FreeNodeListItem( struct MinList * list )
{
	struct NodeListItem * node;
	
	ENTER();
	
	while((node = (struct NodeListItem *)RemTail((struct List *) list )))
	{
		DBG_STRING(node->mItem);
		
		if( node->mItem )
			FreeMem( node->mItem, node->mItemLength+1 );
		
		FreeMem( node, sizeof(struct NodeListItem));
	}
	
	LEAVE();
}

BOOL DupNodeListItem( struct MinList * dst, struct MinList * src )
{
	struct NodeListItem * node;
	BOOL rc = TRUE;
	
	ENTER();
	
	ITERATE_LIST( src, struct NodeListItem *, node )
	{
		if(!AddNodeListItem((struct MinList *)dst, node->mItem))
		{
			rc = FALSE;
			break;
		}
	}
	
	if( rc == FALSE )
	{
		FreeNodeListItem( dst );
	}
	
	RETURN(rc);
	return rc;
}

//---------------------------------------------- NList Construct Hook ------

STATIC VOID ProcessNZBFileNode( xmlNodePtr file_node,
	struct NList_Entry *entry, struct Xml2Base * Xml2Base )
{
	xmlNodePtr child = file_node->xmlChildrenNode;
	xmlChar * xch;
	
	ENTER();
	NewList((struct List *) &entry->mlSegments );
	NewList((struct List *) &entry->mlGroups );
	
	if((xch = xmlGetProp( file_node, "poster")))
	{
		if(*((char *)xch))
			CopyMem( xch, entry->poster, sizeof(entry->poster)-1);
		
		DBG_STRING(entry->poster);
		xmlFree(xch);
	}
	if((xch = xmlGetProp( file_node, "subject")))
	{
		if(*((char *)xch))
			CopyMem( xch, entry->file, sizeof(entry->file)-1);
		
		DBG_STRING(entry->file);
		xmlFree(xch);
	}
	if((xch = xmlGetProp( file_node, "date")))
	{
		if(*((char *)xch))
		{
			if(StrToLong( xch, &entry->date ) != -1)
			{
				// adjust UNIX time to AmigaOS time
				
				if(entry->date > UNIX_TIME_OFFSET)
					entry->date -= UNIX_TIME_OFFSET;
			}
		}
		
		DBG_VALUE(entry->date);
		xmlFree(xch);
	}
	
	for( ; child ; child = child->next )
	{
		int type = 0;
		
		DBG_STRING(child->name);
		
		#define isSegment	1
		#define isGroup		2
		
		if(!xmlStrcasecmp( child->name, "segments"))
			type = isSegment;
		else if(!xmlStrcasecmp( child->name, "groups"))
			type = isGroup;
		
		if( type != 0 )
		{
			xmlNodePtr son = child->xmlChildrenNode;
			
			for( ; son ; son = son->next )
			{
				int which = 0;
				
				switch( type )
				{
					case isSegment:
						if(!xmlStrcasecmp( son->name, "segment"))
							which = isSegment;
						break;
					
					case isGroup:
						if(!xmlStrcasecmp( son->name, "group"))
							which = isGroup;
						break;
				}
				
				if( which != 0 )
				{
					ULONG size = 0;
					
					if(which == isSegment)
					{
						if((xch = xmlGetProp( son, "bytes")))
						{
							StrToLong( xch, &size );
							xmlFree( xch );
						}
					}
					
					xch = xmlNodeListGetString( son->doc, son->xmlChildrenNode, 1);
					
					DBG_STRING(xch);
					DBG_VALUE(size);
					
					if( xch )
					{
						if(*((char *) xch ))
						{
							if(AddNodeListItem(((which == isSegment) ? &entry->mlSegments : &entry->mlGroups), (char *)xch ))
							{
								if(which == isSegment)
								{
									entry->segments++;
									entry->size += size;
								}
							}
							else OutOfMemory("adding nzb item");
						}
						
						xmlFree(xch);
					}
				}
			}
		}
	}
	
	LEAVE();
}

HOOKPROTONHNO( ConstructFunc, struct NList_Entry *, struct NList_ConstructMessage * msg )
{
	struct NList_Entry * entry;
	struct NList_Single * nls = (struct NList_Single *) msg->entry;
	
	ENTER();
	
	if(nls->MagicID != NLSMID)
	{
		DBG(" **** handshaking error! **** \n");
		return NULL;
	}
	
	if((entry = AllocVec( sizeof(struct NList_Entry), MEMF_PUBLIC|MEMF_CLEAR)))
	{
		struct DateTime dt;
		UBYTE timeStr[LEN_DATSTRING], dateStr[LEN_DATSTRING];
		
		ProcessNZBFileNode( nls->file_node, entry, nls->Xml2Base );
		
		snprintf(entry->size_string, sizeof(entry->size_string)-1, 
			"\033r%lu", (ULONG) entry->size );
		
		dt.dat_Stamp.ds_Days   = entry->date_stamp.ds_Days   = entry->date / 86400;
		dt.dat_Stamp.ds_Minute = entry->date_stamp.ds_Minute = (entry->date % 86400) / 60;
		dt.dat_Stamp.ds_Tick   = entry->date_stamp.ds_Tick   = ((entry->date % 86400) % 60) * TICKS_PER_SECOND;
		
		dt.dat_Format  = FORMAT_DEF;
		dt.dat_Flags   = DTF_SUBST;
		dt.dat_StrDay  = NULL;
		dt.dat_StrDate = dateStr;
		dt.dat_StrTime = timeStr;
		DateToStr(&dt);
		
		snprintf( entry->date_string, sizeof(entry->date_string)-1, "%s %s", dateStr, timeStr);
		
		snprintf(entry->segments_string, sizeof(entry->segments_string)-1,
			"\033r%lu ", (ULONG) entry->segments );
	}
	else OutOfMemory("Analyzer ConstructHook");
	
	RETURN(entry);
	return(entry);
}
MakeStaticHook( ConstructHook, ConstructFunc );

//---------------------------------------------- NList Destruct Hook -------

HOOKPROTONHNO( DestructFunc, VOID, struct NList_DestructMessage * msg )
{
	struct NList_Entry * entry = (struct NList_Entry *) msg->entry;
	
	if( entry )
	{
		FreeNodeListItem( &entry->mlSegments );
		FreeNodeListItem( &entry->mlGroups );
		
		FreeVec( entry );
	}
}
MakeStaticHook( DestructHook, DestructFunc );

//---------------------------------------------- NList Display Hook --------

HOOKPROTONHNO( DisplayFunc, long, struct NList_DisplayMessage * msg )
{
	struct NList_Entry * entry = (struct NList_Entry *) msg->entry;
	
	if( entry == NULL )
	{
		msg->strings[FiLE] = "File";
		msg->strings[SIZE] = "Size";
		msg->strings[DATE] = "Date";
		msg->strings[POST] = "Poster";
		msg->strings[SEGM] = "Pieces";//"Segments";
	//	msg->strings[GROU] = "Group";
	}
	else
	{
		msg->strings[FiLE] = entry->file;
		msg->strings[SIZE] = entry->size_string;
		msg->strings[DATE] = entry->date_string;
		msg->strings[POST] = entry->poster;
		msg->strings[SEGM] = entry->segments_string;
	//	msg->strings[GROU] = entry->group;
	}
	return 0;
}
MakeStaticHook( DisplayHook, DisplayFunc );

//---------------------------------------------- NList Compare Hook --------

STATIC LONG cmpcol( struct NList_Entry *e1, struct NList_Entry *e2, ULONG column )
{
	switch (column)
	{
		default:
		case FiLE:
			return Stricmp( e1->file, e2->file );
		
		case SIZE:
			return e1->size - e2->size;
		
		case DATE:
			return -CompareDates(&e1->date_stamp,&e2->date_stamp);
		
		case POST:
			return Stricmp( e1->poster, e2->poster );
		
		case SEGM:
			return e1->segments - e2->segments;
		
		//case GROU:
		//	return Stricmp( e1->group, e2->group );
	}
}

HOOKPROTONHNO( CompareFunc, LONG, struct NList_CompareMessage * msg )
{
	struct NList_Entry *e1, *e2;
	ULONG col1, col2;
	LONG result;

	e1 = (struct NList_Entry *)msg->entry1;
	e2 = (struct NList_Entry *)msg->entry2;
	col1 = msg->sort_type & MUIV_NList_TitleMark_ColMask;
	col2 = msg->sort_type2 & MUIV_NList_TitleMark2_ColMask;

	if (msg->sort_type == (LONG)MUIV_NList_SortType_None) return 0;

	if (msg->sort_type & MUIV_NList_TitleMark_TypeMask) {
	    result = cmpcol( e2, e1, col1);
	} else {
	    result = cmpcol( e1, e2, col1);
	}

	if (result != 0 || col1 == col2) return result;

	if (msg->sort_type2 & MUIV_NList_TitleMark2_TypeMask) {
	    result = cmpcol( e2, e1, col2);
	} else {
	    result = cmpcol( e1, e2, col2);
	}

	return result;
}
MakeStaticHook( CompareHook, CompareFunc );

//------------------------------------------------------------------------------

STATIC BOOL LoadNZBFile(Object *app,Object *window,Object *list,STRPTR nzbFile);
STATIC VOID DownloadFile(Object *app,Object *window,Object *list);

#if 0
// MUIMasterBase is thread-safe ?...
#define MUIMasterBase PRIVATE_MUIMasterBase
#define MUI_NewObject PRIVATE_MUI_NewObject

STATIC Object * MUI_NewObject(char *classname, Tag tag1, ...)
{
	return(MUI_NewObjectA(classname, (struct TagItem *) &tag1));
}
#undef MUIMasterBase
#endif

STATIC VOID Analyzer_SubTask(STRPTR nzbFile)
{
	struct Library * MUIMasterBase;
	Object * refresh, * download;
	
	ENTER();
	DBG_STRING(nzbFile);
	
	if((MUIMasterBase = OpenLibrary(MUIMASTER_NAME,MUIMASTER_VMIN)))
	{
		Object * app, * window, * list;
		
		//ObtainSemaphore(&AnalyzerSemaphore);
		//PRIVATE_MUIMasterBase = MUIMasterBase;
		
		app = ApplicationObject,
			MUIA_Application_Title      , "AmiNZB.Analyzer",
			MUIA_Application_Base       , "AmiNZB.Analyzer",
			MUIA_Application_Version    , ProgramVersionTag(),
			MUIA_Application_Description, ProgramDescription(),
			
			SubWindow, window = WindowObject,
				MUIA_Window_Title, FilePart(nzbFile),
				MUIA_Window_ID, MAKE_ID('N','Z','B','A'),
				WindowContents, VGroup,
					Child, HGroup,
						Child, VSpace(0),
						Child, refresh = MUI_MakeObject(MUIO_Button, "Reload file"),
						Child, download = MUI_MakeObject(MUIO_Button, "Download selected"),
						Child, HVSpace,
					End,
					Child, NListviewObject,
						MUIA_NListview_NList, list = NListObject,
							MUIA_ObjectID,			MAKE_ID('A','L','S','T'),
							MUIA_NList_DefaultObjectOnClick, TRUE,
							MUIA_NList_MultiSelect,		MUIV_NList_MultiSelect_Always,
							MUIA_NList_DisplayHook2,	&DisplayHook,
							MUIA_NList_CompareHook2,	&CompareHook,
							MUIA_NList_ConstructHook2,	&ConstructHook,
							MUIA_NList_DestructHook2,	&DestructHook,
							MUIA_NList_AutoVisible, 	TRUE,
							MUIA_NList_TitleSeparator,	TRUE,
							MUIA_NList_Title,		TRUE,
							MUIA_NList_MinColSortable,	0,
							MUIA_NList_Imports,		MUIV_NList_Imports_All,
							MUIA_NList_Exports,		MUIV_NList_Exports_All,
							MUIA_NList_Format, ",,,,,,",
						End,
					End,
				End,
			End,
		End;
		
		//PRIVATE_MUIMasterBase = NULL;
		//ReleaseSemaphore(&AnalyzerSemaphore);
		
		#define RID_REFRESH	0xf0000f01
		#define RID_DOWNLOAD	0xf0000f10
		
		if( app )
		{
			DoMethod(window,MUIM_Notify,MUIA_Window_CloseRequest,TRUE,
				app,2,MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);
			
			DoMethod( refresh, MUIM_Notify,MUIA_Pressed,FALSE,
				app,2,MUIM_Application_ReturnID, RID_REFRESH);
			
			DoMethod( download, MUIM_Notify,MUIA_Pressed,FALSE,
				app,2,MUIM_Application_ReturnID, RID_DOWNLOAD);
			
			MakeColsSortable( list );
			
			DoMethod( app, MUIM_Application_Load, MUIV_Application_Load_ENVARC );
			
			set(window,MUIA_Window_Open,TRUE);
			
			if(xget( window, MUIA_Window_Open))
			{
				ULONG sigs = 0;
				BOOL running = TRUE;
				struct Window * iWindow;
				
				if((iWindow = (struct Window *) xget( window, MUIA_Window_Window)))
					((struct Process *)FindTask(0))->pr_WindowPtr = iWindow;
				
				if(LoadNZBFile(app,window,list,nzbFile))
				{
					// Ther is no need to refresh if the file 
					// is loaded correctly, if fex xml2.library
					// cannot be opened it could be needed
					
					set( refresh, MUIA_ShowMe, FALSE );
				}
				
				do {
					switch(DoMethod(app,MUIM_Application_NewInput,&sigs))
					{
						case MUIV_Application_ReturnID_Quit:
							running = FALSE;
							break;
						
						case RID_REFRESH:
							DoMethod( list, MUIM_NList_Clear, TRUE );
							LoadNZBFile(app,window,list,nzbFile);
							break;
						
						case RID_DOWNLOAD:
							DownloadFile(app,window,list);
							break;
						
						default:
							break;
					}
					if(running && sigs)
					{
						sigs = Wait(sigs | SIGBREAKF_CTRL_C);
						if (sigs & SIGBREAKF_CTRL_C) break;
					}
				} while(running);
				
				set(window,MUIA_Window_Open,FALSE);
				DoMethod( app, MUIM_Application_Save, MUIV_Application_Save_ENVARC );
			}
			else __request("Cannot open NZB Analyzer window!...");
			
			MUI_DisposeObject(app);
		}
		else __request("Failed to create NZB Analyzer Application!");
		
		CloseLibrary(MUIMasterBase);
	}
	
	FreeVec( nzbFile ); // it is allocated at Analyzer()
	
	LEAVE();
}

//------------------------------------------------------------------------------

STATIC VOID DownloadFile(Object *app,Object *window,Object *list)
{
	ULONG id = MUIV_NList_NextSelected_Start;
	struct dlNode * dl = NULL, * new_dl = NULL;
	BOOL fatal = FALSE, something_added = FALSE;
	ULONG time = Time(NULL);
	
	ENTER();
	set(app, MUIA_Application_Sleep, TRUE);
	
	do {
		struct NList_Entry *entry = NULL;
		
		DoMethod( list, MUIM_NList_NextSelected, &id );
		if( id == (ULONG)MUIV_NList_NextSelected_End ) break;
		
		DoMethod( list, MUIM_NList_GetEntry, id, &entry );
		if(!entry) continue;
		
		fatal = TRUE;
		DBG_POINTER(entry);
		
		if((new_dl = AllocVec(sizeof(struct dlNode), MEMF_PUBLIC|MEMF_CLEAR)))
		{
			NewList((struct List *) &new_dl->segments );
			NewList((struct List *) &new_dl->groups );
			
			if(DupNodeListItem(&new_dl->segments,&entry->mlSegments))
			{
				if(DupNodeListItem(&new_dl->groups,&entry->mlGroups))
				{
					CopyMem( entry->file, new_dl->subject, sizeof(new_dl->subject));
					CopyMem( entry->poster, new_dl->poster, sizeof(new_dl->poster));
					
					new_dl->id = time;
					new_dl->date = entry->date;
					new_dl->size = entry->size;
					new_dl->segnum = entry->segments;
					new_dl->next = dl;
					dl = new_dl;
					
					fatal = FALSE;
					something_added = TRUE;
					
					DBG_VALUE(new_dl->id);
					DBG_POINTER(new_dl->subject);
				}
			}
		}
	} while(!fatal);
	
	set(app, MUIA_Application_Sleep, FALSE);
	
	if( fatal == TRUE )
	{
		OutOfMemory("No new download will happend!");
		
		FreeDlNode( dl );
		FreeDlNode( new_dl );
	}
	else if( something_added == FALSE )
	{
		__request("Nothing selected!");
	}
	else
	{
		DownloadStart( dl );
	}
	
	LEAVE();
}

//------------------------------------------------------------------------------

STATIC VOID nzbFileError(STRPTR nzbFile,STRPTR msg)
{
	request("%s: %s", FilePart(nzbFile), msg );
}

STATIC BOOL LoadNZBFile(Object *app,Object *window,Object *list,STRPTR nzbFile)
{
	struct Xml2Base * Xml2Base;
	BOOL rc = FALSE;
	
	ENTER();
	set( app, MUIA_Application_Sleep, TRUE );
	
	if((Xml2Base = (struct Xml2Base *) OpenLibrary("xml2.library", 4)))
	{
		xmlDoc *doc = NULL;
		
		if((doc = xmlParseFile(nzbFile)))
		{
			xmlNode *root = NULL;
			
			if((root = xmlDocGetRootElement(doc)) != NULL)
			{
				if(xmlStrcasecmp( root->name, "NZB"))
				{
					nzbFileError(nzbFile,"XML file isn't of type NZB!");
				}
				else
				{
					xmlNodePtr node;
					
					for( node = root->xmlChildrenNode ; node ; node = node->next )
					{
						DBG_STRING(node->name);
						
						if(!xmlStrcasecmp( node->name, "file"))
						{
							STATIC struct NList_Single nls;
							
							nls.MagicID   = NLSMID;
							nls.Xml2Base  = Xml2Base;
							nls.file_node = node;
							
							//DBG(" ---- InsertSingle - START\n");
							DoMethod( list, MUIM_NList_InsertSingle,(APTR) &nls, MUIV_NList_Insert_Bottom);
							//DBG(" ---- InsertSingle - END\n");
						}
						
						if(SetSignal(0L,SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
							break;
					}
					
					rc = TRUE;
				}
			}
			else nzbFileError(nzbFile,"Cannot obtain root element");
			
			xmlFreeDoc(doc);
		}
		else nzbFileError(nzbFile,"Cannot parse XML data");
		
		xmlCleanupParser ( ) ;
		CloseLibrary((struct Library *) Xml2Base );
	}
	else __request("Cannot open xml2.library v4.0 or higher");
	
	set( app, MUIA_Application_Sleep, FALSE );
	RETURN(rc);
	return rc;
}

// gcc -noixemul -DDEBUG -DTEST_NZBLOADER analyzer.c debug.c thread.c -o analyzer -s
#ifdef TEST_NZBLOADER
int main(){LoadNZBFile(0,0,0,"help/NewzLeech943.nzb");}
#endif
//------------------------------------------------------------------------------

BOOL Analyzer(STRPTR nzbFile)
{
	STRPTR nzbFile_r = NULL;
	BOOL rc = FALSE;
	//STATIC BOOL FirstTime = TRUE;
	
	ENTER();
	DBG_STRING(nzbFile);
	
	/*if(FirstTime)
	{
		InitSemaphore(&AnalyzerSemaphore);
		FirstTime = FALSE;
	}*/
	
	if(nzbFile && *nzbFile)
	{
		ULONG nzbFile_len;
		
		// make a dup of the given nzbFile to be thread-safe...
		nzbFile_len = strlen(nzbFile);
		
		if(nzbFile_len > 0)
		{
			if((nzbFile_r = AllocVec(nzbFile_len + 2, MEMF_PUBLIC)))
			{
				APTR thr;
				
				CopyMem( nzbFile, nzbFile_r, nzbFile_len);
				nzbFile_r[nzbFile_len] = '\0';
				
				DBG_STRING(nzbFile_r);
				thr = QuickThread((APTR) Analyzer_SubTask,(APTR) nzbFile_r );
				
				if(thr == NULL)
				{
					__request("Cannot launch Analyzer SubTask!");
					
					FreeVec( nzbFile_r );
					nzbFile_r = NULL;
				}
				else rc = TRUE;
			}
		}
	}
	
	if( rc == FALSE )
	{
		__request("Analyzer Failure (sanity checks didn't succed)");
	}
	
	RETURN(rc);
	return rc;
}

