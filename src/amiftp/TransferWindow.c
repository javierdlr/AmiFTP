/* RCS Id: $Id: TransferWindow.c 1.803 1996/11/30 21:14:49 lilja Exp lilja $
   Locked version: $Revision: 1.803 $
*/

#include "AmiFTP.h"
#include "gui.h"

BOOL AskGetDir(void);


int GetDir(char *remote_parent, char *local_parent, char *name, char *localname);
void UpdateTransTitle(struct Node *node);

struct Window *TransferWindow;
Object *TransferLayout;
Object *TransferWin_Object;

struct Window *OpenTransferWindow(void);
void CloseTransferWindow(void);

enum {
    TG_RemoteFile=0, TG_LocalFile, TG_CPS, TG_ETA,
    TG_Gauge, TG_Abort,
    NumGadgets_TG};

Object *TG_List[NumGadgets_TG];
ULONG fuelargs[]={0,0};
static int OverwriteAll=0;

int DownloadFile(struct List *flist, const char *localname, const int binary,
		 const int move)
{
    ULONG wait,signal,mainsignal,done=FALSE;
    int result=1;
    struct dirlist *curr;
    struct Node *node;
    char localfile[MAXPATHLEN+1];
    BOOL aborted=FALSE;

    settype(binary);
    fuelargs[0]=fuelargs[1]=0;
    OverwriteAll=0;

    if (MainWindow)
      if (!OpenTransferWindow())
	return TRANS_GUI;

    for (node=GetHead(flist);node != NULL; node=GetSucc(node)) {
	ULONG sel=0;
	GetListBrowserNodeAttrs(node, LBNA_Selected, &sel, TAG_DONE);
	if (sel) {
	    static char tmp[1024];
	    curr=(void *)node->ln_Name;
	    aborted=0;
	    UpdateTransTitle(node);
	    switch(curr->mode & S_IFMT) {
	      case S_IFDIR:
		if (AskGetDir()) {
		    if (result=GetDir(CurrentState.CurrentRemoteDir,
					  CurrentState.CurrentDLDir,curr->name,
					  curr->name)) {
			goto out;
		    }
		    if (result==TRSF_OK) {
			if (flist==FileList)
			  LBEditNode(MG_List[MG_ListView],MainWindow,NULL,
				     node,LBNA_Selected,FALSE,TAG_DONE);
			else
			  SetListBrowserNodeAttrs(node, LBNA_Selected, FALSE,
						  TAG_DONE);
		    }

		}
		break;
	      case S_IFREG:
		memset(localfile,0,sizeof(localfile));
		if (!localname) {
		    stcgfn(tmp,curr->name, 1024);
		    strmfp(localfile,CurrentState.CurrentDLDir,tmp, MAXPATHLEN);
		}
		else {
		    if (localname[strlen(localname)-1]=='/'||localname[strlen(localname)-1]==':') {
			strcpy(localfile,localname);
			stcgfn(localfile+strlen(localfile),curr->name, MAXPATHLEN-strlen(localfile));
		    }
		    else {
			if (getfa(localname)==1) {
			    strcpy(localfile,localname);
			    strcat(localfile,"/");
			    stcgfn(localfile+strlen(localfile),curr->name, MAXPATHLEN-strlen(localfile));
			}
			else {
			    if (index(localname,'/')||index(localname,':'))
			      strcpy(localfile,localname);
			    else
			      strmfp(localfile,CurrentState.CurrentDLDir,(char*)localname, MAXPATHLEN);
			}
		    }
		}
		if (curr->adt) {
		    char aname[35];
		    sprintf(aname,"%s/%s",curr->owner,curr->name);
		    result=get_file(aname, localfile, curr->size);
		    if (result==TRSF_OK && MainPrefs.mp_GetReadme) {
			char *readme=NameToReadme(curr->name, curr->readmelength);
			char lname[64];
			sprintf(aname, "%s/%s", curr->owner, readme);
			strmfp(lname, CurrentState.CurrentDLDir, readme, 64);
			result=get_file(aname, lname, curr->readmelen);
		    }
		}
		else {
		    result=get_file(curr->name, localfile, curr->size);
		}
		if (result==TRSF_OK) {
		    if (flist==FileList)
		      LBEditNode(MG_List[MG_ListView], MainWindow, NULL,
				 node, LBNA_Selected, FALSE, TAG_DONE);
		    else
		      SetListBrowserNodeAttrs(node, LBNA_Selected, FALSE, TAG_DONE);
		 }
		else if (result==TRSF_ABORTED)
		  goto out;
		break;
	      case S_IFLNK:
		{
		    char *name;
		    name=linkname(curr->name);
		    if (name) {
			memset(localfile,0,sizeof(localfile));
			if (!localname) {
			    stcgfn(tmp,name, 1024);
			    strmfp(localfile, CurrentState.CurrentDLDir, tmp, MAXPATHLEN);
			}
			else {
			    if (localname[strlen(localname)-1]=='/'||
				localname[strlen(localname)-1]==':') {
				strcpy(localfile, localname);
				stcgfn(localfile+strlen(localfile), name, MAXPATHLEN-strlen(localfile));
			    }
			    else {
				if (getfa(localname)==1) {
				    strcpy(localfile, localname);
				    strcat(localfile,"/");
				    stcgfn(localfile+strlen(localfile), name, MAXPATHLEN-strlen(localfile));
				}
				else {
				    if (index(localname, '/') ||
					index(localname, ':'))
				      strcpy(localfile, localname);
				    else
				      strmfp(localfile, CurrentState.CurrentDLDir,
					     (char*)localname, MAXPATHLEN);
				}
			    }
			}
			result=get_file(name, localfile, curr->size);
			if (result==TRSF_BADFILE) {
			    if (AskGetDir()) {
				if (result==GetDir(CurrentState.CurrentRemoteDir,CurrentState.CurrentDLDir,name,name)!=TRSF_OK) {
				    free(name);
				    goto out;
				}
			    }
			    if (flist==FileList)
			      LBEditNode(MG_List[MG_ListView], MainWindow, NULL,
					 node, LBNA_Selected, FALSE, TAG_DONE);
			    else
			      SetListBrowserNodeAttrs(node, LBNA_Selected, FALSE,
						      TAG_DONE);
			}
			else if (result==TRSF_OK) {
			    if (flist==FileList)
			      LBEditNode(MG_List[MG_ListView], MainWindow, NULL,
					 node, LBNA_Selected, FALSE, TAG_DONE);
			    else
			      SetListBrowserNodeAttrs(node, LBNA_Selected, FALSE,
						      TAG_DONE);
			}
			else if (result==TRSF_ABORTED)
			  goto out;
			free(name);
		    }
		}
		break;
	    }
	}
    }
    UpdateTransTitle(0);
    for (node=GetHead(flist);node!=NULL;node=GetSucc(node)) {
	ULONG t=0;
	GetListBrowserNodeAttrs(node, LBNA_Selected, &t, TAG_DONE);
	if (t) {
	    break;
	}
    }
    if (node==0 && flist==FileList) {
	UpdateMainButtons(MB_NONESELECTED);
    }
    UpdateWindowTitle();
  out:
    if (result==TRSF_OK || result==TRSF_ABORTED) {
	if (result==TRSF_OK && MainPrefs.mp_DisplayBeep && TransferWindow) {
	    DisplayBeep(Screen);
	}
	if (TransferWindow)
	  CloseTransferWindow();
	return result==TRSF_OK?TRANS_OK:TRANS_ABORTED;
    }

    if (TransferWin_Object) {
	GetAttr(WINDOW_SigMask, TransferWin_Object, &signal);

	SetGadgetAttrs((struct Gadget*)TG_List[TG_Abort], TransferWindow,NULL,
		       GA_Disabled, TRUE,
		       TAG_DONE);
	RefreshGList((struct Gadget*)TG_List[TG_Abort], TransferWindow, NULL,1);

	if (!SilentMode) {
	    while (!done) {
		GetAttr(WINDOW_SigMask, MainWin_Object, &mainsignal);
		wait=Wait(signal|AG_Signal|mainsignal);
		if (wait & AG_Signal)
		  HandleAmigaGuide();
		if (wait & signal)
		  done=HandleTransferIDCMP();
		if (wait & mainsignal)
		  HandleMainWindowIDCMP(FALSE);
	    }
	}
	
	CloseTransferWindow();
    }
    return TRANS_ERROR;
}

