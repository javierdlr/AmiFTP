/* RCS Id: $Id: MainWindow.c 1.795 1996/09/28 13:32:58 lilja Exp $
   Locked version: $Revision: 1.795 $
*/

#include "AmiFTP.h"
#include "gui.h"
#include "MenuClass.h"
#include "AmiFTP_rev.h"

struct Window *MainWindow;
struct Screen *Screen;
struct DrawInfo *DrawInfo;
#ifndef _MENUCLASS_
struct VisualInfo *VisualInfo;
#endif
static struct TextFont *ScreenFont;
static struct TextFont *AmiFTPFont;
static struct TextFont *ListViewFont;
struct TextAttr AmiFTPAttr = {NULL, 0, FS_NORMAL, FPF_PROPORTIONAL};
struct TextAttr ListViewAttr = {NULL, 0, FS_NORMAL, FPF_PROPORTIONAL};
struct TextAttr *AmiFTPAttrF;
struct TextAttr *ListViewAttrF;
struct TextFont *PropFont;
struct TextFont *LBFont;

static ULONG lsel=-1;
//static Object *pagelayout;
extern struct List clist;

int prev_state=-1;

Object *MainWin_Object;
Object *MainWindowLayout;

#ifdef _MENUCLASS_
extern Object *menustripobj, *hotlistmenu;
extern int32 mimgsize;
extern int HandleMenus(int32 mitem);
void SetupLocaleStrings(){}; // used by "old" menu in main.c
#else
extern struct Menu *menu;
#endif

Object *MG_List[NumGadgets_main];
extern struct MsgPort *AppPort; /* Move to .h */

static UBYTE sitenamebuffer[100], remotedirbuffer[100];//, localdirbuffer[100];
struct ColumnInfo *columninfo;
/*struct ColumnInfo columninfo[]={
    {0, NULL, 0},
    {0, NULL, 0},
    {0, NULL, 0},
    {0, NULL, 0},
    {0, NULL, 0},
    {0, NULL, 0},
    {-1, (STRPTR)~0, -1}
};*/

/*struct ColumnInfo dummycolumninfo[]={
    {0, NULL, 0},
    {-1, (STRPTR)~0, -1}
};*/

static int32 compare_func(struct Hook *hook, APTR obj, struct LBSortMsg *msg)
{
	int32 res = 0;
//DBUG("compare_func() col:%ld 0x%08lx\n",msg->lbsm_Column,msg->lbsm_Direction);

	if(msg->lbsm_Column == COL_SIZE) {
//DBUG("  Size column (COL_SIZE=%ld)\n",COL_SIZE);
		if(msg->lbsm_TypeA==1  &&  msg->lbsm_TypeB==1) { // 1:LBNCA_Text
			struct dirlist *dl_A = (struct dirlist*)msg->lbsm_UserDataA,
			               *dl_B = (struct dirlist*)msg->lbsm_UserDataB;
			uint32 sizeA = (uint32)dl_A->size,
			       sizeB = (uint32)dl_B->size;
//DBUG("  A:%ld  B:%ld (adt)\n",dl_A->adt,dl_B->adt);
			res = sizeA - sizeB;
//DBUG("  A:%ld  B:%ld (LBNA_UserData)\n",(uint32)dl_A->size,(uint32)dl_B->size);
//			res = (uint32)msg->lbsm_UserDataA - (uint32)msg->lbsm_UserDataB;
			if(res == 0) { return 0; } // equal
			if(res > 0) { return 1; }  // A > B
			return -1;                 // A < B
		}
	}

	if(msg->lbsm_Column == COL_DATE) {
//DBUG("  Date column (COL_DATE=%ld)\n",COL_DATE);
		if(msg->lbsm_TypeA==1  &&  msg->lbsm_TypeB==1) { // 1:LBNCA_Text
			int32 timeA = datetotime((STRPTR)msg->lbsm_DataA.Text),
			      timeB = datetotime((STRPTR)msg->lbsm_DataB.Text);
//DBUG("  A:'%s'  B:'%s'\n",msg->lbsm_DataA.Text,msg->lbsm_DataB.Text);
			res = timeA - timeB;
			if(res == 0) { return 0; } // equal
			if(res > 0) { return 1; }  // A > B
			return -1;                 // A < B
		}
	}

//	if(res > 0) { return 1; }  // A > B
//	if(res < 0) { return -1; } // A < B

	return 0;
}

//extern char *wintitle;

static BOOL Upload=FALSE;
struct List DropUploadList;

static void ScrollListbrowser(ULONG direction);
void FreeInfoList(struct List *list);
void CreateInfoList(struct List *list);


