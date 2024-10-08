/* RCS Id: $Id: ConnectWindow.c 1.795 1996/09/28 13:32:58 lilja Exp $
   Locked version: $Revision: 1.795 $
*/

#include "AmiFTP.h"
#include "gui.h"

struct Window *ConnectWindow;
Object *ConnectLayout;
Object *ConnectWin_Object;

struct Window *OpenConnectWindow(void);
void CloseConnectWindow(void);

enum {
	CG_Host=0, CG_Status, CG_Abort,
	NumGadgets_CG
};

Object *CG_List[NumGadgets_CG];

int ConnectSite(struct SiteNode *sn, const BOOL noscan)
{
	BOOL Continue=TRUE;
	ULONG wmask,signal,mainwinsignal,done=FALSE;
	struct List *head;
	int retcode, result;
	extern BOOL SilentMode;
	extern ULONG MOTDDate;
DBUG("ConnectSite()\n",NULL);
	if (MainWindow)
		if (!OpenConnectWindow()) return CONN_GUI;

	LockWindow(MainWin_Object);
	//APTR lw = SleepWindow(MainWindow);

	if (ConnectWindow) {
		if (SetGadgetAttrs((struct Gadget*)CG_List[CG_Status], ConnectWindow, NULL,
		                   GA_Text,GetAmiFTPString(CW_Connecting),
		                  TAG_END))
			RefreshGList((struct Gadget*)CG_List[CG_Status],ConnectWindow,NULL,1);

		if (SetGadgetAttrs((struct Gadget*)CG_List[CG_Host],ConnectWindow, NULL,
		                   GA_Text,sn->sn_Node.ln_Name?sn->sn_Node.ln_Name:sn->sn_SiteAddress,
		                  TAG_END))
			RefreshGList((struct Gadget*)CG_List[CG_Host],ConnectWindow,NULL,1);
	}

	if ((result=doconnect(sn))==CONN_OK) {
		strncpy(CurrentState.CurrentSite, sn->sn_SiteAddress, 50);
		UpdateSiteName(CurrentState.CurrentSite);
		remote_os_type=sn->sn_VMSDIR;

		if (sn->sn_RemoteDir) {
			PrintConnectStatus(GetAmiFTPString(CW_ChangingDirectory));
			change_remote_dir(sn->sn_RemoteDir, 0);
		}

		ClearCache(TRUE);
		InitCache();
		if (!noscan) {
			if (ConnectWindow) 
				if (SetGadgetAttrs((struct Gadget*)CG_List[CG_Status], ConnectWindow, NULL,
				                   GA_Text, GetAmiFTPString(CW_ReadingDir),
				                  TAG_END))
					RefreshGList((struct Gadget*)CG_List[CG_Status], ConnectWindow, NULL, 1);

				if (head=sn->sn_ADT?ReadRecentList():read_remote_dir()) {
					if (MainWindow)
						SetGadgetAttrs((struct Gadget*)MG_List[MG_ListView], MainWindow, NULL,
						               LISTBROWSER_Labels, ~0, TAG_DONE);
					else
						SetAttrs(MG_List[MG_ListView], LISTBROWSER_Labels, ~0, TAG_DONE);

					if (CurrentState.ADTMode!=sn->sn_ADT) {
						CurrentState.ADTMode=sn->sn_ADT;
						ChangeAmiFTPMode();
					}

					AddCacheEntry(head, CurrentState.CurrentRemoteDir);
					FileList=head;

							SetAttrs(MG_List[MG_ListView],
							         LISTBROWSER_Labels, FileList,
							         //LISTBROWSER_AutoFit, TRUE,
							         //LISTBROWSER_ColumnInfo, &columninfo,
							         LISTBROWSER_ColumnInfo, columninfo,
LISTBROWSER_ColumnTitles,TRUE,
LISTBROWSER_TitleClickable,TRUE,
LISTBROWSER_SortColumn, COL_NAME,
							         LISTBROWSER_Striping, TRUE,
							         LISTBROWSER_MakeVisible, 0,
							         LISTBROWSER_Selected, -1,
							        TAG_DONE);
//DBUG("LBM_SORT 0x%08lx\n",MainWindow);
if(MainWindow) {
SetLBColumnInfoAttrs(columninfo, // restore "normal" ftp mode columns
                     LBCIA_Column,COL_DATE, LBCIA_Sortable,TRUE,
                                            LBCIA_Title,GetAmiFTPString(MW_COL_DATETIME),
                     LBCIA_Column,COL_OWN, LBCIA_Title,GetAmiFTPString(MW_COL_OWNER),
                     LBCIA_Column,COL_GRP, LBCIA_Title,GetAmiFTPString(MW_COL_GROUP),
                    TAG_DONE);
DoGadgetMethod((struct Gadget*)MG_List[MG_ListView], MainWindow, NULL,
               LBM_SORT, NULL, COL_NAME, LBMSORT_FORWARD, NULL);
RefreshGadgets((struct Gadget*)MG_List[MG_ListView], MainWindow, NULL);
//	RefreshGList((struct Gadget*)MG_List[MG_ListView], MainWindow, NULL, 1);
}
/*
					if (MainWindow)
						if (SetGadgetAttrs((struct Gadget*)MG_List[MG_ListView], MainWindow, NULL,
						                   LISTBROWSER_Labels, FileList,
						                   //LISTBROWSER_AutoFit, TRUE,
						                   //LISTBROWSER_ColumnInfo, &columninfo,
						                   LISTBROWSER_ColumnInfo, columninfo,
LISTBROWSER_ColumnTitles,TRUE,
LISTBROWSER_TitleClickable,TRUE,
LISTBROWSER_SortColumn, COL_NAME,
						                   LISTBROWSER_Striping, TRUE,
						                   LISTBROWSER_MakeVisible, 0,
						                   LISTBROWSER_Selected, -1,
						                  TAG_DONE))
							RefreshGList((struct Gadget*)MG_List[MG_ListView], MainWindow, NULL, 1);
						else
							SetAttrs(MG_List[MG_ListView],
							         LISTBROWSER_Labels, FileList,
							         //LISTBROWSER_AutoFit, TRUE,
							         //LISTBROWSER_ColumnInfo, &columninfo,
							         LISTBROWSER_ColumnInfo, columninfo,
LISTBROWSER_ColumnTitles,TRUE,
LISTBROWSER_TitleClickable,TRUE,
LISTBROWSER_SortColumn, COL_NAME,
							         LISTBROWSER_Striping, TRUE,
							         LISTBROWSER_MakeVisible, 0,
							         LISTBROWSER_Selected, -1,
							        TAG_DONE);
*/
					Continue=FALSE;
				}
				else Continue=TRUE;
DBUG("Continue = %ld\n",Continue);
		}
		else Continue=FALSE;

		if (sn->sn_LocalDir) UpdateLocalDir(sn->sn_LocalDir);
		else if (MainPrefs.mp_LocalDir) UpdateLocalDir(MainPrefs.mp_LocalDir);

		UpdateMainButtons(MB_NONESELECTED);
	}

	if (result==CONN_ABORTED) {
		CloseConnectWindow();
		UnlockWindow(MainWin_Object);
		//WakeWindow(MainWindow,lw);
		return CONN_ERROR;
	}

	if (Continue) {
		char *text=NULL;

		if (timedout) text=GetAmiFTPString(Str_ConnectionTimedOut);

		if (text && ConnectWindow)
			if (SetGadgetAttrs((struct Gadget*)CG_List[CG_Status], ConnectWindow, NULL,
			                   GA_Text, text, TAG_DONE))
				RefreshGList((struct Gadget*)CG_List[CG_Status], ConnectWindow, NULL, 1);

		retcode=CONN_ERROR;
	}
	else retcode=CONN_OK;

	if (ConnectWindow) {
		if (SetGadgetAttrs((struct Gadget*)CG_List[CG_Abort], ConnectWindow, NULL,
		                   GA_Disabled, TRUE, TAG_DONE))
			RefreshGList((struct Gadget*)CG_List[CG_Abort], ConnectWindow, NULL, 1);

		done = Continue? FALSE : TRUE;
		GetAttr(WINDOW_SigMask, ConnectWin_Object, &signal);
		GetAttr(WINDOW_SigMask, MainWin_Object, &mainwinsignal);

		if (!SilentMode) {
			while (!done) {
				wmask=Wait(signal|mainwinsignal|AG_Signal);
				if (wmask&AG_Signal)     HandleAmigaGuide();
				if (wmask&signal)        done=HandleConnectIDCMP();
				if (wmask&mainwinsignal) HandleMainWindowIDCMP(FALSE);
			}
		}
		CloseConnectWindow();
		UpdateMainButtons(connected?MB_NONESELECTED:MB_DISCONNECTED);
	}

	if (retcode==CONN_OK && CurrentState.ADTMode && MainPrefs.mp_ShowMOTD) {
		if (MOTDDate>MainPrefs.mp_LastAMOTD) {
			struct List flist;
			struct Node *node;
			struct dirlist file;

			NewList(&flist);
			memset(&file, 0, sizeof (file));
			file.name="info/adt/aminet-motd";
			file.size=0;
			file.mode=S_IFREG;
			if (node=AllocListBrowserNode(1,
			                              LBNA_Selected, TRUE,
			                              LBNA_Column, 0,
			                               LBNCA_Text, file.name,
			                             TAG_DONE)) {
				node->ln_Name=(void *)&file;
				AddTail(&flist, node);
				if (DownloadFile(&flist, "T:", ASCII, 0)==TRANS_OK) {
					ViewFile("T:aminet-motd");
					MainPrefs.mp_LastAMOTD=MOTDDate;
					ConfigChanged=TRUE;
				}
				Remove(node);
				FreeListBrowserNode(node);
			}
		}
	}

	UnlockWindow(MainWin_Object);
	//WakeWindow(MainWindow,lw);

	return retcode;
}