ULONG HandleTransferIDCMP(void)
{
    ULONG done=FALSE;
    ULONG result;
    struct wmHandle wcode={0};
    int16 code;
    //while((result=CA_HandleInput(TransferWin_Object,&code))!=WMHI_LASTMSG) {
    while((result=IDoMethod(TransferWin_Object, WM_HANDLEINPUT, &wcode))!=WMHI_LASTMSG){

        code = result&WMHI_KEYMASK;
    switch (result & WMHI_CLASSMASK) {
	  case WMHI_CLOSEWINDOW:
	    done=TRUE;
	    break;
	  case WMHI_GADGETUP:
	    done=TRUE;
	    break;
	  case WMHI_ICONIFY:
	    if (MainWindow) {
		//if (CA_Iconify(MainWin_Object))
        if (IDoMethod(MainWin_Object, WM_ICONIFY))
		  MainWindow=NULL;
	    }
	    else {
		MainWindow=(struct Window *)IDoMethod(MainWin_Object, WM_OPEN);//CA_OpenWindow(MainWin_Object);
		MoveWindowInFrontOf(TransferWindow, MainWindow);
	    }
	    break;
	  case WMHI_RAWKEY:
	    if (code==95)
	      SendAGMessage(AG_TRANSWIN);
	    break;
	}
    }
    return done;
}