ULONG HandleMainWindowIDCMP(const BOOL AllowIconify)
{
	ULONG result,done=FALSE;
	uint16 code;
	Upload=FALSE;

	while (MainWin_Object &&
	       //(result=CA_HandleInput(MainWin_Object,&code))!=WMHI_LASTMSG) {
	       (result=IDoMethod(MainWin_Object, WM_HANDLEINPUT, &code))!=WMHI_LASTMSG) {
		switch (result & WMHI_CLASSMASK) {
			case WMHI_CLOSEWINDOW:
				done=TRUE;
			break;
			case WMHI_GADGETUP:
				switch (result & WMHI_GADGETMASK) {
					case MG_ListView:
					{
						struct Node *node;
						ULONG attr;
//DBUG("MG_ListView\n",NULL);
						if (FileList) {
							for (node=GetHead(FileList);node;node=GetSucc(node)) {
								GetListBrowserNodeAttrs(node, LBNA_Selected,&attr, TAG_DONE);
								if (attr) break;
							}

							if (node) { UpdateMainButtons(MB_FILESELECTED); }
							else { UpdateMainButtons(MB_NONESELECTED); }

							if (code!=-1) {
								ULONG attr=0;
								ULONG action=0;
								GetAttrs((struct Gadget*)MG_List[MG_ListView],
								         LISTBROWSER_NumSelected, &attr,
								         LISTBROWSER_RelEvent, &action,
								         LISTBROWSER_SelectedNode, &node,
								        TAG_DONE);
								if (attr&&node) {
									if (action&LBRE_DOUBLECLICK && lsel==code) {
										struct dirlist *curr=(void *)node->ln_Name;
										struct List *head;

										LockWindow(MainWin_Object);
										//APTR lw = SleepWindow(MainWindow);

										if (curr->mode&0x4000) {
											if (!change_remote_dir(curr->name, 0)) {
												if ( (head=LookupCache(CurrentState.CurrentRemoteDir)) ) {
													DetachToolList();
													FileList=head;
													AttachToolList(TRUE);
													UpdateMainButtons(MB_NONESELECTED);
												}
												else if ( (head=read_remote_dir()) ) {
													DetachToolList();
													AddCacheEntry(head,CurrentState.CurrentRemoteDir);
													FileList=head;
													AttachToolList(TRUE);
													UpdateMainButtons(MB_NONESELECTED);
												}
												else ShowErrorReq(Str_ErrorReadingDir);
											}
											else { RemoteCDFailed(); }
										}
										else if (S_ISLNK(curr->mode)) {
											char *name;
											struct List *head;
											name = linkname(curr->name);
											if (name) {
												if (change_remote_dir(name,0)==ENOTDIR) {
													DownloadFile(FileList,NULL,TransferMode,0);
												}
												else {
													if ( (head=LookupCache(CurrentState.CurrentRemoteDir)) ) {
														DetachToolList();
														FileList=head;
														AttachToolList(TRUE);
														UpdateMainButtons(MB_NONESELECTED);
													}
													else if ( (head=read_remote_dir()) ) {
														DetachToolList();
														AddCacheEntry(head,CurrentState.CurrentRemoteDir);
														FileList=head;
														AttachToolList(TRUE);
														UpdateMainButtons(MB_NONESELECTED);
													}
													else ShowErrorReq(GetAmiFTPString(Str_ErrorReadingDir));
												}
												free(name);
											}
										}
										else { DownloadFile(FileList, NULL, TransferMode, 0); }

										UnlockWindow(MainWin_Object);
										//WakeWindow(MainWindow,lw);
									}
								}
								lsel=code;
							}
						}
					}
					break;
					case MG_CacheList:
					{
						struct Node *node;
						struct List *head;
						//int i;
						char *dir;

						GetAttr(CHOOSER_SelectedNode,MG_List[MG_CacheList],(ULONG*) &node);

						if (node) {
							GetChooserNodeAttrs(node, CNA_Text, &dir, TAG_DONE);

							if (!change_remote_dir(dir, 0)) {
								if ( (head=LookupCache(CurrentState.CurrentRemoteDir)) ) {
									DetachToolList();
									FileList=head;
									AttachToolList(TRUE);
									UpdateMainButtons(MB_NONESELECTED);
								}
							}
							else { RemoteCDFailed(); }
						}
					}
					break;
					case MG_SpeedBar:
						if (!HandleSpeedBar(code)) done=TRUE;
					break;
					case MG_DLGetFile:
						DLPath_clicked();
					break;
					case MG_DirName:
						Dir_clicked();
					break;
					case MG_SiteName:
						Site_clicked();
					break;
					default: break;
				}
				break;

				case WMHI_MENUPICK: {
//DBUG("WMHI_MENUPICK 0x%08lx\n",code);
#ifdef _MENUCLASS_
					uint32 item = NO_MENU_ID;
					while( (item=IDoMethod(menustripobj, MM_NEXTSELECT, 0, item)) != NO_MENU_ID ) {
//DBUG("\t0x%08lx\n",item);
						int ret = HandleMenus(item);
						if(ret == 17) break;
						else if(ret == 0) done = TRUE;
					}
#else
					USHORT menunum=result&WMHI_MENUMASK;
					struct CallBackHook *cbh;
					struct Window *win = MainWindow;
					while (MainWindow==win && menunum!=MENUNULL) {
						struct MenuItem *menuitem = ItemAddress(menu, menunum);
							cbh=(void *)GTMENUITEM_USERDATA(menuitem);
							if ((ULONG)cbh > 100) {
								if (cbh->cbh_func) {
									int ret = cbh->cbh_func(menuitem);
										if (ret == 17) break;
										else if (ret == 0) done = TRUE;
								}
							}
							else {// Hotlist item se1lected
								int hnum=(int)cbh,i;
								struct SiteNode *ptr=NULL;
								struct Node *lbn;
								hnum--;
								for (i=0,lbn=GetHead(&SiteList);lbn;lbn=GetSucc(lbn)) {
									GetListBrowserNodeAttrs(lbn, LBNA_UserData,&ptr, TAG_DONE);
									if (ptr) {
										if (ptr->sn_HotList) {
											if (i==hnum) break;
											i++;
										}
									}
								}
								if (ptr && (i==hnum) && ptr->sn_MenuType!=SLN_PARENT && !ptr->sn_BarLabel) {
									ConnectSite(ptr,0);
									if (connected)
										strncpy(CurrentState.LastLVSite,ptr->sn_Node.ln_Name,sizeof(CurrentState.LastLVSite));//60);
								}
							}
							menunum=menuitem->NextSelect;
						}
#endif
				}
				break;

				/*case WMHI_MENUHELP: {
					struct CallBackHook *cbh;
					struct MenuItem *menuitem=ItemAddress(menu,result&WMHI_MENUMASK);
					if (menuitem) {
						cbh=(void *)GTMENUITEM_USERDATA(menuitem);
						if (cbh) {
							if ((LONG)cbh>100) SendAGMessage(cbh->cbh_aguide);
							else SendAGMessage(AG_MENUHOTLIST);
						}
					}
				}
				break;*/

				case WMHI_ICONIFY:
					if (AllowIconify) {
						IDoMethod(MainWin_Object, WM_ICONIFY);
						//if (CA_Iconify(MainWin_Object))
						MainWindow=NULL;
					}
				break;
				case WMHI_UNICONIFY:
					MainWindow=(struct Window *)IDoMethod(MainWin_Object, WM_OPEN);
					//CA_OpenWindow(MainWin_Object);
				break;

				/*case WMHI_RAWKEY:
DBUG("key: %ld\n", code);
				break;*/
		}

DBUG("MenuNeedsUpdate %ld\n",MenuNeedsUpdate);
		/*if (MenuNeedsUpdate) {
			UpdateMenus();
			MenuNeedsUpdate=FALSE;
		}*/
	}

	if (Upload) {
		//struct Node *node;
		LockWindow(MainWin_Object);
		UploadFile(&DropUploadList, NULL, TransferMode);
		Upload=FALSE;
		free_dirlist(&DropUploadList);
		UnlockWindow(MainWin_Object);
		Dir_clicked();
	}

	return done;
}

struct List dummy_list;
static struct Hook MainIDCMPHook;

static ULONG MainIDCMPHookFunc(struct Hook *hook,
                               Object *WinObj,
                               struct IntuiMessage *msg)
{
    switch (msg->Class) {
      case IDCMP_RAWKEY:
	switch (msg->Code) {
	  case 95:
	    SendAGMessage(AG_MAINWIN);
	    break;
	  case CURSORUP:
	    if (msg->Qualifier & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT))
	      ScrollListbrowser(LBP_PAGEUP);
	    else if (msg->Qualifier & (IEQUALIFIER_LALT|IEQUALIFIER_RALT))
	      ScrollListbrowser(LBP_TOP);
	    else
	      ScrollListbrowser(LBP_LINEUP);
	    break;
	  case CURSORDOWN:
	    if (msg->Qualifier & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT))
	      ScrollListbrowser(LBP_PAGEDOWN);
	    else if (msg->Qualifier & (IEQUALIFIER_LALT|IEQUALIFIER_RALT))
	      ScrollListbrowser(LBP_BOTTOM);
	    else
	      ScrollListbrowser(LBP_LINEDOWN);
	    break;
	}
	break;
    }
    return 0;
}


