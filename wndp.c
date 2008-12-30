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
#include "muiface.h"
#include "thread.h"
#include "debug.h"
#include <MUI/Busy_mcc.h>

//------------------------------------------------------------------------------

STATIC VOID ProgressWindow_SubTask( STRPTR message )
{
	struct Library * MUIMasterBase;
	
	ENTER();
	DBG_STRING(message);
	
	if((MUIMasterBase = OpenLibrary(MUIMASTER_NAME,MUIMASTER_VMIN)))
	{
		Object * app, * window;
		
		app = ApplicationObject,
			MUIA_Application_Title      , "AmiNZB.WNDP",
			MUIA_Application_Base       , "AmiNZB.WNDP",
			MUIA_Application_Version    , ProgramVersionTag(),
			MUIA_Application_Description, ProgramDescription(),
			
			SubWindow, window = WindowObject,
				MUIA_Window_Borderless , TRUE,
				MUIA_Window_CloseGadget, FALSE,
				MUIA_Window_SizeGadget , FALSE,
				MUIA_Window_DepthGadget, FALSE,
				MUIA_Window_DragBar    , FALSE,
				MUIA_Window_ScreenTitle, ProgramDescription(),
				WindowContents, VGroup,  GroupFrameT(NULL),
					InnerSpacing(2,4),
					Child, TextObject,
						MUIA_Text_Contents, (ULONG)message,
						MUIA_Text_PreParse, "\033b\033c",
						MUIA_Font, MUIV_Font_Big,
					End,
					Child, BusyObject, End,
				End,
			End,
		End;
		
		if( app )
		{
			DoMethod(window,MUIM_Notify,MUIA_Window_CloseRequest,TRUE,
				app,2,MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);
			
			set(window,MUIA_Window_Open,TRUE);
			
			if(xget( window, MUIA_Window_Open))
			{
				ULONG sigs = 0;
				BOOL running = TRUE;
				struct Window * iWindow;
				
				if((iWindow = (struct Window *) xget( window, MUIA_Window_Window)))
				{
					((struct Process *)FindTask(0))->pr_WindowPtr = iWindow;
					
					//SetWindowTitles( iWindow, NULL, ProgramDescription());
				}
				
				do {
					switch(DoMethod(app,MUIM_Application_NewInput,&sigs))
					{
						case MUIV_Application_ReturnID_Quit:
							//running = FALSE;
							DisplayBeep(NULL);
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
			}
			
			MUI_DisposeObject(app);
		}
		
		CloseLibrary(MUIMasterBase);
	}
	
	LEAVE();
}

//------------------------------------------------------------------------------

APTR ProgressWindow( STRPTR message )
{
	APTR rc = NULL;
	
	ENTER();
	
	rc = QuickThread((APTR) ProgressWindow_SubTask,(APTR) message );
	
	RETURN(rc);
	return(rc);
}