struct Window *OpenTransferWindow(void)
{
    Object *g1, *g2;
    struct LayoutLimits limits;
    extern struct RastPort *ARPort;
    LONG len=TextLength(ARPort,"0",1);
    fuelargs[0]=fuelargs[1]=0;//&fargs;

    if (TransferWindow)
      return TransferWindow;

    TransferLayout=LayoutObject,
		GA_DrawInfo, DrawInfo,
		GA_TextAttr, AmiFTPAttrF,
		LAYOUT_DeferLayout, TRUE,
		LAYOUT_SpaceOuter, TRUE,
		LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
		LAYOUT_HorizAlignment, LALIGN_CENTRE,

		StartHGroup,
			StartVGroup,
				StartVGroup,
					LAYOUT_BevelStyle, BVS_GROUP,
					LAYOUT_SpaceOuter, TRUE,
					LOffset(2), ROffset(2), TOffset(1), BOffset(1),

					StartMember, TG_List[TG_RemoteFile]=ButtonObject,
						GA_ID, TG_RemoteFile,
						GA_RelVerify, TRUE,
						GA_ReadOnly, TRUE,
						GA_Underscore, 0,
						GA_Text, " ",
						BUTTON_Justification, BCJ_LEFT,
					ButtonEnd,
					CHILD_MinWidth, PropFont->tf_XSize*35,
					Label(GetAmiFTPString(TW_RemoteFile)),

					StartMember, TG_List[TG_LocalFile]=ButtonObject,
						GA_ID, TG_LocalFile,
						GA_RelVerify, TRUE,
						GA_ReadOnly, TRUE,
						GA_Underscore, 0,
						GA_Text, " ",
						BUTTON_Justification, BCJ_LEFT,
					ButtonEnd,
					CHILD_MinWidth, PropFont->tf_XSize*35,
					Label(GetAmiFTPString(TW_LocalFile)),
				EndGroup,

				StartVGroup, EvenSized,
					LAYOUT_BevelStyle, BVS_GROUP,
					LAYOUT_SpaceOuter, TRUE,

					StartMember, g1=LayoutObject, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ, EvenSized,

					StartMember, TG_List[TG_ETA]=ButtonObject,
						GA_ID, TG_ETA,
						GA_RelVerify, TRUE,
						GA_ReadOnly, TRUE,
						GA_Text, "00:00:00",
						BUTTON_Justification, BCJ_RIGHT,
					ButtonEnd,
					CHILD_MinWidth, len*9,
					Label(GetAmiFTPString(TW_TimeLeft)),

					StartMember, TG_List[TG_CPS]=ButtonObject,
						GA_ID, TG_CPS,
						GA_RelVerify, TRUE,
						GA_ReadOnly, TRUE,
						GA_Text, " ",
						BUTTON_Justification, BCJ_RIGHT,
					ButtonEnd,
					CHILD_MinWidth, len*10,
					Label(GetAmiFTPString(TW_Cps)),
				EndGroup,

				StartMember, g2=LayoutObject, LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
				StartMember, TG_List[TG_Gauge]=FuelGaugeObject,
					FUELGAUGE_Orientation, FUELGAUGE_HORIZONTAL,
					FUELGAUGE_Justification, FGJ_CENTER,
					FUELGAUGE_Percent, FALSE,
					FUELGAUGE_Min, 0,
					FUELGAUGE_Max, 100,
					FUELGAUGE_Level, 0,
	           		FUELGAUGE_Ticks, 0,
					GA_Text, "%ld/%ld",
					FUELGAUGE_VarArgs, &fuelargs,
				FuelGaugeEnd,
                		 CHILD_MinHeight, len*3,
	            CHILD_MinWidth, len*18,
				CHILD_WeightedHeight, 0,
				Label(GetAmiFTPString(TW_DataTransferred)),
	            EndGroup,
			EndGroup,
			EndGroup,
		EndGroup,

		StartMember, TG_List[TG_Abort]=ButtonObject,
			GA_ID, TG_Abort,
			GA_RelVerify, TRUE,
			GA_Text, GetAmiFTPString(TW_Abort),
		ButtonEnd,
		CHILD_WeightedWidth, 0,
		CHILD_NominalSize, TRUE,
	LayoutEnd;