void processFile(char *remotePath, char *localPath, char* fileName, int64 fileSize)
{
    struct dirlist *entry;
	struct Node *node;

    static char slocalPath[1024];
    static char sremotePath[1024];

    sprintf(slocalPath, "%s%s", localPath, fileName);
    sprintf(sremotePath, "%s%s", remotePath, fileName);

    if ( (entry=new_direntry(sremotePath, slocalPath, NULL, NULL, NULL, S_IFREG, fileSize)) )
    {
        if ( (node=AllocListBrowserNode(1,
					  LBNA_UserData, entry,
					  LBNA_Column, 0,
					  LBNCA_Text, entry->name,
					  LBNA_Selected, TRUE,
					  TAG_DONE)) )
		{
			node->ln_Name=(void *)entry;
			AddTail(&DropUploadList, node);
			Upload=TRUE;
		}
    	else
      		free_direntry(entry);

	}

}

void processDirectory(char *localPath, char *remotePath)
{
     APTR context = ObtainDirContextTags(EX_StringNameInput,(char*)localPath,
                    EX_DataFields,(EXF_NAME|EXF_TYPE|EXF_SIZE),TAG_END);

        if (context)
    	{
        	struct ExamineData *dat;

			while ((dat = ExamineDir(context)))
        	{
                if( EXD_IS_FILE(dat) )
                {
                    char *currentRemotePath = calloc(strlen(remotePath)+2,1);
                    strcpy(currentRemotePath, remotePath);
                    char *currentLocalPath = calloc(strlen(localPath)+2,1);
                    strcpy(currentLocalPath, localPath);

                    size_t remoteLen = strlen(currentRemotePath);
                    size_t localLen = strlen(currentLocalPath);
                    int i = 0;
                    for (i = 0; i < remoteLen; i++)
                    {
                        if (currentRemotePath[i]==':') currentRemotePath[i]='/';
                        }

                    if (currentRemotePath[remoteLen-1]!='/')
                    {
                    	currentRemotePath[remoteLen]='/';
                    	currentRemotePath[remoteLen+1]='\0';
                	}
                    if (currentLocalPath[localLen-1]!='/' && currentLocalPath[localLen-1]!=':')
                    {
                    	currentLocalPath[localLen]='/';
                    	currentLocalPath[localLen+1]='\0';
                	}
                    processFile(currentRemotePath, currentLocalPath, dat->Name, dat->FileSize);
                    free(currentRemotePath);
                    free(currentLocalPath);
            	}
                else if( EXD_IS_DIRECTORY(dat) )
                {
                    int localLen = strlen(localPath)+strlen(dat->Name)+2;
                    char *local = (char*)calloc(localLen,1);
                    if (local)
                    {
                        strcpy(local, localPath);
                        int remoteLen = strlen(remotePath)+strlen(dat->Name)+2;
                        char *remote = (char*)calloc(remoteLen,1);

                        if (remote)
                        {
                            strcpy(remote, remotePath);

                            if (AddPart(remote, dat->Name, remoteLen) && AddPart(local, dat->Name, localLen))
                            {
                        		processDirectory(local, remote);
                        	}
                        	free(remote);
                        }
                        free(local);
                    }
                }
            }

        ReleaseDirContext(context);
		}
}


static struct Hook AppMessageHook; // dragndrop
static ULONG AppMessageHookFunc(struct Hook *hook, Object *WinObj, struct AppMessage *msg)
{
    int i;
    struct Node *node;
    //extern Object *AboutWin_Object;
    extern Object *EditSiteWin_Object;
    extern Object *TransferWin_Object;
    extern Object *SiteListWin_Object;
    extern Object *MainPrefsWin_Object;

    if (!connected || TransferWin_Object || /*AboutWin_Object ||*/ EditSiteWin_Object ||
	SiteListWin_Object || MainPrefsWin_Object)
      return 0;

    while ( (node=RemHead(&DropUploadList)) ) {
	free(node->ln_Name);
	FreeVec(node);
    }

    for (i=0; i<msg->am_NumArgs; i++) {

    if (msg->am_ArgList[i].wa_Lock && msg->am_ArgList[i].wa_Name) {
	    char path[1024];

	    NameFromLock(msg->am_ArgList[i].wa_Lock, path, 1024);
	    AddPart(path, msg->am_ArgList[i].wa_Name, 1024);

        struct ExamineData *data = ExamineObjectTags(EX_StringNameInput, path, TAG_END);
        if (data)
        {
            if (EXD_IS_FILE(data))
            {
				processFile(data->Name, path,"", data->FileSize);
            }
            else if (EXD_IS_DIRECTORY(data))
            {
                STRPTR remotePath = ASPrintf("%s/", data->Name);
            	processDirectory(path, remotePath);
                FreeVec(remotePath);
            }
            FreeDosObject(DOS_EXAMINEDATA,(APTR)data);
        }    
    }
    }

    return 0;
}

//struct RastPort *ARPort,rastport;
extern struct List SpeedBarList;

int filePen = 1;
int drawerPen = 2;
struct Window *OpenFTPWindow(const BOOL StartIconified)
{
	Object *g1,*g2;//, *g3;//, *but1, *but2, *buttonlayout;
	struct DiskObject *iconify = NULL;

	Screen=LockPubScreen(MainPrefs.mp_OpenOnDefaultScreen? NULL : MainPrefs.mp_PubScreen);
	if (!Screen) {
		//char pubname[256];
		Screen=LockPubScreen(NULL);
/*		GetDefaultPubScreen(pubname);
		if (MainPrefs.mp_PubScreen) free(MainPrefs.mp_PubScreen);
		MainPrefs.mp_PubScreen=strdup(pubname);*/
	}

	struct DrawInfo *drawInfo = GetScreenDrawInfo(Screen);
	if (drawInfo)
	{
		filePen = drawInfo->dri_Pens[TEXTPEN];
		drawerPen = drawInfo->dri_Pens[HIGHLIGHTTEXTPEN];
		FreeScreenDrawInfo(Screen, drawInfo);
	}
	ScreenFont=OpenFont(Screen->Font);

	AmiFTPAttr.ta_Name=Screen->Font->ta_Name;
	AmiFTPAttr.ta_YSize=Screen->Font->ta_YSize;
	ListViewAttr.ta_Name=Screen->Font->ta_Name;
	ListViewAttr.ta_YSize=Screen->Font->ta_YSize;

	if (!MainPrefs.mp_UseDefaultFonts) {
		if (MainPrefs.mp_FontName && MainPrefs.mp_FontSize > 0) {
			AmiFTPAttr.ta_Name=MainPrefs.mp_FontName;
			AmiFTPAttr.ta_YSize=MainPrefs.mp_FontSize;
		}
		if (MainPrefs.mp_ListFontName && MainPrefs.mp_ListFontSize > 0) {
			ListViewAttr.ta_Name=MainPrefs.mp_ListFontName;
			ListViewAttr.ta_YSize=MainPrefs.mp_ListFontSize;
		}
	}

	if ( (AmiFTPFont=OpenDiskFont(&AmiFTPAttr)) ) {
		AmiFTPAttrF=&AmiFTPAttr;
		PropFont=AmiFTPFont;
	}
	else {
		AmiFTPAttrF=Screen->Font;
		PropFont=ScreenFont;
	}