ULONG HandleConnectIDCMP()
{
    ULONG result,done=FALSE;
    uint16 code;

    while ((result=IDoMethod(ConnectWin_Object, WM_HANDLEINPUT, &code))!=WMHI_LASTMSG) {
	switch (result & WMHI_CLASSMASK) {
	  case WMHI_CLOSEWINDOW:
	    done=TRUE;
	    break;
	  case WMHI_GADGETUP:
	    done=TRUE;
	    break;
	  case WMHI_RAWKEY:
	    if (code==95)
	      SendAGMessage(AG_CONNECTWIN);
	    break;
	}
    }
    return done;
}

struct Window *OpenConnectWindow()
{
    //struct LayoutLimits limits;

    if (ConnectWindow)
      return ConnectWindow;

    ConnectLayout=NewObject(LayoutClass, NULL,//LayoutObject,
                     GA_DrawInfo, DrawInfo,
                     GA_TextAttr, AmiFTPAttrF,
                     LAYOUT_DeferLayout, TRUE,
                     LAYOUT_SpaceOuter, TRUE,
                     LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
                     LAYOUT_HorizAlignment, LALIGN_CENTRE,

                     LAYOUT_AddChild, CG_List[CG_Host]=NewObject(ButtonClass, NULL,//ButtonObject,
                       GA_ID, CG_Host,
                       GA_RelVerify, TRUE,
                       GA_ReadOnly, TRUE,
                       GA_Text, " ",
                       BUTTON_Justification, BCJ_LEFT,
                       TAG_DONE),
                       CHILD_MinWidth, PropFont->tf_XSize*30,
                       Label(GetAmiFTPString(CW_Site)),

                     LAYOUT_AddChild, CG_List[CG_Status]=NewObject(ButtonClass, NULL,//ButtonObject,
                       GA_ID, CG_Status,
                       GA_RelVerify, TRUE,
                       GA_ReadOnly, TRUE,
                       GA_Text, " ",
                       BUTTON_Justification, BCJ_LEFT,
                       TAG_DONE),
                       CHILD_MinWidth, PropFont->tf_XSize*30,
                       Label(GetAmiFTPString(CW_Status)),

                     LAYOUT_AddChild, CG_List[CG_Abort]=NewObject(ButtonClass, NULL,//ButtonObject,
                       GA_ID, CG_Abort,
                       GA_RelVerify, TRUE,
                       GA_Text, GetAmiFTPString(CW_Abort),
                       TAG_DONE),
                       CHILD_WeightedWidth, 0,
                       //CHILD_NominalSize, TRUE,
                   TAG_DONE);