    if (!TransferLayout)
      return NULL;

    SetAttrs(g1, LAYOUT_AlignLabels, g2, TAG_DONE);
    SetAttrs(g2, LAYOUT_AlignLabels, g1, TAG_DONE);

    LayoutLimits((struct Gadget *)TransferLayout,&limits,PropFont,Screen);
    limits.MinHeight+=Screen->WBorTop+Screen->WBorBottom;
    limits.MinWidth+=Screen->WBorLeft+Screen->WBorRight;

    TransferWin_Object = WindowObject,
                          WA_Title, GetAmiFTPString(TW_WinTitle),
                          WA_PubScreen,Screen,
                          WA_DepthGadget, TRUE,
                          WA_DragBar, TRUE,
                          WA_CloseGadget, TRUE,
                          WA_Activate, TRUE,
                          WA_SmartRefresh, TRUE,
                          WA_Top, MainWindow->TopEdge+(MainWindow->Height-limits.MinHeight)/2,
                          WA_Left, MainWindow->LeftEdge+(MainWindow->Width-limits.MinWidth)/2,
                          WINDOW_IconifyGadget,TRUE,
                          WINDOW_ParentGroup, TransferLayout,
                          WA_IDCMP, IDCMP_RAWKEY,
                        EndWindow;

    if (!TransferWin_Object)
      return NULL;

    if (TransferWindow=(struct Window *)IDoMethod(TransferWin_Object, WM_OPEN)){//CA_OpenWindow(TransferWin_Object)) {
	return TransferWindow;
    }
    DisposeObject(TransferLayout);
    TransferLayout=NULL;

    return NULL;
}

void CloseTransferWindow(void)
{
    if (TransferWin_Object) {
	DisposeObject(TransferWin_Object);
	TransferWindow=NULL;
	TransferWin_Object=NULL;
	TransferLayout=NULL;
    }
}

static long FileSize=0,last=0;

int UploadFile(struct List *transferlist, const char *remote, const int binary)
{
    ULONG wait,signal,mainsignal,done=FALSE;
    char buf[20];
    int res;
    struct Node *node;

    settype(binary);

    if (MainWindow)
      if (!OpenTransferWindow())
	return TRANS_GUI;

    for (node=GetHead(transferlist);node!=NULL;node=GetSucc(node)) {
	int size=0;
	char *rem=0;
	//BPTR lock;
	//struct FileInfoBlock __aligned fib;
	struct dirlist *entry=(void *)node->ln_Name;
	
	UpdateTransTitle(node);
	if (CurrentState.ADTMode) {
	    rem=malloc(256);
	    strcpy(rem, "new/");
	    strcat(rem, FilePart(entry->name));
	    remote=rem;
	}

	if (entry->size)
	  size=entry->size;
	else {
        /*
	    lock=Lock(entry->name,SHARED_LOCK);

	    if (lock) {
		Examine(lock,&fib);
		size=fib.fib_Size;
		UnLock(lock);
        }
        */

        struct ExamineData *dat = ExamineObjectTags(EX_StringNameInput,entry->name,TAG_END);
    	if (dat)
    	{
            size = dat->FileSize;
            FreeDosObject(DOS_EXAMINEDATA,dat);
        }
	}

	if (TransferWindow) {
	    sprintf(buf,"%ld",size);
	    SetGadgetAttrs((struct Gadget*)TG_List[TG_LocalFile], TransferWindow, NULL,
			   GA_Text, entry->name,
			   TAG_DONE);
	    RefreshGList((struct Gadget*)TG_List[TG_LocalFile],TransferWindow,NULL,1);
	    SetGadgetAttrs((struct Gadget*)TG_List[TG_RemoteFile], TransferWindow, NULL,
			   GA_Text,remote?(char *)remote:(char *)FilePart(entry->name),
			   TAG_DONE);
	    RefreshGList((struct Gadget*)TG_List[TG_RemoteFile],TransferWindow,NULL,1);
	    SetGadgetAttrs((struct Gadget*)TG_List[TG_CPS], TransferWindow, NULL,
			   GA_Text,"0",
			   TAG_DONE);
	    RefreshGList((struct Gadget*)TG_List[TG_CPS],TransferWindow,NULL,1);
	    RefreshGList((struct Gadget*)TG_List[TG_CPS],TransferWindow,NULL,1);
	    fuelargs[0]=fuelargs[1]=0;
	    SetGadgetAttrs((struct Gadget*)TG_List[TG_Gauge],TransferWindow,NULL,
			       FUELGAUGE_Level,0,
			       FUELGAUGE_VarArgs,&fuelargs,
			       TAG_DONE);

	    FileSize=size;
	    last=0;
	}
	res=sendrequest("STOR",entry->name,remote?(char *)remote:(char *)FilePart(entry->name));
	if (rem) {
	    free(rem);
	    rem=NULL;
	}
	if (res!=TRSF_OK)
	  break;
    }

    if (!TransferWin_Object)
      return res;
    UpdateTransTitle(0);

    if (res==TRSF_ABORTED||res==TRSF_OK) {
	if (res==TRSF_OK && MainPrefs.mp_DisplayBeep && TransferWindow) {
	    DisplayBeep(Screen);
	}
	CloseTransferWindow();
	return res;
    }

    GetAttr(WINDOW_SigMask, TransferWin_Object, &signal);
    GetAttr(WINDOW_SigMask, MainWin_Object, &mainsignal);

    SetGadgetAttrs((struct Gadget*)TG_List[TG_Abort], TransferWindow,NULL,
		   GA_Disabled,TRUE,
		   TAG_DONE);
    RefreshGList((struct Gadget*)TG_List[TG_Abort],TransferWindow,NULL,1);

    if (!SilentMode) {
	while (!done) {
	    wait=Wait(signal|AG_Signal);
	    
	    if (wait&signal)
	      done=HandleTransferIDCMP();
	    if (wait & AG_Signal)
	      HandleAmigaGuide();
	    if (wait & mainsignal)
	      HandleMainWindowIDCMP(FALSE);
	}
    }

    CloseTransferWindow();
    return TRANS_ERROR;
}