	if ( (ListViewFont=OpenDiskFont(&ListViewAttr)) ) {
		ListViewAttrF=&ListViewAttr;
		LBFont=ListViewFont;
	}
	else {
		ListViewAttrF=AmiFTPAttrF;
		LBFont=PropFont;
	}


struct TextAttr SBFontF;
struct TextFont *SBFont = NULL;
memcpy(&SBFontF, AmiFTPAttrF, sizeof(SBFontF));
SBFontF.ta_YSize -= 2;
//printf("'%s' %d\n",SBFontF.ta_Name,SBFontF.ta_YSize);
SBFont = OpenDiskFont(&SBFontF);
//printf("'%s' %ld\n",SBFont->tf_Message.mn_Node.ln_Name,SBFont->tf_YSize);

struct Hook *compare_hook = AllocSysObjectTags(ASOT_HOOK,
                                               ASOHOOK_Entry, compare_func,
                                              TAG_DONE);

columninfo = AllocLBColumnInfo(TOT_COLS,
	LBCIA_Column,COL_NAME, LBCIA_Title,GetAmiFTPString(MW_COL_NAME),//"Name",
	                       LBCIA_AutoSort,TRUE, LBCIA_SortArrow,TRUE,
	LBCIA_Column,COL_SIZE, LBCIA_Title,GetAmiFTPString(MW_COL_SIZE),//"Size",
	                       LBCIA_AutoSort,TRUE, LBCIA_SortArrow,TRUE, LBCIA_CompareHook, compare_hook,
	LBCIA_Column,COL_TYPE, LBCIA_Title,GetAmiFTPString(MW_COL_TYPE),//"Type",
	                       LBCIA_AutoSort,TRUE, LBCIA_SortArrow,TRUE, LBCIA_Sortable,TRUE,
	LBCIA_Column,COL_DATE, LBCIA_Title,GetAmiFTPString(MW_COL_DATETIME),//"Date/Time",
	                      LBCIA_AutoSort,TRUE, LBCIA_SortArrow,TRUE, LBCIA_CompareHook, compare_hook,
	LBCIA_Column,COL_OWN, LBCIA_Title,GetAmiFTPString(MW_COL_OWNER),//"Owner",
	LBCIA_Column,COL_GRP, LBCIA_Title,GetAmiFTPString(MW_COL_GROUP),//"Group",
TAG_DONE);


	//InitRastPort(&rastport);
	//SetFont(&rastport, PropFont);
	//ARPort=&rastport;
	//TextLength(&rastport, "0", 1);
	DrawInfo=GetScreenDrawInfo(Screen);
#ifndef _MENUCLASS_
	VisualInfo=GetVisualInfo(Screen, TAG_DONE);
#endif

	InitSpeedBarList();

	CreateInfoList(&dummy_list);

	lsel=-1;
	MainWindowLayout=NewObject(LayoutClass, NULL,//LayoutObject,
	               //LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	               GA_DrawInfo, DrawInfo,
	               GA_TextAttr, AmiFTPAttrF,
	               //LAYOUT_DeferLayout, TRUE,//FALSE,
	               LAYOUT_SpaceOuter,     TRUE,
	               LAYOUT_HorizAlignment, LALIGN_RIGHT,
	               LAYOUT_Orientation,    LAYOUT_ORIENT_VERT,

	               LAYOUT_AddChild, g1=NewObject(LayoutClass, NULL,//LayoutObject,
	               LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	                   LAYOUT_AddChild, MG_List[MG_SiteName]=NewObject(StringClass, NULL,//StringObject,
	                   GA_ID,        MG_SiteName,
	                   GA_RelVerify, TRUE,
	                   STRINGA_Buffer,   sitenamebuffer,
	                   STRINGA_MaxChars, 99,
	                   TAG_DONE),
	                   Label(GetAmiFTPString(MW_SiteName)),
	               TAG_DONE),
	               CHILD_WeightedHeight,0,

	               LAYOUT_AddChild, NewObject(LayoutClass, NULL,//StartHGroup,
	               //LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	               LAYOUT_InnerSpacing, FALSE,//Spacing(FALSE),
	                   LAYOUT_AddChild, g2=NewObject(LayoutClass, NULL,//VGroupObject,
	                   LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	                       LAYOUT_AddChild, MG_List[MG_DirName]=NewObject(StringClass, NULL,//StringObject,
	                       GA_ID,        MG_DirName,
	                       GA_RelVerify, TRUE,
	                       GA_Disabled,  TRUE,
	                       STRINGA_Buffer,   remotedirbuffer,
	                       STRINGA_MaxChars, 120,
	                       TAG_DONE),
	                       Label(GetAmiFTPString(MW_DirName)),
	                   TAG_DONE),

	                   LAYOUT_AddChild, MG_List[MG_CacheList]=NewObject(ChooserClass, NULL,//ChooserObject,
	                   GA_ID,        MG_CacheList,
	                   GA_RelVerify, TRUE,
	                   GA_Disabled,  TRUE,
	                   CHOOSER_Labels,   &clist,
	                   CHOOSER_AutoFit,  TRUE,
	                   CHOOSER_DropDown, TRUE,
	                   TAG_DONE),
	                   CHILD_WeightedWidth, 0,
	               TAG_DONE),
	               CHILD_WeightedHeight, 0,

	               LAYOUT_AddChild, MG_List[MG_SpeedBarGroup]=NewObject(LayoutClass, NULL,//LayoutObject,
	               LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	               LAYOUT_SpaceInner,  FALSE,
	                   LAYOUT_AddChild, MG_List[MG_SpeedBar]=NewObject(SpeedBarClass, NULL,//SpeedBarObject,
	                   GA_ID,        MG_SpeedBar,
	                   GA_RelVerify, TRUE,
	                   SPEEDBAR_EvenSize,   TRUE,
	                   SPEEDBAR_Buttons,    &SpeedBarList,
	                   SPEEDBAR_BevelStyle, BVS_NONE,
	                   SPEEDBAR_ButtonType, MainPrefs.mp_ShowToolBar? MainPrefs.mp_ShowToolBar-1 : TAG_IGNORE,
	                   SPEEDBAR_Font,       SBFont,
                    TAG_DONE),
                    CHILD_WeightedHeight, 0,
	               TAG_DONE),
	               CHILD_WeightedHeight, 0,

	               LAYOUT_AddChild, NewObject(LayoutClass, NULL,//StartVGroup,
	               LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	               //LAYOUT_BackFill,    LAYERS_BACKFILL,
	                   LAYOUT_AddChild, MG_List[MG_ListView]=NewObject(ListBrowserClass, NULL,//ListBrowserObject,
	                   GA_ID,        MG_ListView,
	                   GA_RelVerify, TRUE,
	                   GA_TextAttr,  ListViewAttrF,
	                   LISTBROWSER_Labels,       (ULONG)FileList? FileList:&dummy_list,
	                   LISTBROWSER_MultiSelect,  TRUE,
	                   LISTBROWSER_ShowSelected, TRUE,
	                   LISTBROWSER_ColumnInfo,   (ULONG)(FileList? columninfo:NULL),//dummycolumninfo),
LISTBROWSER_ColumnTitles,TRUE,
LISTBROWSER_TitleClickable,TRUE,
LISTBROWSER_SortColumn, COL_NAME,
	                   //LISTBROWSER_MinVisible, 10,
	                   //LISTBROWSER_Separators, FALSE,
	                   //LISTBROWSER_AutoFit, TRUE,
	                   //LISTBROWSER_Striping, TRUE,
	                   //LISTBROWSER_HorizontalProp, TRUE,
	                   TAG_DONE),
	                   CHILD_MinHeight, LBFont->tf_YSize*6,
	                   //CHILD_WeightedHeight,0,
	               TAG_DONE),

//	               LAYOUT_AddChild, NewObject(LayoutClass, NULL,//VLayoutObject,
	               LAYOUT_AddChild, MG_List[MG_LogWinGroup] = NewObject(LayoutClass, NULL,//VLayoutObject,
                LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	               //LAYOUT_EvenSize,    TRUE,
//	                     LAYOUT_AddChild, NewObject(LayoutClass, NULL,//StartHGroup, //StartHGroup, Spacing(FALSE),
	                     //LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	                         LAYOUT_AddChild, MG_List[MG_DLGetFile] = NewObject(GetFileClass, NULL,//GetFileObject,
	                         GA_ID,        MG_DLGetFile,
	                         GA_RelVerify, TRUE,
	                         GETFILE_DrawersOnly, TRUE,
	                         GETFILE_TitleText,   GetAmiFTPString(Str_SelectDLPath),
	                         GETFILE_Drawer,      CurrentState.CurrentDLDir,
	                         GETFILE_ReadOnly,    TRUE,
	                         //GA_ID, MG_DLString,
	                         //GA_RelVerify, TRUE,
	                         // STRINGA_Buffer, localdirbuffer,
	                         //STRINGA_MaxChars, 80,
	                         TAG_DONE),
	                         CHILD_WeightedHeight, 0,
	                         Label(GetAmiFTPString(MW_DownloadDir)),
//	                     TAG_DONE),
//	                     CHILD_WeightedHeight, 0,
	               TAG_DONE),
	              CHILD_WeightedHeight, 0,

