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


/** $Id: muiface.c,v 0.1 2006/07/04 03:42:46 diegocr Exp $
 **/

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <proto/alib.h>
#include <proto/asl.h>
#include <SDI_hook.h>
#include "muiface.h"
#include "thread.h"
#include "ipc.h"
#include "debug.h"
#include "analyzer.h"
#include "util.h"
#include "downler.h"
#include "http.h"
#include <stdio.h> /* snprintf */
#include <string.h> /* strlen */
#include <MUI/BetterBalance_mcc.h>
#include <MUI/BetterString_mcc.h>
#include <MUI/NListview_mcc.h>
#include <libraries/gadtools.h> /* NM_TITLE, etc */

GLOBAL Object * AmiNZB_Logo ( VOID );
GLOBAL VOID ObtainServerList( VOID );

//------------------------------------------------------------------------------
struct Library * MUIMasterBase = NULL;

STATIC STRPTR ApplicationData[] = {
	"AmiNZB",				/* title / base */
	"$VER: AmiNZB 0.4 (09.04.2007)",	/* Version */
	"©2007, Diego Casorran",		/* Copyright */
	"AmiNZB - NZB Download Manager",	/* Description */
	NULL
};

STATIC struct MsgPort * mPort = NULL;
STATIC struct Task * mTask = NULL;

Object * MainApp = NULL, * MainWindow, *dList, *iList;

struct zSettings
{
	Object * Window;
	Object * nntp_host,* nntp_port,* nntp_user,* nntp_pass;
	Object * incoming, * completed;
	Object * saveONexit;
};
STATIC struct zSettings *Settings;

//------------------------------------------------------------------------------

enum {
	MENU_BASE = 0x1f3f,
	MENU_LOAD, MENU_ABOUT, MENU_AMUI, MENU_HIDE, MENU_QUIT,
	MENU_CONF, MENU_MUI
};

struct NewMenu MainMenu[] =
{
   { NM_TITLE, (STRPTR)"Project"                 , 0 ,0,0,(APTR)0             },
   { NM_ITEM , (STRPTR)"Load NZB..."             ,"L",0,0,(APTR)MENU_LOAD     },
   { NM_ITEM , (STRPTR)NM_BARLABEL               , 0 ,0,0,(APTR)0             },
   { NM_ITEM , (STRPTR)"About"                   ,"?",0,0,(APTR)MENU_ABOUT    },
   { NM_ITEM , (STRPTR)"About MUI"               , 0 ,0,0,(APTR)MENU_AMUI     },
   { NM_ITEM , (STRPTR)NM_BARLABEL               , 0 ,0,0,(APTR)0             },
   { NM_ITEM , (STRPTR)"Hide"                    ,"H",0,0,(APTR)MENU_HIDE     },
   { NM_ITEM , (STRPTR)"Quit"                    ,"Q",0,0,(APTR)MENU_QUIT     },

   { NM_TITLE, (STRPTR)"Settings"                , 0 ,0,0,(APTR)0             },
   { NM_ITEM , (STRPTR)"AmiNZB"                  , 0 ,0,0,(APTR)MENU_CONF     },
   { NM_ITEM , (STRPTR)"MUI"                     , 0 ,0,0,(APTR)MENU_MUI      },

   { NM_END,NULL,0,0,0,(APTR)0 },
};

STATIC BOOL NeedConfiguration( VOID );

//------------------------------------------------------------------------------

struct NList_Entry
{
	ULONG task;
	UBYTE task_str[32];
	UBYTE file[128];
	UBYTE size[32];
	UBYTE segments[32];
	UBYTE cps[32];
	UBYTE downloaded[32]; // the data downloaded COULD be higher than the file's size
	
	UBYTE status[1024];
};
enum { TASK, FiLE, SIZE, SEGM, DOWN, CCPS, STAT };

//---------------------------------------------- NList Construct Hook ------