void UpdateDLGads(const long bytes, const long restart_point, const time_t timee)
{

    if (TransferWin_Object) {
	static char buf1[20],buf2[20],buf3[20];
	int hours=0,mins=0,secs=0, cps=0;
	if (timee) {
	    cps=(bytes-restart_point)/timee;
	    if (FileSize && cps) {
		secs=(FileSize-bytes)/cps;
		hours=secs / 3600;
		mins = (secs-hours*3600)/60;
		secs=secs%60;
	    }
	}

	sprintf(buf1, "%ld", bytes);
	sprintf(buf2, "%ld", cps);
	sprintf(buf3, "%0.2d:%0.2d:%0.2d", hours, mins, secs);
	if (SetGadgetAttrs((struct Gadget*)TG_List[TG_CPS], TransferWindow, NULL,
			   GA_Text, buf2, TAG_DONE)){ 
	    RefreshGList((struct Gadget*)TG_List[TG_CPS], TransferWindow, NULL, 1);
	}
	if (SetGadgetAttrs((struct Gadget*)TG_List[TG_ETA], TransferWindow, NULL,
			   GA_Text, buf3, TAG_DONE)) {
	    RefreshGList((struct Gadget*)TG_List[TG_ETA], TransferWindow, NULL, 1);
	}
	fuelargs[0]=bytes;//FileSize?(bytes*100)/FileSize:0;
	fuelargs[1]=FileSize;
	SetGadgetAttrs((struct Gadget*)TG_List[TG_Gauge], TransferWindow, NULL,
		       FUELGAUGE_Level, FileSize?(FileSize>10240?bytes/(FileSize/100):(bytes*100)/FileSize):0,
		       FUELGAUGE_VarArgs, &fuelargs[0],
		       TAG_DONE);
    }
}