	TAG_DONE);
	if (!MainWindowLayout) {
//	Printf("Failed to create layout\n");
		return 0;
	}

	SetAttrs(g1, LAYOUT_AlignLabels, g2, TAG_DONE);
	SetAttrs(g2, LAYOUT_AlignLabels, g1, TAG_DONE);

	MainIDCMPHook.h_Entry    = (HOOKFUNC)MainIDCMPHookFunc;
	MainIDCMPHook.h_SubEntry = NULL;
	MainIDCMPHook.h_Data     = NULL;

	AppMessageHook.h_Entry    = (HOOKFUNC)AppMessageHookFunc;
	AppMessageHook.h_SubEntry = NULL;
	AppMessageHook.h_Data     = NULL;
/*
	if (!MainPrefs.mp_ShowToolBar) {
//	SetAttrs(MainWindowLayout, LAYOUT_RemoveChild, MG_List[MG_SpeedBar], TAG_DONE);
//	FreeSpeedBarList();
		SetAttrs(MG_List[MG_SpeedBar], SPEEDBAR_Buttons,~0, TAG_END);
		IDoMethod(MG_List[MG_SpeedBarGroup], OM_REMMEMBER, MG_List[MG_SpeedBar]);
	}
	else { SetAttrs(MG_List[MG_SpeedBar], SPEEDBAR_ButtonType,MainPrefs.mp_ShowToolBar-1, TAG_DONE); }
*/
	BOOL firstStart = CurrentState.Width==0&&MainPrefs.mp_Width==0&&CurrentState.Height==0&&MainPrefs.mp_Height==0;
	if (firstStart)
	{
		MainPrefs.mp_Width = 800;
		MainPrefs.mp_Height = 600;
	}
#ifdef _MENUCLASS_
	//Get MenuImageSize envvar
	char buffer[16];
	if(GetVar("MenuImageSize",buffer,sizeof(buffer),GVF_GLOBAL_ONLY) != -1)
	{
		//mimgsize = atol(buffer);
		StrToLong(buffer, &mimgsize);
	}

	menustripobj = BuildMenuClass(Screen);

#endif


// Reset icon X/Y positions so it iconifies properly on Workbench
if( (iconify=GetIconTags("PROGDIR:AMIFTP", ICONGETA_FailIfUnavailable,FALSE, TAG_END)) )
{
	iconify->do_CurrentX = NO_ICON_POSITION;
	iconify->do_CurrentY = NO_ICON_POSITION;
}


    MainWin_Object = NewObject(WindowClass, NULL,//WindowObject,
                       WA_Title,       "AmiFTP",//wintitle,
                       WA_ScreenTitle, VERS " ("DATE") � 2020 Frank Menzel <goos.entwickler-x.de>",//wintitle,
                       WA_PubScreen,   Screen,
                       WA_SizeGadget,  TRUE,
                       WA_SizeBBottom, TRUE,
                       firstStart? TAG_IGNORE:WA_Top, CurrentState.TopEdge? CurrentState.TopEdge:MainPrefs.mp_TopEdge-Screen->ViewPort.DyOffset,
                       firstStart? TAG_IGNORE:WA_Left, CurrentState.LeftEdge? CurrentState.LeftEdge:MainPrefs.mp_LeftEdge-Screen->ViewPort.DxOffset,
                       firstStart? WINDOW_Position:TAG_IGNORE, WPOS_CENTERSCREEN,
                       WA_InnerHeight, CurrentState.Height? CurrentState.Height:MainPrefs.mp_Height,
                       WA_InnerWidth,  CurrentState.Width? CurrentState.Width:MainPrefs.mp_Width,
                       WA_DepthGadget,  TRUE,
                       WA_DragBar,      TRUE,
                       WA_CloseGadget,  TRUE,
                       WA_Activate,     TRUE,
                       WA_SmartRefresh, TRUE,
                       WA_MenuHelp, TRUE,
                       WA_IDCMP,    IDCMP_MENUHELP,
#ifdef _MENUCLASS_
                       WA_MenuStrip, menustripobj,
#endif
                       WINDOW_IconifyGadget, TRUE,
                       WINDOW_IconTitle,     "AmiFTP",
                       //WINDOW_Icon,          GetDiskObject("PROGDIR:AMIFTP"),
                       WINDOW_Icon,          iconify,
                       WINDOW_AppPort,       AppPort,
                       WINDOW_AppWindow,  TRUE,
                       WINDOW_AppMsgHook, &AppMessageHook,
                       WINDOW_IDCMPHook,     &MainIDCMPHook,
                       WINDOW_IDCMPHookBits, IDCMP_RAWKEY,
                       WINDOW_UniqueID,    "AmiFTP_main",
                       WINDOW_PopupGadget, TRUE,

                       WINDOW_Layout,        MainWindowLayout,
                     TAG_DONE);

	if (!MainWin_Object) {
//	Printf("Failed to create WinObject\n");
		DisposeObject(MainWindowLayout);
		return 0;
	}

	UpdateMenus();