HOOKPROTONHNO( ConstructFunc, APTR, struct NList_ConstructMessage * msg )
{
	struct NList_Entry * entry;
	
	if((entry = AllocVec( sizeof(struct NList_Entry), MEMF_ANY|MEMF_CLEAR)))
	{
		struct dlStatus * st = (struct dlStatus *) msg->entry;
		
		snprintf( entry->task_str, sizeof(entry->task_str)-1,
			"\033c\0338$%08lx",(ULONG)(entry->task = st->task));
		
		CopyMem( st->file, entry->file, sizeof(entry->file)-1);
		CopyMem( st->status, entry->status, sizeof(entry->status)-1);
		
		snprintf(entry->size,sizeof(entry->size)-1,"\033r%lu ",st->size_total);
		snprintf(entry->downloaded,sizeof(entry->downloaded)-1,"\033r%lu ",st->size_current);
		
		snprintf(entry->segments,sizeof(entry->segments)-1,"\033c%i/%i",
			st->segment_current, st->segments_total );
		
		snprintf(entry->cps,sizeof(entry->cps)-1,"%lu", st->cps );
	}
	return (APTR) entry;
}
MakeStaticHook( ConstructHook, ConstructFunc );

//---------------------------------------------- NList Destruct Hook -------

HOOKPROTONHNO( DestructFunc, VOID, struct NList_DestructMessage * msg )
{
	if( msg->entry )
		FreeVec( msg->entry );
}
MakeStaticHook( DestructHook, DestructFunc );

//---------------------------------------------- NList Display Hook --------

HOOKPROTONHNO( DisplayFunc, long, struct NList_DisplayMessage * msg )
{
	struct NList_Entry * entry = (struct NList_Entry *) msg->entry;
	
	if( entry == NULL )
	{
		msg->strings[TASK] = "Manager";
		msg->strings[FiLE] = "File";
		msg->strings[SIZE] = "Size";
		msg->strings[SEGM] = "Segments";
		msg->strings[DOWN] = "Downloaded";
		msg->strings[CCPS] = "CPS";
		msg->strings[STAT] = "Status";
	}
	else
	{
		msg->strings[TASK] = entry->task_str;
		msg->strings[FiLE] = entry->file;
		msg->strings[SIZE] = entry->size;
		msg->strings[SEGM] = entry->segments;
		msg->strings[DOWN] = entry->downloaded;
		msg->strings[CCPS] = entry->cps;
		msg->strings[STAT] = entry->status;
	}
	
	return 0;
}
MakeStaticHook( DisplayHook, DisplayFunc );

//---------------------------------------------- NList Compare Hook --------

#define Atoi(string) ({ LONG v; StrToLong( string, &v); v ; })