BOOL CheckExists(char *lfile,int size, ULONG *restartpoint)
{
    /*
    static ULONG tags[]={
	RTEZ_ReqTitle, NULL,
	RT_Window, NULL,
	RT_LockWindow, TRUE,
	RT_ReqPos, REQPOS_CENTERWIN,
	TAG_END
      };
      */
   

    

    

	/*
    BPTR lock;
    struct FileInfoBlock fib;
    */
    int ret = 0;
    char *Gadgetstring;

    /*
    lock=Lock(lfile,ACCESS_READ);
    if (!lock)
      return FALSE;
    Examine(lock,&fib);
    UnLock(lock);
    */
    int64 fileSize = 0;
    struct ExamineData *data = ExamineObjectTags(EX_StringNameInput, lfile, TAG_END);
    if (data)
    {
        fileSize = data->FileSize;
        FreeDosObject(DOS_EXAMINEDATA, data);
    }
    if (SilentMode || OverwriteAll) {
	*restartpoint=0;
	return FALSE;
    }

    
    

    /*
    tags[1]=(ULONG)GetAmiFTPString(Str_AmiFTPRequest);
    tags[3]=(ULONG)MainWindow;
    */

    Gadgetstring=malloc(strlen(GetAmiFTPString(TW_OverwriteAll))+
			strlen(GetAmiFTPString(TW_Overwrite))+
			strlen(GetAmiFTPString(TW_Resume))+
			strlen(GetAmiFTPString(TW_CancelTransfer))+6);
    if (!Gadgetstring)
    {
      return FALSE;
    }
    if (fileSize>=size) {
	sprintf(Gadgetstring,"%s|%s|%s", 
		GetAmiFTPString(TW_Overwrite),
		GetAmiFTPString(TW_OverwriteAll),
		GetAmiFTPString(TW_CancelTransfer));



        /*
	ret=(BOOL)rtEZRequest(GetAmiFTPString(TW_FileExists), Gadgetstring, NULL,
			      (struct TagItem *)tags, lfile, fib.fib_Size, size);
                  */
    ret = showRequester(MainWindow, GetAmiFTPString(Str_AmiFTPRequest), Gadgetstring, GetAmiFTPString(TW_FileExists), fileSize, size);
	free(Gadgetstring);
	*restartpoint=0;
	if (ret==1)
    {
	  return FALSE;
	}
    if (ret==2) {
	    OverwriteAll = 1;
	    return FALSE;
	}
    }
    else {    
	sprintf(Gadgetstring,"%s|%s|%s|%s",
		GetAmiFTPString(TW_Overwrite),
		GetAmiFTPString(TW_OverwriteAll),
		GetAmiFTPString(TW_Resume),
		GetAmiFTPString(TW_CancelTransfer));
        /*
	ret=(BOOL)rtEZRequest(GetAmiFTPString(TW_FileExists), Gadgetstring, NULL,
			      (struct TagItem *)tags, lfile, fib.fib_Size, size);
                  */
    ret = showRequester(MainWindow, GetAmiFTPString(Str_AmiFTPRequest), Gadgetstring, GetAmiFTPString(TW_FileExists), fileSize, size);
	free(Gadgetstring);
	if (ret==1)
    {
	  return FALSE;
    }
	if (ret==2) {
	    OverwriteAll = 1;
	    return FALSE;
	}
	if (ret==3) {
	    *restartpoint=fileSize;
	    return FALSE;
	}
    }
    return TRUE;
}
static char tbufs[50];

void SetTransferSize(const long size)
{
    sprintf(tbufs, "%ld", size);
    FileSize=size;
    fuelargs[0]=0;
    fuelargs[1]=FileSize;
    
    if (TransferWindow) {
	SetGadgetAttrs((struct Gadget*)TG_List[TG_Gauge], TransferWindow, NULL,
		       FUELGAUGE_VarArgs, &fuelargs,
		       TAG_DONE);
	RefreshGList((struct Gadget*)TG_List[TG_Gauge], TransferWindow, NULL, 1);
    }
}

int get_file(char *name, char *localname, int size)
{
    int		rval;
    ULONG restartpoint=0;
    static char buf[50];

    if (TransferWindow) {
	sprintf(buf,"%ld",size);
	FileSize=size;
	last=0;
	if (SetGadgetAttrs((struct Gadget*)TG_List[TG_LocalFile], TransferWindow, NULL,
			   GA_Underscore, '\n',
			   GA_Text, localname,
			   TAG_DONE))
	  RefreshGList((struct Gadget*)TG_List[TG_LocalFile], TransferWindow, NULL, 1);
	if (SetGadgetAttrs((struct Gadget*)TG_List[TG_RemoteFile], TransferWindow, NULL,
			   GA_Text, name,
			   TAG_DONE))
	  RefreshGList((struct Gadget*)TG_List[TG_RemoteFile], TransferWindow, NULL, 1);
	if (SetGadgetAttrs((struct Gadget*)TG_List[TG_CPS], TransferWindow, NULL,
			   GA_Text,"0",
			   TAG_DONE))
	  RefreshGList((struct Gadget*)TG_List[TG_CPS], TransferWindow, NULL, 1);
	fuelargs[0]=0;
	fuelargs[1]=size;
	SetGadgetAttrs((struct Gadget*)TG_List[TG_Gauge], TransferWindow, NULL,
		       FUELGAUGE_Level, 0,
		       FUELGAUGE_VarArgs, &fuelargs,
		       TAG_DONE);
    }
    if (CheckExists(localname, size, &restartpoint))
      return TRSF_OK;
    rval = recvrequest("RETR", localname, name, "w", restartpoint);
    return rval;
}

char *make_path(char *parent, char *curdir)
{
    char	*tmp;

    if (*curdir == '/') {
	tmp = (char *)strdup(curdir);
    } else {
	tmp = (char *)calloc((unsigned int)(strlen(parent) + 1 +
					    strlen(curdir) + 1),1);
	if (tmp == NULL)
	  return NULL;
	strcpy(tmp, parent);
	if (strcmp(parent, "/") && parent[strlen(parent)-1]!=':')
	  strcat(tmp, "/");
	strcat(tmp, curdir);
    }
    return tmp;
}