//updateMenuHotlist();

	{
		int state=prev_state==-1? MB_DISCONNECTED : prev_state;
		prev_state=-1;

		UpdateMainButtons(state);
	}

	if (StartIconified) {
		MainWindow=NULL;
		IDoMethod(MainWin_Object, WM_ICONIFY);
		return (struct Window*)1;
	}

	if ( (MainWindow=(struct Window *)IDoMethod(MainWin_Object, WM_OPEN)) ) {

		if (firstStart)
		{
			ULONG value;
			GetAttr(WA_Top, MainWin_Object, &value);
			MainPrefs.mp_TopEdge = (WORD)value;

			GetAttr(WA_Left, MainWin_Object, &value);
			MainPrefs.mp_LeftEdge = (WORD)value;
		}

		UpdateWindowTitle();
		UnlockPubScreen(NULL, Screen);

		extern BPTR LogWindow;
		if ((MainPrefs.mp_Log==0) != (LogWindow==0))
		{
			if (LogWindow) CloseLogWindow();
			else OpenLogWindow();

			//UpdateMenus();
		}

	return MainWindow;
	}

    UnlockPubScreen(NULL, Screen);
#ifdef _MENUCLASS_
    DisposeObject(menustripobj);
#endif
    DisposeObject(MainWin_Object);
    MainWin_Object=NULL;
    MainWindowLayout=NULL;
    CloseFont(ScreenFont);
    ScreenFont=NULL;

    FreeLBColumnInfo(columninfo);
FreeSysObject(ASOT_HOOK, compare_hook);
    if(SBFont) { CloseFont(SBFont); SBFont=NULL; }

    if (ListViewFont) CloseFont(ListViewFont);
    if (AmiFTPFont) CloseFont(AmiFTPFont);

    FreeInfoList(&dummy_list);
    ListViewFont=NULL;
    AmiFTPFont=NULL;

    return NULL;
}

void CloseMainWindow(void)
{
DBUG("CloseMainWindow()\n",NULL);
	if (MainWin_Object) {
/*		LONG w,h,l,t;

		GetAttrs(MainWin_Object,
		 WA_InnerWidth, &w, WA_InnerHeight, &h,
		 TAG_DONE);*/
		CurrentState.Width=MainWindow->Width-MainWindow->BorderLeft-MainWindow->BorderRight;
		CurrentState.Height=MainWindow->Height-MainWindow->BorderTop-MainWindow->BorderBottom;
		CurrentState.TopEdge=MainWindow->TopEdge;
		CurrentState.LeftEdge=MainWindow->LeftEdge;
/*	kprintf("%ld %ld %ld %ld\n", CurrentState.Width, CurrentState.Height,
		CurrentState.TopEdge, CurrentState.LeftEdge);*/
#ifdef _MENUCLASS_
		DisposeObject(menustripobj);
		menustripobj = NULL;
		hotlistmenu = NULL;
#endif
		CloseLogWindow();
		DisposeObject(MainWin_Object);
		MainWin_Object=NULL;
		MainWindowLayout=NULL;
		MainWindow=NULL;
	}

	if (ScreenFont) {
		CloseFont(ScreenFont);
		ScreenFont=NULL;
	}

	if (ListViewFont) {
		CloseFont(ListViewFont);
		ListViewFont=NULL;
	}

	if (AmiFTPFont) {
		CloseFont(AmiFTPFont);
		AmiFTPFont=NULL;
	}

#ifndef _MENUCLASS_
	if (VisualInfo) {
		FreeVisualInfo(VisualInfo);
		VisualInfo=NULL;
	}
#endif

	FreeSpeedBarList();
	FreeInfoList(&dummy_list);
}


void disableMenuItem(uint32 id, BOOL status)
{
	Object *obj = (Object *)IDoMethod(menustripobj, MM_FINDID, 0, id);
	SetAttrs(obj, MA_Disabled,status, TAG_DONE);
}

#define DisableGadget(gadget, disable) if (SetGadgetAttrs((struct Gadget*)gadget, MainWindow, NULL, GA_Disabled, disable, TAG_DONE) && MainWindow) RefreshGList((struct Gadget*)gadget, MainWindow, NULL, 1);

void UpdateMainButtons(const int state)
{
    UpdateWindowTitle();

    if (state == prev_state)
      return;

#ifdef _MENUCLASS_
    disableMenuItem(MNU_Disconnect, FALSE); // enabling DISCONNECT entry
    disableMenuItem(MNU_Reconnect, FALSE);  // enabling RCONNECT entry
    disableMenuItem(MNU_RawCommand, FALSE); // enabling SEND_FT_CMD entry
    disableMenuItem(MNU_FilesTitle, FALSE); // enabling FILES menu
    if(CurrentState.ADTMode) disableMenuItem(MNU_ToggleADT, FALSE); // enabling TOGGLEADT entry
#endif

    switch (state) {
      case MB_DISCONNECTED:
#ifdef _MENUCLASS_
       disableMenuItem(MNU_Disconnect, TRUE); // disabling DISCONNECT entry
       disableMenuItem(MNU_Reconnect, TRUE);  // disabling RCONNECT entry
       disableMenuItem(MNU_RawCommand, TRUE); // disabling SEND_FT_CMD entry
       disableMenuItem(MNU_FilesTitle, TRUE); // disabling FILES menu
       disableMenuItem(MNU_ToggleADT, TRUE);  // disabling TOGGLEADT entry
#endif

	DisableGadget(MG_List[MG_DirName], TRUE);
	DisableGadget(MG_List[MG_CacheList], TRUE);
	//DisableGadget(MG_List[MG_Reload], TRUE);
	if (SetGadgetAttrs((struct Gadget*)MG_List[MG_ListView], MainWindow, NULL,
			   LISTBROWSER_Labels, &dummy_list,
			   LISTBROWSER_ColumnInfo, NULL,//&dummycolumninfo,
			   LISTBROWSER_Striping, FALSE,
			   //LISTBROWSER_AutoFit, TRUE,
LISTBROWSER_ColumnTitles, FALSE,
			   TAG_DONE) && MainWindow)
	  RefreshGList((struct Gadget*)MG_List[MG_ListView], MainWindow, NULL, 1);

	lsel=-1;
	break;
      case MB_NONESELECTED:
        if (CurrentState.ADTMode) {
	    DisableGadget(MG_List[MG_DirName], TRUE);
	    DisableGadget(MG_List[MG_CacheList], TRUE);
		}
        else
        {
            DisableGadget(MG_List[MG_DirName], FALSE);
            DisableGadget(MG_List[MG_CacheList], FALSE);
            }
        /*
	if (CurrentState.ADTMode) {
	    if (MainPrefs.mp_ShowButtons)
	      DisableGadget(MG_List[MG_Readme], TRUE);
	    DisableGadget(MG_List[MG_DirName], TRUE);
	    DisableGadget(MG_List[MG_CacheList], TRUE);
	    DisableGadget(MG_List[MG_Reload], TRUE);
	}
	else {
	    if (MainPrefs.mp_ShowButtons)
	      DisableGadget(MG_List[MG_Parent], FALSE);
	    DisableGadget(MG_List[MG_DirName], FALSE);
	    DisableGadget(MG_List[MG_CacheList], FALSE);
	    DisableGadget(MG_List[MG_Reload], FALSE);
	}
	if (MainPrefs.mp_ShowButtons) {
	    DisableGadget(MG_List[MG_Get], TRUE);
	    DisableGadget(MG_List[MG_Put], FALSE);
	    DisableGadget(MG_List[MG_View], TRUE);
	    DisableGadget(MG_List[MG_Get2], TRUE);
	    DisableGadget(MG_List[MG_Put2], FALSE);
	    DisableGadget(MG_List[MG_View2], TRUE);
	    DisableGadget(MG_List[MG_Disconnect], FALSE);
	}    */
	lsel=-1;
	break;
	  case MB_FILESELECTED:

        if (CurrentState.ADTMode) {
	    DisableGadget(MG_List[MG_DirName], TRUE);
	    DisableGadget(MG_List[MG_CacheList], TRUE);
		}
        /*
	if (CurrentState.ADTMode) {
	    if (MainPrefs.mp_ShowButtons)
	      DisableGadget(MG_List[MG_Readme], FALSE);
	    DisableGadget(MG_List[MG_DirName], TRUE);
	    DisableGadget(MG_List[MG_CacheList], TRUE);
	    DisableGadget(MG_List[MG_Reload], TRUE);
	}
	else {
	    if (MainPrefs.mp_ShowButtons)
	      DisableGadget(MG_List[MG_Parent], FALSE);
	    DisableGadget(MG_List[MG_DirName], FALSE);
	    DisableGadget(MG_List[MG_Reload], FALSE);
	    DisableGadget(MG_List[MG_CacheList], FALSE);
	}

	if (MainPrefs.mp_ShowButtons) {
	    DisableGadget(MG_List[MG_Get], FALSE);
	    DisableGadget(MG_List[MG_Put], FALSE);
	    DisableGadget(MG_List[MG_Disconnect], FALSE);
	    DisableGadget(MG_List[MG_View], FALSE);
	    DisableGadget(MG_List[MG_Get2], FALSE);
	    DisableGadget(MG_List[MG_Put2], FALSE);
	    DisableGadget(MG_List[MG_View2], FALSE);
	}       */
	break;
      default:
	break;
    }

    if (MainPrefs.mp_ShowToolBar)
      UpdateSpeedBar(state);

    prev_state=state;

    if (MainWindow)
      RefreshGList((struct Gadget*)MG_List[MG_CacheList], MainWindow, NULL, 1);
}
#undef DisableGadget