    if (!ConnectLayout)
      return NULL;

    //LayoutLimits((struct Gadget *)ConnectLayout, &limits, PropFont,Screen);
    //limits.MinWidth+=Screen->WBorLeft+Screen->WBorRight;
    //limits.MinHeight+=Screen->WBorTop+Screen->WBorBottom;

    ConnectWin_Object = NewObject(WindowClass, NULL,//WindowObject,
                          WA_Title,        GetAmiFTPString(CW_WinTitle),
                          WA_PubScreen,    Screen,
                          WA_DepthGadget,  TRUE,
                          WA_DragBar,      TRUE,
                          WA_CloseGadget,  TRUE,
                          WA_Activate,     TRUE,
                          WA_SmartRefresh, TRUE,
                          //WA_Top, MainWindow->TopEdge+(MainWindow->Height-limits.MinHeight)/2,
                          //WA_Left, MainWindow->LeftEdge+(MainWindow->Width-limits.MinWidth)/2,
                          WA_IDCMP, IDCMP_RAWKEY,
                          WINDOW_Position,  WPOS_CENTERWINDOW,
                          WINDOW_RefWindow, MainWindow,
                          WINDOW_Layout,    ConnectLayout,
                        TAG_DONE);

    if (!ConnectWin_Object)
      return NULL;


    if (ConnectWindow=(struct Window *)IDoMethod(ConnectWin_Object, WM_OPEN)) {
	return ConnectWindow;
    }
    DisposeObject(ConnectLayout);
    ConnectLayout=NULL;

    return NULL;
}

void CloseConnectWindow(void)
{
    if (ConnectWin_Object) {
	DisposeObject(ConnectWin_Object);
	ConnectWindow=NULL;
	ConnectWin_Object=NULL;
	ConnectLayout=NULL;
    }
}

void PrintConnectStatus(char *text)
{
    char *s=text;
    
    if (CurrentState.Iconified)
      return;
    while(*s++)
      if (*s=='\n'||*s=='\r')
	*s=' ';
    
    if (ConnectWindow) {
	if (SetGadgetAttrs((struct Gadget*)CG_List[CG_Status], ConnectWindow, NULL,
			   GA_Text, text,
			   TAG_END))
	  RefreshGList((struct Gadget*)CG_List[CG_Status], ConnectWindow, NULL, 1);
    }
    else if (!SilentMode)
      ShowErrorReq(text);
}