BOOL AskGetDir(void)
{
    /*
    static ULONG tags[]={
	RTEZ_ReqTitle, NULL,
	RT_Window, NULL,
	RT_LockWindow, TRUE,
	RT_ReqPos, REQPOS_CENTERWIN,
	TAG_END
      };
      */

    if (!MainWindow)
      return FALSE;

    BOOL ret = showRequester(MainWindow, GetAmiFTPString(Str_AmiFTPRequest), GetAmiFTPString(TW_GetDir), GetAmiFTPString(TW_DownloadDir))==1;
    /*
    tags[1]=(ULONG)GetAmiFTPString(Str_AmiFTPRequest);
    tags[3]=(ULONG)MainWindow;
    ret=(BOOL)rtEZRequest(GetAmiFTPString(TW_DownloadDir),
			  GetAmiFTPString(TW_GetDir), NULL, (struct TagItem *)tags);
    */
    return ret;
}

struct QueueEntry {
	struct Node qe_Node;
	struct List *filelist;
	char *dirname;
	char *localname;
	char *remote_parent;
	char *local_parent;
    };

void PrintQueue(struct List *queue)
{
    
    struct QueueEntry *qe;
    for (qe=(struct QueueEntry *)GetHead(queue);
	 qe != NULL;
	 qe=(struct QueueEntry *)GetSucc((struct Node *)qe)) {
	Printf("remote_parent '%s' dirname '%s'\nlocal_parent '%s' localname '%s'\n",
	       qe->remote_parent,qe->dirname,qe->local_parent,qe->localname);
	Printf("----------------------------------------------------\n");
    }
}