void AttachToolList(const BOOL NoneSelected)
{
DBUG("AttachToolList()\n",NULL);
		SetAttrs((struct Gadget*)MG_List[MG_ListView],
		         LISTBROWSER_Labels, FileList,
		         LISTBROWSER_ColumnInfo,     columninfo,
		         LISTBROWSER_ColumnTitles,   TRUE,
		         LISTBROWSER_TitleClickable, TRUE,
		         LISTBROWSER_SortColumn,     COL_NAME,
		         LISTBROWSER_Striping, TRUE,
		         //NoneSelected?LISTBROWSER_AutoFit:TAG_IGNORE, TRUE,
		         NoneSelected? LISTBROWSER_Selected:TAG_IGNORE, -1,
		         NoneSelected? LISTBROWSER_MakeVisible:TAG_IGNORE, 0,
		        TAG_DONE);

	if (MainWindow) {
		RefreshGadgets((struct Gadget*)MG_List[MG_ListView], MainWindow, NULL);
		//RefreshGList((struct Gadget*)MG_List[MG_ListView], MainWindow, NULL, 1);
	}
	if (NoneSelected) lsel=-1;
}

void DetachToolList(void)
{
	SetAttrs(MG_List[MG_ListView], LISTBROWSER_Labels,~0, TAG_DONE);
	if (MainWindow) {
		RefreshGadgets((struct Gadget*)MG_List[MG_ListView], MainWindow, NULL);
		//RefreshGList((struct Gadget*)MG_List[MG_ListView], MainWindow, NULL, 1);
	}
}

void UpdateRemoteDir(const char *dir)
{
	if (dir  &&  dir!=&CurrentState.CurrentRemoteDir[0])
		strncpy(CurrentState.CurrentRemoteDir, dir, sizeof(CurrentState.CurrentRemoteDir));//255);

	if (MainWindow) {
		SetGadgetAttrs((struct Gadget*)MG_List[MG_DirName], MainWindow, NULL, STRINGA_TextVal,dir, TAG_END);
	}

	UpdateWindowTitle();
}

void UpdateLocalDir(const char *dir)
{
	if (dir!=&CurrentState.CurrentDLDir[0])
		strncpy(CurrentState.CurrentDLDir, dir, sizeof(CurrentState.CurrentDLDir));//255);

	if (MainWindow) {
		SetGadgetAttrs((struct Gadget*)MG_List[MG_DLGetFile], MainWindow, NULL, GETFILE_Drawer,dir, TAG_DONE);
	}

	UpdateWindowTitle();
}

void UpdateSiteName(const char *site)
{
	if (site!=CurrentState.CurrentSite)
		strncpy(CurrentState.CurrentSite, site, sizeof(CurrentState.CurrentSite));//50);

	if (MainWindow) {
		SetGadgetAttrs((struct Gadget*)MG_List[MG_SiteName], MainWindow, NULL, STRINGA_TextVal,site, TAG_END);
	}
}

void LockWindow(Object *window_object)
{
#ifndef _MENUCLASS_
	if (window_object==MainWin_Object)
		ClearMenuStrip(MainWindow);
#endif
DBUG("LockWindow()\n",NULL);
	SetAttrs(window_object, WA_BusyPointer,TRUE, TAG_DONE);
}
/*APTR SleepWindow(struct Window *win)
{
 struct Requester *lock = NULL;

 if(win)
  if( (lock=AllocVecTags(sizeof(struct Requester), AVT_ClearWithValue,0, TAG_DONE)) )
  {
   InitRequester(lock);
   Request(lock, win);
   if(win->FirstRequest == lock) {
    SetWindowPointer(win, WA_BusyPointer,TRUE, TAG_END);
    //SetAttrs(MainWin_Object, WA_BusyPointer,TRUE, TAG_END);
   }
   else
   {
    FreeVec(lock);
    lock = NULL;
   }
  }
DBUG("SleepWindow() 0x%08lx\n",lock);
 return(lock);
}*/

void UnlockWindow(Object *window_object)
{
	SetAttrs(window_object, WA_BusyPointer,FALSE, TAG_DONE);
DBUG("UnlockWindow()\n",NULL);
#ifndef _MENUCLASS_
	if (window_object==MainWin_Object) {
		struct Menu *mmenu=menu->NextMenu; // Files menu
		struct MenuItem *menuitem;

		menuitem=mmenu->FirstItem;
		menuitem=mmenu->FirstItem;
		menuitem=mmenu->FirstItem;
		menuitem=mmenu->FirstItem;

		menuitem->Flags|=TransferMode==BINARY? CHECKED : 0;
		menuitem=menuitem->NextItem;
		menuitem->Flags|=TransferMode==ASCII? CHECKED : 0;

		mmenu=mmenu->NextMenu; // Sort menu
		menuitem=mmenu->FirstItem;
		menuitem->Flags|=SortMode==SORTBYNAME? CHECKED : 0;
		menuitem=menuitem->NextItem;
		menuitem->Flags|=SortMode==SORTBYDATE? CHECKED : 0;

		mmenu=mmenu->NextMenu; // Settings menu
		menuitem=mmenu->FirstItem;
		menuitem=menuitem->NextItem;
		menuitem=menuitem->NextItem;

		menuitem=menuitem->NextItem;
		menuitem->Flags|=LogWindow? CHECKED : 0;

		menuitem=menuitem->NextItem;
		menuitem->Flags|=MainPrefs.mp_Showdotfiles? CHECKED : 0;

		menuitem=menuitem->NextItem;
		menuitem->Flags|=MainPrefs.mp_ShowAllADTFiles? CHECKED : 0;

		if (MainWindow) ResetMenuStrip(MainWindow, menu);
		else SetAttrs(window_object, WINDOW_MenuStrip, menu, TAG_DONE);
	}
#endif
}
/*void WakeWindow(struct Window *win, APTR lock)
{
DBUG("WakeWindow()  0x%08lx\n",lock);
 if(lock)
 {
  EndRequest(lock, win);
  FreeVec(lock);
  lock = NULL;
  SetWindowPointer(win, TAG_END);
  //SetAttrs(MainWin_Object, WA_BusyPointer,FALSE, TAG_END);
 }
}*/