STATIC LONG cmpcol(struct NList_Entry *e1, struct NList_Entry *e2, ULONG column)
{
	switch (column)
	{
		default:
		case FiLE:
			return Stricmp( e1->file, e2->file );
		
		case SIZE:
			return Atoi(e1->size) - Atoi(e2->size);
		
		case TASK:
			return e1->task - e2->task;
		
		case SEGM:
			return Stricmp( e1->segments, e2->segments );
		
		case DOWN:
			return Stricmp( e1->downloaded, e2->downloaded );
		
		case CCPS:
			return Stricmp( e1->cps, e2->cps );
		
		case STAT:
			return Stricmp( e1->status, e2->status );
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

STATIC Object * mStrObject( STRPTR ShortHelp, ULONG ObjectID, ULONG String_MaxLen )
{
	return BetterStringObject,
		StringFrame,
		MUIA_String_MaxLen	, String_MaxLen,
		MUIA_String_AdvanceOnCR	, TRUE,
		MUIA_CycleChain		, TRUE,
		MUIA_ShortHelp		, (ULONG)ShortHelp,
		MUIA_ObjectID		, ObjectID,
	End;
}

STATIC Object * mStrObjectN( STRPTR ShortHelp, ULONG ObjectID, ULONG String_MaxLen, ULONG Integer )
{
	Object * obj;
	
	if((obj = mStrObject( ShortHelp, ObjectID, String_MaxLen )))
	{
		SetAttrs( obj,
			MUIA_String_Integer	, Integer,
			MUIA_String_Accept	, (ULONG)"0123456789",
		TAG_DONE);
	}
	
	return obj;
}

STATIC Object * mStrObjectS( STRPTR ShortHelp, ULONG ObjectID, ULONG String_MaxLen )
{
	Object * obj;
	
	if((obj = mStrObject( ShortHelp, ObjectID, String_MaxLen )))
	{
		SetAttrs( obj,
			MUIA_String_Secret	, TRUE,
		TAG_DONE);
	}
	
	return obj;
}

STATIC Object * mPopStr( STRPTR aslReqTxt, ULONG ObjectID )
{
	return PopaslObject,
		MUIA_Popstring_String, mStrObject( aslReqTxt, ObjectID, SAVEFOLDERLEN),
		MUIA_Popstring_Button, PopButton(MUII_PopDrawer),
		ASLFR_TitleText, (ULONG) aslReqTxt,
		ASLFR_DrawersOnly, TRUE,
	End;
}

//------------------------------------------------------------------------------

SAVEDS ASM VOID LoadNZB_HookFunc( VOID )
{
	struct Library * AslBase;
	
	ENTER();
	
	if((AslBase = OpenLibrary("asl.library", 0)))
	{
		struct FileRequester *freq;
		
		if((freq = AllocAslRequestTags(ASL_FileRequest, TAG_DONE)))
		{
			if(AslRequestTags(freq,
				ASLFR_TitleText, (ULONG)"Load NZB File...",
				ASLFR_InitialDrawer, (ULONG)"PROGDIR:",
				ASLFR_RejectIcons, TRUE,
				ASLFR_DoPatterns, TRUE,
				ASLFR_InitialPattern, (ULONG)"#?.nzb",
				TAG_DONE))
			{
				STRPTR name;
				ULONG namelen;
				
				namelen = strlen(freq->fr_File) + strlen(freq->fr_Drawer) + 32;
				
				if((name = AllocVec(namelen + 1, MEMF_PUBLIC | MEMF_CLEAR)))
				{
					AddPart(name, freq->fr_Drawer, namelen);
					AddPart(name, freq->fr_File, namelen);
					
					DBG_STRING(name);
					Analyzer(name);
					
					FreeVec(name);
				}
				else OutOfMemory("asl.library");
			}
			
			FreeAslRequest(freq);
		}
		else __request("Cannot alloc AslRequest!");
		
		CloseLibrary( AslBase );
	}
	else __request("Cannot open asl.library!");
	
	LEAVE();
}

//------------------------------------------------------------------------------

SAVEDS ASM VOID CheckSettings_HookFunc( VOID )
{
	LONG ok = 0;
	char __fib[sizeof(struct FileInfoBlock) + 3];
	struct FileInfoBlock *fib = 
		(struct FileInfoBlock *)(((long)__fib + 3L) & ~3L);
	
	ENTER();
	
	if(NeedConfiguration())
	{
		DisplayBeep(NULL);
		request("Required fields are missing...");
	}
	else
	{
		STRPTR path;
		BPTR lock;
		
		ok++;
		
		path = IncomingFolder();
		if((lock = Lock( path, SHARED_LOCK)))
		{
			if(Examine( lock, fib)!=DOSFALSE)
			{
				if(fib->fib_DirEntryType > 0)
					ok++; // it is a valid directory
				else {
					request("The entry \"%s\" seems not a valid directory!", path);
				}
			}
			UnLock(lock);
		}
		else {
			request("The entry \"%s\" seems not a valid directory!", path);
		}
		
		path = CompletedFolder();
		if((lock = Lock( path, SHARED_LOCK)))
		{
			if(Examine( lock, fib)!=DOSFALSE)
			{
				if(fib->fib_DirEntryType > 0)
					ok++; // it is a valid directory
				else {
					request("The entry \"%s\" seems not a valid directory!", path);
				}
			}
			UnLock(lock);
		}
		else {
			request("The entry \"%s\" seems not a valid directory!", path);
		}
	}
	
	if( ok == 3 )
	{
		set(Settings->Window,MUIA_Window_Open,FALSE);
		
		if( mTask->tc_UserData != NULL ) // that must be a filename passed at the command line...
		{
			Analyzer((STRPTR)mTask->tc_UserData);
		}
	}
	
	LEAVE();
}

//------------------------------------------------------------------------------

SAVEDS ASM VOID ObtainFreeServer_HookFunc( VOID )
{
	LONG answer;
	ENTER();
	
	answer = MUI_Request(MainApp,MainWindow,0,ProgramName(),"*_Yes|_No",
		"We'll connect to an http server to receive a list of public free servers,\n"
		"once received a connection test will be perfomed on each server, which\n"
		"could take some time (long time if the hosts aren't up!)\n\n"
		"Do you want to continue ?");
	
	if( answer == TRUE )
		ObtainFreeServer();
	
	LEAVE();
}

//------------------------------------------------------------------------------

SAVEDS ASM VOID ObtainServerList_HookFunc( VOID )
{
	LONG answer;
	ENTER();
	
	answer = MUI_Request(MainApp,MainWindow,0,ProgramName(),"*_Yes|_No",
		"This operation could take long time to complete.\n"
		"\n"
		"Do you want to continue ?");
	
	if( answer == TRUE )
		ObtainServerList();
	
	LEAVE();
}

//------------------------------------------------------------------------------

BOOL muiface(STRPTR nzbFile)
{
	BOOL success = FALSE;
	ULONG mPortSig = 0;
	BOOL remport = FALSE, twice = FALSE;
	Object *AboutWindow, *AboutClose, *ObtainFreeServer, *ObtainServerList;
	static const struct Hook LoadNZB_Hook = { { NULL,NULL },(VOID *)LoadNZB_HookFunc,NULL,NULL };
	static const struct Hook CheckSettings_Hook = { { NULL,NULL },(VOID *)CheckSettings_HookFunc,NULL,NULL };
	static const struct Hook ObtainFreeServer_Hook = { { NULL,NULL },(VOID *)ObtainFreeServer_HookFunc,NULL,NULL };
	static const struct Hook ObtainServerList_Hook = { { NULL,NULL },(VOID *)ObtainServerList_HookFunc,NULL,NULL };
	
	ENTER();
	
	if((mPort = CreateMsgPort()))
	{
		mPort->mp_Node.ln_Pri = 0;
		
		Forbid();
		if(!FindPort(ProgramName()))
		{
			mPort->mp_Node.ln_Name = ProgramName();
			AddPort( mPort );
			remport = TRUE;
		}
		else twice = TRUE;
		Permit();
	}
	
	if( twice == TRUE )
	{
		__request("This program cannot be launch twice!");
		goto done;
	}
	
	if (!(MUIMasterBase = OpenLibrary(MUIMASTER_NAME,MUIMASTER_VMIN)))
	{
		__request("This program requires MUI!");
		goto done;
	}
	
	if(!(Settings = AllocVec(sizeof(struct zSettings),MEMF_PUBLIC|MEMF_CLEAR)))
	{
		OutOfMemory("");
		goto done;
	}
	
	MainApp = ApplicationObject,
		MUIA_Application_Title      , ApplicationData[0],
		MUIA_Application_Version    , ApplicationData[1],
		MUIA_Application_Copyright  , ApplicationData[2],
		MUIA_Application_Author     , ApplicationData[2] + 7,
		MUIA_Application_Description, ApplicationData[3],
		MUIA_Application_Base       , ApplicationData[0],
		MUIA_Application_SingleTask , TRUE,
		MUIA_Application_UseRexx    , FALSE,
		MUIA_Application_Menustrip  , MUI_MakeObject( MUIO_MenustripNM, MainMenu, 0),
		
		SubWindow, MainWindow = WindowObject,
			MUIA_Window_Title, ApplicationData[3],
			MUIA_Window_ID, MAKE_ID('M','A','I','N'),
			WindowContents, HGroup,
				GroupSpacing( 0 ),
				InnerSpacing(1,1),
				MUIA_Background, (ULONG)"2:00000000,00000000,00000000",
				Child, VGroup,
					GroupSpacing(0),
					Child, VirtgroupObject,
						MUIA_Background, (ULONG)"2:ffffffff,ffffffff,ffffffff",
						MUIA_HorizDisappear, 1,
						Child, 	AmiNZB_Logo ( ),
					End,
				End,
				Child, BetterBalanceObject,
					MUIA_ObjectID, MAKE_ID('B','A','L','2'),
				End,
				Child, VGroup,
					GroupSpacing(0),
					Child, NListviewObject,
						MUIA_NListview_NList, dList = NListObject,
							TextFrame,
							GroupSpacing(0),
							MUIA_ObjectID,			MAKE_ID('D','L','S','T'),
							MUIA_NList_DefaultObjectOnClick, TRUE,
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
					Child, BetterBalanceObject,
						MUIA_ObjectID, MAKE_ID('B','A','L','1'),
					End,
					Child, NListviewObject,
						MUIA_Weight, 10,
						MUIA_NListview_NList, iList = NListObject,
							MUIA_ObjectID,		  MAKE_ID('I','N','F','O'),
							MUIA_NList_ConstructHook, MUIV_NList_ConstructHook_String,
							MUIA_NList_DestructHook,  MUIV_NList_DestructHook_String,
							MUIA_NList_AutoVisible,   TRUE,
							MUIA_NList_Input,         FALSE,
							MUIA_ContextMenu, MUIV_NList_ContextMenu_Never,
						End,
					End,
				End,
			End,
		End,
		
		SubWindow, Settings->Window = WindowObject,
			MUIA_Window_Title, "Settings",
			MUIA_Window_ID, MAKE_ID('S','E','T','T'),
			WindowContents, VGroup,
				Child, ColGroup(2), GroupFrameT("Server Details"),
					Child, Label2("SERVER"),
					Child, HGroup,
						GroupSpacing(2),
						Child, Settings->nntp_host = mStrObject ("",MAKE_ID('H','O','S','T'),255),
						Child, Settings->nntp_port = mStrObjectN("",MAKE_ID('P','O','R','T'),6,119),
					End,
					Child, Label2("Username"),
					Child, Settings->nntp_user = mStrObject ("",MAKE_ID('U','S','E','R'),255),
					Child, Label2("Password"),
					Child, Settings->nntp_pass = mStrObjectS("",MAKE_ID('P','A','S','S'),255),
					Child, HVSpace,
					Child, HGroup,
						Child, ObtainFreeServer = MUI_MakeObject(MUIO_Button, "Obtain Free Server"),
						Child, ObtainServerList = MUI_MakeObject(MUIO_Button, "Obtain Groups List"),
					End,
				End,
				Child, ColGroup(2), GroupFrameT(NULL),
					Child, Label2("Incoming"),
					Child, Settings->incoming = mPopStr("Folder where to save files being downloaded", MAKE_ID('I','N','C','O')),
					Child, Label2("Completed"),
					Child, Settings->completed = mPopStr("Folder where to save completed downloads", MAKE_ID('C','o','M','P')),
				End,
				Child, ColGroup(3),
					Child, Settings->saveONexit = MUI_MakeObject(MUIO_Checkmark, 0L),
					Child, Label("Save settings on exit"),
					Child, HVSpace,
				End,
			End,
		End,
		
		SubWindow, AboutWindow = WindowObject,
			MUIA_Window_Borderless , TRUE,
			MUIA_Window_CloseGadget, FALSE,
			MUIA_Window_SizeGadget , FALSE,
			MUIA_Window_DepthGadget, FALSE,
			MUIA_Window_DragBar    , FALSE,
			WindowContents, VGroup,
				VirtualFrame,
				MUIA_Background, MUII_WindowBack,
				Child, VGroup,
					TextFrame,
					MUIA_Background, MUII_TextBack,
					InnerSpacing(12,20),
					Child, TextObject,
						MUIA_Text_Contents, (ULONG) ApplicationData[3],
						MUIA_Text_PreParse, "\033b\033c",
						MUIA_Font, MUIV_Font_Big,
					End,
					Child, TextObject,
						MUIA_Text_Contents, ApplicationData[1] + 6,
						MUIA_Text_PreParse, "\033b\033c\0338",
					End,
					Child, TextObject,
						MUIA_Text_Contents, ApplicationData[2],
						MUIA_Text_PreParse, "\033c",
					End,
					Child, TextObject,
						MUIA_Text_Contents, "All Rights Reserved",
						MUIA_Text_PreParse, "\033c",
					End,
				End,
				Child, ColGroup(3),
					Child, HVSpace,
					Child, AboutClose = MUI_MakeObject(MUIO_Button, "Close Window"),
					Child, HVSpace,
				End,
			End,
		End,
	End;

	if( ! MainApp ) {
		__request("Failed to create Application!");
		goto done;
	}
	
	DoMethod(MainWindow,MUIM_Notify,MUIA_Window_CloseRequest,TRUE,
		MainApp,2,MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);
	
	DoMethod(Settings->Window,MUIM_Notify,MUIA_Window_CloseRequest,TRUE,
		MUIV_Notify_Self, 2, MUIM_CallHook, &CheckSettings_Hook );
	
	DoMethod( AboutClose, MUIM_Notify, MUIA_Pressed, FALSE,
		AboutWindow, 3, MUIM_Set, MUIA_Window_Open, FALSE );
	
	MakeColsSortable( dList );
	
	DoMethod( MainApp, MUIM_Notify, MUIA_Application_MenuAction, MENU_CONF,
		Settings->Window, 3, MUIM_Set, MUIA_Window_Open, TRUE );
	
	DoMethod( MainApp, MUIM_Notify, MUIA_Application_MenuAction, MENU_ABOUT,
		AboutWindow, 3, MUIM_Set, MUIA_Window_Open, TRUE );
	
	DoMethod( ObtainFreeServer, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Self, 2, MUIM_CallHook, &ObtainFreeServer_Hook );
	DoMethod( ObtainServerList, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Self, 2, MUIM_CallHook, &ObtainServerList_Hook );
	
	DoMethod( MainApp, MUIM_Notify, MUIA_Application_MenuAction, MENU_AMUI, MUIV_Notify_Self, 2, MUIM_Application_AboutMUI, MainWindow);
	DoMethod( MainApp, MUIM_Notify, MUIA_Application_MenuAction, MENU_HIDE, MUIV_Notify_Self, 3, MUIM_Set, MUIA_Application_Iconified, TRUE);
	DoMethod( MainApp, MUIM_Notify, MUIA_Application_MenuAction, MENU_QUIT, MUIV_Notify_Self, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
	DoMethod( MainApp, MUIM_Notify, MUIA_Application_MenuAction, MENU_MUI,  MUIV_Notify_Self, 2, MUIM_Application_OpenConfigWindow,0);
	DoMethod( MainApp, MUIM_Notify, MUIA_Application_MenuAction, MENU_LOAD, MUIV_Notify_Self, 2, MUIM_CallHook, &LoadNZB_Hook );
	
	set( Settings->nntp_port, MUIA_FixWidth, 48 );
	set( Settings->saveONexit, MUIA_Selected, TRUE );
	
	DoMethod( MainApp, MUIM_Application_Load, MUIV_Application_Load_ENVARC );
	
	set(MainWindow,MUIA_Window_Open,TRUE);
	
	if(xget( MainWindow, MUIA_Window_Open))
	{
		ULONG sigs = 0;
		struct Window * iWindow;
		UBYTE sm[64];
		
		mTask       = FindTask(NULL);
		mPortSig    = (1L << mPort->mp_SigBit);
		if((iWindow = (struct Window *) xget( MainWindow, MUIA_Window_Window)))
			((struct Process *)mTask)->pr_WindowPtr = iWindow;
		
		snprintf( sm, sizeof(sm)-1, "%s %s", ApplicationData[1]+6, ApplicationData[2]);
		nfo( sm );
		
		if(NeedConfiguration())
		{
			mTask->tc_UserData = (APTR) nzbFile;
			
			set(Settings->Window,MUIA_Window_Open,TRUE);
			
			request("Please, configure the program before continuing.");
		}
		else
		{
			mTask->tc_UserData = NULL;
			
			if( nzbFile != NULL )
			{
				Analyzer( nzbFile );
			}
			RecoverSavedDownloads();
		}
		
		while(DoMethod(MainApp,MUIM_Application_NewInput,&sigs) != (ULONG)MUIV_Application_ReturnID_Quit)
		{
			if (sigs)
			{
				sigs = Wait(sigs | mPortSig | tm->sigmask | SIGBREAKF_CTRL_C);
				if (sigs & SIGBREAKF_CTRL_C) break;
				
				aThreadWatch(sigs);
				
				if( sigs & mPortSig )
				{
					IPC_Dispatch( mPort );
				}
			}
		}
		
		set(MainWindow,MUIA_Window_Open,FALSE);
		
		if(xget( Settings->saveONexit, MUIA_Selected))
			DoMethod( MainApp, MUIM_Application_Save, MUIV_Application_Save_ENVARC );
		
		success = TRUE;
	}
	else __request("Cannot open main window...");
	
done:
	if(MainApp)
		MUI_DisposeObject(MainApp); MainApp = NULL;
	if (MUIMasterBase)
		CloseLibrary(MUIMasterBase);
	
	if( mPort )
	{
		APTR msg;
		while((msg = (APTR) GetMsg( mPort )))
			ReplyMsg((APTR)msg);
		
		if( remport )
			RemPort( mPort );
		
		DeleteMsgPort( mPort );
		mPort = NULL;
	}
	
	if(Settings)
		FreeVec(Settings);
	
	LEAVE();
	return(success);
}

//------------------------------------------------------------------------------

STRPTR ProgramName( VOID )
{
	return (STRPTR) ApplicationData[0];
}
STRPTR ProgramVersionTag( VOID )
{
	return (STRPTR) ApplicationData[1];
}
STRPTR ProgramDescription( VOID )
{
	return ApplicationData[3];
}
STRPTR ProgramCopyright( VOID )
{
	return ApplicationData[2];
}

//------------------------------------------------------------------------------

VOID MakeColsSortable( Object *list )
{
	DoMethod( list, MUIM_Notify, MUIA_NList_TitleClick,  MUIV_EveryTime, MUIV_Notify_Self, 4, MUIM_NList_Sort3, MUIV_TriggerValue, MUIV_NList_SortTypeAdd_2Values, MUIV_NList_Sort3_SortType_Both);
	DoMethod( list, MUIM_Notify, MUIA_NList_TitleClick2, MUIV_EveryTime, MUIV_Notify_Self, 4, MUIM_NList_Sort3, MUIV_TriggerValue, MUIV_NList_SortTypeAdd_2Values, MUIV_NList_Sort3_SortType_2);
	DoMethod( list, MUIM_Notify, MUIA_NList_SortType,    MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set, MUIA_NList_TitleMark,MUIV_TriggerValue);
	DoMethod( list, MUIM_Notify, MUIA_NList_SortType2,   MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set, MUIA_NList_TitleMark2,MUIV_TriggerValue);
}

//------------------------------------------------------------------------------

BOOL iListAdd(STRPTR msg)
{
	BOOL rc = TRUE;
	
	ENTER();
	DBG_STRING(msg);
	
	if( MainApp != NULL )
	{
		if(FindTask(NULL) == mTask)
		{
			iListInsert(msg);
		}
		else
		{
			rc = IPC_PutMsg( mPort, IPCA_INFOMSG, (APTR) msg );
		}
	}
	else {
		DBG(" ****  MainApp isnt valid, closing application?\n");
	}
	
	LEAVE();
	return rc;
}

VOID iListInsert(STRPTR msg)
{
	STATIC UBYTE fmt_msg[1024];
	
	/* DO NOT CALL THIS FUNCTION DIRECTLY!, use iListAdd INSTEAD! */
	
	ENTER();
	
	if(!Strnicmp( msg, "error", 5))
		DisplayBeep(NULL);
	
	#if 0
	snprintf( fmt_msg, sizeof(fmt_msg)-1,
		"\033l\033b\0338·\033n\0332 \033b%c\033n%s", *msg, &msg[1] );
	#else
	snprintf( fmt_msg, sizeof(fmt_msg)-1, "\033l\033b·\033n %s", msg );
	#endif
	
	DoMethod( iList, MUIM_NList_InsertSingleWrap, fmt_msg,
		MUIV_NList_Insert_Bottom, WRAPCOL0, ALIGN_LEFT );
	
	DoMethod( iList, MUIM_NList_Jump, MUIV_NList_Jump_Bottom);
	set(iList, MUIA_NList_Active, MUIV_NList_Active_PageDown);
	
	LEAVE();
}

//------------------------------------------------------------------------------

STATIC BOOL NeedConfiguration( VOID )
{
	STRPTR o;
	
	if(!((o = IncomingFolder()) && *o)) return TRUE;
	if(!((o = CompletedFolder()) && *o)) return TRUE;
	
	o = (STRPTR)xget( Settings->nntp_host, MUIA_String_Contents );
	if(!(o && *o)) return TRUE;
	
	o = (STRPTR)xget( Settings->nntp_port, MUIA_String_Contents );
	if(!(o && *o)) return TRUE;
	
	return FALSE;
}

VOID SettingsGetNntpAuthInfo( STRPTR *nntp_host, UWORD *nntp_port, STRPTR *nntp_user, STRPTR *nntp_pass )
{
	long port;
	
	*nntp_host = (STRPTR)xget( Settings->nntp_host, MUIA_String_Contents );
	*nntp_user = (STRPTR)xget( Settings->nntp_user, MUIA_String_Contents );
	*nntp_pass = (STRPTR)xget( Settings->nntp_pass, MUIA_String_Contents );
	
	StrToLong((STRPTR)xget( Settings->nntp_port, MUIA_String_Contents ), &port);
	*nntp_port = (UWORD) port;
}

VOID SettingsSetNntpAuthInfo( STRPTR nntp_host, UWORD nntp_port, STRPTR nntp_user, STRPTR nntp_pass )
{
	PushSet( Settings->nntp_host, 2, MUIA_String_Contents, nntp_host );
	PushSet( Settings->nntp_port, 2, MUIA_String_Integer, (ULONG)nntp_port );
	
	if( nntp_user != NULL )
		PushSet( Settings->nntp_user, 2, MUIA_String_Contents, nntp_user );
	if( nntp_pass != NULL )
		PushSet( Settings->nntp_pass, 2, MUIA_String_Contents, nntp_pass );
}

STRPTR IncomingFolder( VOID )
{
	return((STRPTR) xget( Settings->incoming, MUIA_String_Contents ));
}

STRPTR CompletedFolder( VOID )
{
	return((STRPTR) xget( Settings->completed, MUIA_String_Contents ));
}

//------------------------------------------------------------------------------

VOID dlStatusReport( APTR data )
{
	ENTER();
	
	//DoMethod( MainApp, MUIM_Application_PushMethod, dList, 3,
	//	MUIM_NList_InsertSingle, data, MUIV_NList_Insert_Bottom );
	
	IPC_PutMsg( mPort, IPCA_DLSTATUS, (APTR) data );
	LEAVE();
}

VOID dlStatusReportHandler( struct dlStatus * st )
{
	ENTER();
	
	if( st->magic == dlStatusMagicID)
	{
		struct NList_Entry * lEntry;
		long pos = -1;
		
		DBG_STRING(st->status);
		
		do {
			DoMethod( dList, MUIM_NList_GetEntry, ++pos, &lEntry);
			if(!lEntry) { pos = -1; break;}
			if(lEntry->task == st->task) break;
		} while(1);
		
		if(0>pos) {
			DoMethod( dList, MUIM_NList_InsertSingle,(APTR) st, MUIV_NList_Insert_Bottom);
		}
		else {
			DoMethod( dList, MUIM_NList_ReplaceSingle,(APTR) st, pos, NOWRAP, ALIGN_LEFT);
		}
	}
	
	LEAVE();
}

//------------------------------------------------------------------------------