int GetDir(char *remote_parent, char *local_parent, char *name, char *localname)
{
   
    struct List Queue;
    struct QueueEntry *first,*entry;
    struct Node *node;
    BPTR newdir;
    int len,rval;

    NewList(&Queue);

    first=calloc(sizeof(struct QueueEntry),1);
    if (!first)
      return TRSF_FAILED;
    first->localname=strdup(localname);
    if (!first->localname) {
	free(first);
	return TRSF_FAILED;
    }
    first->local_parent=strdup(local_parent);
    if (!first->local_parent) {
	free(first->localname);
	free(first);
	return TRSF_FAILED;
    }
    first->remote_parent=strdup(remote_parent);
    if (!first->remote_parent) {
	free(first->localname);
	free(first->local_parent);
	free(first);
	return TRSF_FAILED;
    }
    first->dirname=strdup(name);
    if (!first->dirname) {
	free(first->localname);
	free(first->local_parent);
	free(first->remote_parent);
	free(first);
	return TRSF_FAILED;
    }

    AddHead(&Queue,(struct Node*)first);
    while (entry=(struct QueueEntry *)RemTail(&Queue)) {
	char *ldir,*rdir;
	struct dirlist *tmp;

	len=strlen(entry->local_parent)+strlen(entry->localname)+2;
	ldir=calloc(len,1);
	if (ldir==NULL) {
	    rval=TRSF_FAILED;
	    goto out;
	}
	strcpy(ldir,entry->local_parent);
	AddPart(ldir,entry->localname,len);

	rdir=make_path(entry->remote_parent,entry->dirname);
	if (rdir==NULL) {
	    rval=TRSF_FAILED;
	    free(ldir);
	    goto out;
	}
	newdir=CreateDir(ldir);
	if (!newdir && IoErr()!=ERROR_OBJECT_EXISTS) {
	    rval=TRSF_FAILED;
	    goto out;
	}
	UnLock(newdir);
	command("CWD %s",rdir);
	if (code!=250) {
	    free(rdir);
	    free(ldir);
	    rval=TRSF_FAILED;
	    ShowErrorReq(GetAmiFTPString(Str_CDFailed));
	    goto out;
	}
	entry->filelist=read_remote_dir();
	if (!entry->filelist) {
	    free(rdir);
	    free(ldir);
	    rval=TRSF_FAILED;
	    goto out;
	}

	for (node=GetHead(entry->filelist); node != NULL;node=GetSucc(node)) {
	    tmp=(void *)node->ln_Name;
	    if (S_ISREG(tmp->mode)) {
        int tmpsize = strlen(ldir)+strlen(tmp->name)+4;
		char *tmpname=malloc(tmpsize);
		if (!tmpname) {
		    rval=TRSF_FAILED;
		    goto out;
		}
		strmfp(tmpname,ldir,tmp->name, tmpsize);

		if (get_file(tmp->name,tmpname,tmp->size)!=TRSF_OK) {
		    free(tmpname);
		    free(ldir);
		    free(rdir);
		    rval=TRSF_FAILED;
		    goto out;
		}
	    }
	}
	for (node=GetHead(entry->filelist); node != NULL;node=GetSucc(node)) {
	    tmp=(void *)node->ln_Name;
	    if (S_ISDIR(tmp->mode)) {
		struct QueueEntry *qentry;
		qentry=calloc(sizeof(struct QueueEntry),1);
		if (qentry) {
		    if (!(qentry->remote_parent=strdup(rdir))) {
			rval=TRSF_FAILED;
			free(qentry);
			goto out;
		    }
		    if (!(qentry->local_parent=strdup(ldir))) {
			rval=TRSF_FAILED;
			free(qentry->remote_parent);
			free(qentry);
			goto out;
		    }
		    if (!(qentry->dirname=strdup(tmp->name))) {
			rval=TRSF_FAILED;
			free(qentry->dirname);
			free(qentry->remote_parent);
			free(qentry);
			goto out;
		    }
		    if (!(qentry->localname=strdup(tmp->name))) {
			rval=TRSF_FAILED;
			free(qentry->localname);
			free(qentry->dirname);
			free(qentry->remote_parent);
			free(qentry);
			goto out;
		    }
		    AddHead(&Queue,(struct Node*)qentry);
		}
	    }
	}
	for (node=GetHead(entry->filelist); node != NULL;node=GetSucc(node)) {
	    tmp=(void *)node->ln_Name;
	    if (S_ISLNK(tmp->mode)) {
		char *lname;
		int rval;
		lname=linkname(tmp->name);
		if (!lname) {
		    rval=TRSF_FAILED;
		    goto out;
		}
		rval=get_file(tmp->name,lname,-1);
		if (rval==TRSF_FAILED) {
		    struct QueueEntry *qentry;
		    if (!(qentry=calloc(sizeof(struct QueueEntry),1))) {
			free(ldir);
			free(rdir);
			goto out;
		    }
		    if (!(qentry->remote_parent=strdup(rdir))) {
			free(qentry);
			free(ldir);
			free(rdir);
			goto out;
		    }
		    if (!(qentry->local_parent=strdup(ldir))) {
			free(qentry->remote_parent);
			free(qentry);
			free(ldir);
			free(rdir);
			goto out;
		    }
		    if (!(qentry->dirname=strdup(lname))) {
			free(qentry->local_parent);
			free(qentry->remote_parent);
			free(ldir);
			free(rdir);
			free(qentry);
			goto out;
		    }
		    if (!(qentry->localname=strdup(lname))) {
			free(qentry->dirname);
			free(qentry->local_parent);
			free(qentry->remote_parent);
			free(qentry);
			free(ldir);
			free(rdir);
			goto out;
		    }
		    AddHead(&Queue,(struct Node*)qentry);
		}
		if (lname)
		  free(lname);
	    }
	}
	if (ldir)
	  free(ldir);
	if (rdir)
	  free(rdir);
	free_dirlist(entry->filelist);
	if (entry->remote_parent)
	  free(entry->remote_parent);
	if (entry->local_parent)
	  free(entry->local_parent);
	if (entry->dirname)
	  free(entry->dirname);
	if (entry->localname)
	  free(entry->localname);
	free(entry);
    }
    command("CWD %s",remote_parent);
    return TRSF_OK;
  out:
    while(entry=(struct QueueEntry *)RemHead(&Queue)) {
	if (entry->filelist)
	  free_dirlist(entry->filelist);
	if (entry->remote_parent)
	  free(entry->local_parent);
	if (entry->dirname)
	  free(entry->dirname);
	if (entry->localname)
	  free(entry->localname);
	free(entry);
    }
    command("CWD %s",remote_parent);
    return rval;
}

void UpdateTransTitle(struct Node *node)
{
    ULONG bytes=0;
    int numfiles=0;
    struct dirlist *entry;
    static char title[100];
    char *sbytes;

    if (!TransferWin_Object)
      return;

    for (;node; node=GetSucc(node)) {
	ULONG sel=0;
	GetListBrowserNodeAttrs(node, LBNA_Selected, &sel, TAG_DONE);
	if (sel) {
	    entry=(void *)node->ln_Name;
	    if (entry->size>0)
	      bytes+=entry->size;
	    numfiles++;
	}
    }

    if (bytes<1000000) {
	bytes/=1024;
	sbytes="kB";
    }
    else {
	bytes/=(1024*1024);
	sbytes="MB";
    }
     
    sprintf(title, GetAmiFTPString(TW_WinTitle),
	    numfiles, bytes, sbytes);
    SetAttrs(TransferWin_Object, WA_Title, title, TAG_DONE);
}