void ChangeAmiFTPMode(void)
{
    /* goos amiganet mode another buttons ?
    if (CurrentState.ADTMode) {
	SetGadgetAttrs((struct Gadget*)MG_List[MG_Page2], MainWindow, NULL,
		       PAGE_Current, 1, TAG_DONE);
	RethinkLayout((struct Gadget*)pagelayout, MainWindow, NULL, TRUE);
    }
    else {
	SetGadgetAttrs((struct Gadget*)MG_List[MG_Page2], MainWindow, NULL,
		       PAGE_Current, 0, TAG_DONE);
	RethinkLayout((struct Gadget*)pagelayout, MainWindow, NULL, TRUE);
    }
    RefreshGList((struct Gadget*)pagelayout, MainWindow, NULL, 1);*/
}

void UpdateWindowTitle(void)
{
	static char title[100] = VERS " ("DATE") � 2020 Frank Menzel <goos.entwickler-x.de>";
	int numselfiles=0, numselbytes=0, round=0;
	char *bytes="kB";
	int freebytes=0;
	int64 freebytes64=0;
	char *fbytes="kB";
	struct InfoData info;
	BPTR lock;

	if ( (lock=Lock(CurrentState.CurrentDLDir, SHARED_LOCK)) ) {//ACCESS_READ)) ) {
		char pathname[256];
		int i = 0;

		NameFromLock(lock, pathname, 256);
		UnLock(lock);
		while (pathname[i]!=':'  &&  pathname[i]) { i++; }

		i++;
		pathname[i]='\0';

		//if (!getdfs(pathname, &info)) {
		if (GetDiskInfoTags(GDI_StringNameInput,pathname, GDI_InfoData,&info)) {
			freebytes64=(int64)(info.id_NumBlocks-info.id_NumBlocksUsed)*(int64)info.id_BytesPerBlock;

			if (freebytes64 < 1000000) {
				freebytes = freebytes64/1024;
				fbytes="kB";
			}
			if (freebytes64 >= 1000000000) {
				freebytes = freebytes64/(1024*1024*1024);
				fbytes="GB";
			}
			else {
				freebytes = freebytes64/(1024*1024);
				fbytes="MB";
			}
		}
	}

	if (FileList) {
		struct Node *node;

		for (node=GetHead(FileList); node != NULL; node = GetSucc(node)) {
			ULONG sel;
			struct dirlist *ptr;

			GetListBrowserNodeAttrs(node, LBNA_Selected, &sel, TAG_DONE);
			if (sel && (ptr=(void *)node->ln_Name)) {
				numselfiles++;
				numselbytes+=ptr->size==-1?0:ptr->size;
			}
		}

		if (numselbytes < 1000000) {
			numselbytes/=1024;
			bytes="kB";

			sprintf(title, GetAmiFTPString(Str_WindowTitle),
numselfiles, numselbytes, bytes, freebytes, fbytes);
		}
		else {
			round = numselbytes;
			numselbytes/=(1024*1024);
			bytes="MB";
			round -= numselbytes*(1024*1024);
			round/=100000;         // not 100% accurate..
			if(round>9) round = 9; //..but does its job

			sprintf(title, GetAmiFTPString(Str_WindowTitleMB),
numselfiles, numselbytes,decimalSeparator,round, bytes, freebytes, fbytes);
		}
	}
//	sprintf(title, GetAmiFTPString(Str_WindowTitle),
//numselfiles, numselbytes,decimalSeperator,round, bytes, freebytes, fbytes);
	SetAttrs(MainWin_Object, WA_Title,title, TAG_DONE);
}

static void ScrollListbrowser(ULONG direction)
{
	if (FileList) {
		if (GetHead(FileList)!=NULL) {
			SetGadgetAttrs((struct Gadget*)MG_List[MG_ListView], MainWindow, NULL,
			               LISTBROWSER_Position, direction, TAG_DONE);
		}
	}
}

static Object *listimage;
void CreateInfoList(struct List *list)
{
	struct Node *node;
	//extern struct Image im;
	//extern char *linfotext;
	int i;
	char *infostrings[]={ VERS " ("DATE")",
	                      NULL,//"Copyright �1995-1998 by Magnus Lilja",
	                      NULL,//"�2020 by Frank Menzel for Amiga OS4 community",
	                      "<www.OS4Welt.de>",
	                      NULL };

	//infostrings[0]=linfotext;
	infostrings[1] = GetAmiFTPString(Str_AboutStr_info01);
	infostrings[2] = GetAmiFTPString(Str_AboutStr_info02);
	if(Strlen(GetAmiFTPString(Str_AboutStr_info03)) != 0) {
		infostrings[3] = GetAmiFTPString(Str_AboutStr_info03);
		infostrings[4] = "<www.OS4Welt.de>";
	}

	NewList(list);

	if ( (listimage=NewObject(BitMapClass, NULL, //"bitmap.image",
	                          BITMAP_SourceFile, "PROGDIR:images/AmiFTP.png",
	                          BITMAP_Screen,     Screen,
	                          BITMAP_Precision,  PRECISION_EXACT,
	                          BITMAP_Masking,    TRUE,
	                         TAG_END)) ) {
		if ( (node=AllocListBrowserNode(1,
		                                LBNA_Flags, LBFLG_READONLY,
		                                LBNA_Column, 0,
		                                 LBNCA_Image,         (ULONG)listimage,
		                                 LBNCA_Justification, LCJ_CENTRE,
		                               TAG_END)) ) {
			AddTail(list, node);
		}
	}

	for (i=0; i<sizeof(infostrings)/sizeof(char*); i++) {
		if ( (node=AllocListBrowserNode(1,
		                                LBNA_Flags, LBFLG_READONLY,
		                                LBNA_Column, 0,
		                                 LBNCA_Text,          infostrings[i],
		                                 LBNCA_Justification, LCJ_CENTRE,
		                               TAG_DONE)) ) {
			AddTail(list, node);
		}
	}

}

void FreeInfoList(struct List *list)
{
	FreeListBrowserList(list);

	if (listimage) DisposeObject(listimage);
}

/* EOF */
