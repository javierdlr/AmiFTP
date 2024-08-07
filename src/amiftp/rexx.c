/* RCS Id: $Id: rexx.c 1.795 1996/09/28 13:32:58 lilja Exp $
   Locked version: $Revision: 1.795 $
*/

#include "AmiFTP.h"
#include "gui.h"
#include "AmiFTP_rev.h"

#include <stdarg.h> // used in SetStemVar()


struct SiteNode curr_rexx;
BOOL ARexxQuitBit=FALSE;
Object *ARexx_Object;
BOOL SilentMode=FALSE;


static void rexx_GetFile(struct ARexxCmd *ac, struct RexxMsg *rexxmsg)
{
	struct ArgStruct {
		long ascii;
		long bin;
		char *file;
		char *local;
	} *args=(void *)ac->ac_ArgList;
	struct dirlist file;
	struct List filelist;
	//struct Node *node;

	NewList(&filelist);

	//memset(&file, 0, sizeof (file));
	ClearMem(&file, sizeof(file));
	file.name = args->file;
	file.size = 0;
	file.mode = S_IFREG;

	if (connected) {
			struct Node *node;

		if ((node=AllocListBrowserNode(1, LBNA_Selected,TRUE, LBNA_Column,0,
		                               LBNCA_Text,file.name, TAG_DONE))) {
			node->ln_Name=(void *)&file;
			AddTail(&filelist, node);
			if (DownloadFile(&filelist,args->local,args->ascii?ASCII:BINARY,0)==TRANS_OK) {
				ac->ac_RC=RC_OK;
			}
			else { ac->ac_RC=RC_WARN; }

			Remove(node);
			FreeListBrowserNode(node);
		}
		else { ac->ac_RC=RC_WARN; }

	}
	else { ac->ac_RC=RC_WARN; }
}

static void rexx_View(struct ARexxCmd *ac, struct RexxMsg *rexxmsg)
{
	struct ArgStruct {
		long ascii;
		long bin;
		char *file;
	} *args=(void *)ac->ac_ArgList;
	struct dirlist file;
	struct List filelist;
	//struct Node *node;
	//char loc_name[200];

	NewList(&filelist);

	//memset(&file, 0, sizeof(file));
	ClearMem(&file, sizeof(file));
	file.name = args->file;
	file.size = 0;
	file.mode = S_IFREG;

	if (connected) {
		struct Node *node;
		char loc_name[200];

		if ((node=AllocListBrowserNode(1, LBNA_Selected,TRUE, LBNA_Column,0,
		                               LBNCA_Text,file.name, TAG_DONE))) {
			node->ln_Name=(void *)&file;
			AddTail(&filelist, node);
			if (DownloadFile(&filelist,"T:",args->ascii?ASCII:BINARY,0)==TRANS_OK) {
				strmfp(loc_name, "T:", (STRPTR)FilePart(file.name), sizeof(loc_name));
				ViewFile(loc_name);
				ac->ac_RC=RC_OK;
			}
			else { ac->ac_RC=RC_WARN; }

			Remove(node);
			FreeListBrowserNode(node);
		}
		else { ac->ac_RC=RC_WARN; }

	}
	else { ac->ac_RC=RC_WARN; }
}

static void rexx_DeleteFile(struct ARexxCmd *ac, struct RexxMsg *rexxmsg)
{
	struct ArgStruct {
		char **files;
	} *args=(void *)ac->ac_ArgList;
	int i=0;

	if (!connected) {
		ac->ac_RC=RC_WARN;
		return;
	}

	while(args->files[i]) {
		if (!delete_remote(args->files[i], "DELE")) {
			ac->ac_RC=RC_WARN;
			return;
		}
		++i;
	}

	ac->ac_RC=RC_OK;
}

static void rexx_MGetFile(struct ARexxCmd *ac, struct RexxMsg *rexxmsg)
{
	struct ArgStruct {
		long ascii;
		long bin;
		char **files;
	} *args=(void *)ac->ac_ArgList;
	struct dirlist *curr;
	struct List dllist;
	struct Node *node;
	int i=0;

	if (!connected) {
		ac->ac_RC=RC_WARN;
		return;
	}

	NewList(&dllist);

	while(args->files[i]) {
		curr=malloc(sizeof(struct dirlist));
		if (curr) {
			curr->name = args->files[i];
			curr->size = 0;
			curr->mode = S_IFREG;
			if ((node=AllocListBrowserNode(1, LBNA_Selected,TRUE, LBNA_Column,0,
			                               LBNCA_Text,curr->name, TAG_DONE))) {
				node->ln_Name=(void *)curr;
				AddTail(&dllist, (struct Node *)node);
			}
		}
		++i;
	}

	if (DownloadFile(&dllist, NULL, args->ascii?ASCII:BINARY, 0)==TRANS_OK) {
		ac->ac_RC=0;
	}
	else { ac->ac_RC=RC_WARN; }

	while ((node=RemHead(&dllist))) {
		struct dirlist *curr = (void *)node->ln_Name;
		if (curr) { free(curr); }

		//Remove(node);
		FreeListBrowserNode(node);
	}
}

static void rexx_Connect(struct ARexxCmd *ac, struct RexxMsg *rexxmsg)
{
	struct ArgStruct {
		long noscan;
	}*args=(void *)ac->ac_ArgList;
	struct SiteNode sn;

	if (!TCPStack) {
		ac->ac_RC=RC_WARN;
		return;
	}

	if (curr_rexx.sn_Node.ln_Name) {
		Strlcpy(CurrentState.LastLVSite, curr_rexx.sn_Node.ln_Name, sizeof(CurrentState.LastLVSite));
		if (curr_rexx.sn_SiteAddress)
			Strlcpy(CurrentState.CurrentSite, curr_rexx.sn_SiteAddress, sizeof(CurrentState.CurrentSite));
		else
			CurrentState.CurrentSite[0]=0;
		if (curr_rexx.sn_RemoteDir)
			Strlcpy(CurrentState.CurrentRemoteDir, curr_rexx.sn_RemoteDir, sizeof(CurrentState.CurrentRemoteDir));
		else
			CurrentState.CurrentRemoteDir[0]=0;
		if (curr_rexx.sn_LocalDir)
			Strlcpy(CurrentState.CurrentDLDir, curr_rexx.sn_LocalDir, sizeof(CurrentState.CurrentDLDir));
		else
			CurrentState.CurrentDLDir[0]=0;
		if (curr_rexx.sn_LoginName)
			Strlcpy(CurrentState.LoginName, curr_rexx.sn_LoginName, sizeof(CurrentState.LoginName));
		else
			CurrentState.LoginName[0]=0;
		if (curr_rexx.sn_Password)
			Strlcpy(CurrentState.Password, curr_rexx.sn_Password, sizeof(CurrentState.Password));
		else
			CurrentState.Password[0]=0;

		CurrentState.Proxy = curr_rexx.sn_Proxy;
		CurrentState.Port  = curr_rexx.sn_Port;
	}

	if (!strlen(CurrentState.CurrentSite)) {
		ac->ac_RC=RC_WARN;
		return;
	}

	//memset(&sn, 0, sizeof(struct SiteNode));
	ClearMem(&sn, sizeof(struct SiteNode));
	sn.sn_RemoteDir    = strdup(CurrentState.CurrentRemoteDir);
	sn.sn_LocalDir     = CurrentState.CurrentDLDir;
	sn.sn_Node.ln_Name = CurrentState.CurrentSite;
	sn.sn_SiteAddress  = sn.sn_Node.ln_Name;
	sn.sn_Port         = CurrentState.Port? CurrentState.Port : 21;
	sn.sn_Proxy        = CurrentState.Proxy;
	sn.sn_LoginName    = CurrentState.LoginName;
	sn.sn_Password     = CurrentState.Password;
	sn.sn_Anonymous    = CurrentState.LoginName[0]? 0 : 1;

	if (ConnectSite(&sn,args->noscan)==CONN_OK) {
		ac->ac_RC=RC_OK;
	}
	else {
		ac->ac_RC=RC_WARN;
	}

	if (sn.sn_RemoteDir) free(sn.sn_RemoteDir);
}

static void rexx_Disconnect(struct ARexxCmd *ac, struct RexxMsg *rexxmsg)
{
	if (!TCPStack) {
		ac->ac_RC=RC_WARN;
		return;
	}

	quit_ftp();
	UpdateMainButtons(MB_DISCONNECTED);
	ac->ac_RC=RC_OK;
}

static void rexx_LCD(struct ARexxCmd *ac, struct RexxMsg *rexxmsg)
{
	struct ArgStruct {
		long parent;
		char *dir;
	} *args=(void *)ac->ac_ArgList;

	if (args->dir) {
		Strlcpy(CurrentState.CurrentDLDir, args->dir, sizeof(CurrentState.CurrentDLDir));//100);
		UpdateLocalDir(CurrentState.CurrentDLDir);
	}

	/* Todo: Fix Parent-mode */
	ac->ac_RC=RC_OK;
}

static void rexx_CD(struct ARexxCmd *ac, struct RexxMsg *rexxmsg)
{
	struct ArgStruct {
		long noscan;
		long parent;
		char *dir;
	} *args=(void *)ac->ac_ArgList;
	struct List *head;

	if (!args->dir && !args->parent) {
		ac->ac_RC=RC_WARN;
		return;
	}

	if (!connected) {
		ac->ac_RC=RC_WARN;
		return;
	}

	if (!change_remote_dir(args->parent?"..":args->dir, 0)) {
		if (!args->noscan) {
			if ((head=LookupCache(CurrentState.CurrentRemoteDir))) {
				FileList=head;
			}
			else if ((head=read_remote_dir())) {
				FileList=head;
				AddCacheEntry(head, CurrentState.CurrentRemoteDir);
			}
		}

		DetachToolList();
		AttachToolList(TRUE);
		ac->ac_RC=RC_OK;
	}
	else {
		ac->ac_RC=RC_WARN;
		ac->ac_RC2=code;
	}

	UpdateMainButtons(MB_NONESELECTED);
}

static void rexx_Site(struct ARexxCmd *ac, struct RexxMsg *rexxmsg)
{
	struct ArgStruct {
		char *name;
	} *args = (void *)ac->ac_ArgList;
	struct SiteNode *sn = NULL;
	struct Node *lbn;

	for (lbn=GetHead(&SiteList); lbn!=NULL; lbn=GetSucc(lbn)) {
		GetListBrowserNodeAttrs(lbn, LBNA_UserData,&sn, TAG_DONE);
		if (sn) /* Fix: broken */
			if (!stricmp(args->name, sn->sn_Node.ln_Name)
			    && strlen(args->name)==strlen(sn->sn_Node.ln_Name))
				break;
	}

	if (sn) {
		if (connected) {
			if (curr_rexx.sn_RemoteDir   ) free(curr_rexx.sn_RemoteDir);
			if (curr_rexx.sn_LocalDir    ) free(curr_rexx.sn_LocalDir);
			if (curr_rexx.sn_DirString   ) free(curr_rexx.sn_DirString);
			if (curr_rexx.sn_LoginName   ) free(curr_rexx.sn_LoginName);
			if (curr_rexx.sn_Password    ) free(curr_rexx.sn_Password);
			if (curr_rexx.sn_Node.ln_Name) free(curr_rexx.sn_Node.ln_Name);

			//memset(&curr_rexx, 0, sizeof(curr_rexx));
			ClearMem(&curr_rexx, sizeof(curr_rexx));
			curr_rexx.sn_Flags     = sn->sn_Flags;
			curr_rexx.sn_Port      = sn->sn_Port;
			curr_rexx.sn_Proxy     = sn->sn_Proxy;
			curr_rexx.sn_HotList   = sn->sn_HotList;
			curr_rexx.sn_Anonymous = sn->sn_Anonymous;

			if (sn->sn_RemoteDir   ) curr_rexx.sn_RemoteDir     = strdup(sn->sn_RemoteDir);
			if (sn->sn_LocalDir    ) curr_rexx.sn_LocalDir      = strdup(sn->sn_LocalDir);
			if (sn->sn_DirString   ) curr_rexx.sn_DirString     = strdup(sn->sn_DirString);
			if (sn->sn_LoginName   ) curr_rexx.sn_LoginName     = strdup(sn->sn_LoginName);
			if (sn->sn_Password    ) curr_rexx.sn_Password      = strdup(sn->sn_Password);
			if (sn->sn_SiteAddress ) curr_rexx.sn_SiteAddress   = strdup(sn->sn_SiteAddress);
			if (sn->sn_Node.ln_Name) curr_rexx.sn_Node.ln_Name  = strdup(sn->sn_Node.ln_Name);
		}
		else {
			CurrentState.CurrentSite[0] = 0;
			CurrentState.LastLVSite[0]  = 0;

			if (curr_rexx.sn_Node.ln_Name) {
				free(curr_rexx.sn_Node.ln_Name);
				curr_rexx.sn_Node.ln_Name=0;
			}
			if (sn->sn_RemoteDir)
				Strlcpy(CurrentState.CurrentRemoteDir, sn->sn_RemoteDir, sizeof(CurrentState.CurrentRemoteDir));
			else
				CurrentState.CurrentRemoteDir[0]=0;
			if (sn->sn_LocalDir)
				Strlcpy(CurrentState.CurrentDLDir, sn->sn_LocalDir, sizeof(CurrentState.CurrentDLDir));
			if (sn->sn_LoginName)
				Strlcpy(CurrentState.LoginName, sn->sn_LoginName, sizeof(CurrentState.LoginName));
			else
				CurrentState.LoginName[0]=0;
			if (sn->sn_Password)
				Strlcpy(CurrentState.Password, sn->sn_Password, sizeof(CurrentState.Password));
			else
				CurrentState.Password[0]=0;
			if (sn->sn_Node.ln_Name)
				Strlcpy(CurrentState.LastLVSite, sn->sn_Node.ln_Name, sizeof(CurrentState.LastLVSite));
			if (sn->sn_SiteAddress)
				Strlcpy(CurrentState.CurrentSite, sn->sn_SiteAddress, sizeof(CurrentState.CurrentSite));

			CurrentState.Proxy = sn->sn_Proxy;
			CurrentState.Port  = sn->sn_Port;
			if (sn->sn_Anonymous) CurrentState.LoginName[0]=0;
		}

		ac->ac_RC=RC_OK;
	}
	else {
		ac->ac_RC=RC_WARN;
	}
}

static void rexx_PutFile(struct ARexxCmd *ac, struct RexxMsg *rexxmsg)
{
	struct ArgStruct {
		long ascii;
		long bin;
		char *file;
		char *remote;
	} *args=(void *)ac->ac_ArgList;
	struct List UploadList;
	struct Node *node;
	struct dirlist *entry;

	if (!connected) {
		ac->ac_RC=RC_WARN;
		return;
	}

	if ((entry=new_direntry(args->file,args->file, NULL, NULL, NULL, S_IFREG, 0))) {
		if ((node=AllocListBrowserNode(1, LBNA_UserData,entry, LBNA_Column,0,
		                               LBNCA_Text,entry->name, LBNA_Selected,TRUE, TAG_DONE))) {
			node->ln_Name=(void *)entry;
			AddTail(&UploadList, node);
		}
		else { free_direntry(entry); }
	}

	if (GetHead(&UploadList)!=NULL) {
		if (UploadFile(&UploadList, args->remote, args->ascii?ASCII:BINARY)==TRANS_OK) {
			ac->ac_RC=RC_OK;
		}
		else { ac->ac_RC=RC_WARN; }
	}

	free_dirlist(&UploadList);
}

static void rexx_MPutFile(struct ARexxCmd *ac, struct RexxMsg *rexxmsg)
{
	struct ArgStruct {
		long ascii;
		long bin;
		char **files;
	} *args=(void *)ac->ac_ArgList;
	struct List UploadList;
	struct Node *node;
	struct dirlist *entry;
	int i=0;

	if (!connected) {
		ac->ac_RC=RC_WARN;
		return;
	}

	NewList(&UploadList);

	while(args->files[i]) {
		if ((entry=new_direntry(args->files[i],args->files[i], NULL, NULL, NULL, S_IFREG, 0))) {
			if ((node=AllocListBrowserNode(1, LBNA_UserData,entry, LBNA_Column,0,
			                               LBNCA_Text,entry->name, LBNA_Selected,TRUE, TAG_DONE))) {
				node->ln_Name=(void *)entry;
				AddTail(&UploadList, node);
			}
			else { free_direntry(entry); }
		}
		i++;
	}

	if (GetHead(&UploadList)) {
		if (UploadFile(&UploadList, NULL, args->ascii?ASCII:BINARY)==TRANS_OK) {
			ac->ac_RC=RC_OK;
		}
		else { ac->ac_RC=RC_WARN; }

		free_dirlist(&UploadList);
	}

}

static void rexx_SetAttr(struct ARexxCmd *ac, struct RexxMsg *rexxmsg)
{
	struct ArgStruct {
		char *host;
		char *port;
		char *proxyhost;
		char *proxyport;
		long useproxy;
		char *remotedir;
		char *localdir;
		char *username;
		char *password;
		long anonymous;
		char *quiet;
	}*args = (void *)ac->ac_ArgList;
DBUG("rexx_SetAttr()\n",NULL);
	if (args->host) {
		Strlcpy(CurrentState.CurrentSite, args->host, sizeof(CurrentState.CurrentSite));
		CurrentState.LastLVSite[0]=0;
	}

	if (args->port) {
		if (isalpha(args->port[0])) CurrentState.Port=atol(args->port);
	}

//	if (args->proxyhost);
//	if (args->proxyport);

	if (args->useproxy) { CurrentState.Proxy=1; }

	if (args->remotedir) {
		Strlcpy(CurrentState.CurrentRemoteDir, args->remotedir, sizeof(CurrentState.CurrentRemoteDir));
	}
	if (args->localdir) {
		Strlcpy(CurrentState.CurrentDLDir, args->localdir, sizeof(CurrentState.CurrentDLDir));
		UpdateLocalDir(CurrentState.CurrentDLDir);
	}

	if (args->username) {
		Strlcpy(CurrentState.LoginName, args->username, sizeof(CurrentState.LoginName));
	}

	if (args->password) {
		Strlcpy(CurrentState.Password, args->password, sizeof(CurrentState.Password));
	}

	/*if (args->quiet) {
		if (Strnicmp(args->quiet, "TRUE", 4)==0) SilentMode=TRUE;
		else if (Strnicmp(args->quiet, "FALSE", 5)==0) SilentMode=FALSE;
	}*/
	if (args->quiet) { SilentMode=TRUE; }
	else { SilentMode=FALSE; }

	ac->ac_RC=RC_OK;
}

static int VARARGS68K SetStemVar(struct RexxMsg *rexxmsg, char *value, char *stemname,...)
{
	char Name[256];
	va_list ap;
	APTR args;
//DBUG("SetStemVar()\n",NULL);
	va_startlinear(ap, stemname);
	args = va_getlinearva(ap, APTR);

	VSNPrintf(Name, sizeof(Name), stemname, args);

	va_end(ap);
DBUG("\t%s = %s\n",Name,value);
	//return SetRexxVar(rexxmsg, Name, value, strlen(value));
	return SetRexxVarFromMsg(Name, value, rexxmsg);
}

static void rexx_GetAttr(struct ARexxCmd *ac, struct RexxMsg *rexxmsg)
{
	struct ArgStruct {
		char *stem;
		long hotlist;
		long filelist;
	} *args = (void *)ac->ac_ArgList;
	char buff[100];
	long err=0;
//DBUG("rexx_GetAttr() '%s' %ld %ld\n",args->stem,args->hotlist,args->filelist);
	strupr(args->stem);

	err|=SetStemVar(rexxmsg, (STRPTR)VERS" ("__AMIGADATE__")",
	                "%s.VERSION", args->stem);
	err|=SetStemVar(rexxmsg, MainPrefs.mp_PubScreen? MainPrefs.mp_PubScreen : "",
	                "%s.SCREEN", args->stem);
	err|=SetStemVar(rexxmsg, CurrentState.CurrentSite[0]? CurrentState.CurrentSite : "",
	                "%s.HOST", args->stem);
	SNPrintf(buff, sizeof(buff), "%ld", CurrentState.Port);
	err|=SetStemVar(rexxmsg, buff,
	                "%s.PORT", args->stem);
	err|=SetStemVar(rexxmsg, MainPrefs.mp_ProxyServer? MainPrefs.mp_ProxyServer : "",
	                "%s.PROXYHOST", args->stem);
	SNPrintf(buff, sizeof(buff), "%ld", MainPrefs.mp_ProxyPort);
	err|=SetStemVar(rexxmsg, buff,
	                "%s.PROXYPORT", args->stem);
	err|=SetStemVar(rexxmsg, CurrentState.Proxy? "1" : "0",
	                "%s.USEPROXY", args->stem);
	err|=SetStemVar(rexxmsg, CurrentState.LoginName[0]? "0" : "1",
	                "%s.ANONYMOUS", args->stem);
	err|=SetStemVar(rexxmsg, CurrentState.LoginName[0]? CurrentState.LoginName : "",
	                "%s.USERNAME", args->stem);
	err|=SetStemVar(rexxmsg, CurrentState.CurrentDLDir[0]? CurrentState.CurrentDLDir : "",
	                "%s.LOCALDIR", args->stem);
	err|=SetStemVar(rexxmsg, CurrentState.CurrentRemoteDir[0]? CurrentState.CurrentRemoteDir : "",
	                "%s.REMOTEDIR", args->stem);
	err|=SetStemVar(rexxmsg, TCPStack? ping_server()?"0":"1" : "0",
	                "%s.CONNECTED", args->stem);
	err|=SetStemVar(rexxmsg, SilentMode? "1" : "0",
	                "%s.QUIET", args->stem);

	if (args->hotlist) {
		int i = 0;
		struct SiteNode *sn;
		struct Node *lbn;
DBUG("HOTLIST:\n");
		for (lbn=GetHead(&SiteList); lbn!=NULL; lbn=GetSucc(lbn)) {
			GetListBrowserNodeAttrs(lbn, LBNA_UserData,&sn, TAG_DONE);
//DBUG("HL:0x%08lx   MT:0x%08lx   BL:0x%08lx\n",sn->sn_HotList,sn->sn_MenuType,sn->sn_BarLabel);
			if(!sn->sn_BarLabel  &&  !sn->sn_MenuType) {
				err|=SetStemVar(rexxmsg, sn->sn_Node.ln_Name,
				                "%s.HOTLIST.%ld.NAME", args->stem, i);
				err|=SetStemVar(rexxmsg, sn->sn_SiteAddress,
				                "%s.HOTLIST.%ld.ADDRESS", args->stem, i);
				SNPrintf(buff, sizeof(buff), "%ld", sn->sn_Port);
				err|=SetStemVar(rexxmsg, buff,
				                "%s.HOTLIST.%ld.PORT", args->stem, i);
				err|=SetStemVar(rexxmsg, sn->sn_LoginName? sn->sn_LoginName : "",
					               "%s.HOTLIST.%ld.USERNAME", args->stem, i);
				/*err|=SetStemVar(rexxmsg, sn->sn_Password? sn->sn_Password : "",
				                "%s.HOTLIST.%ld.PASSWORD", args->stem, i);*/
				err|=SetStemVar(rexxmsg, sn->sn_LoginName? "0" : "1",
				                "%s.HOTLIST.%ld.ANONYMOUS", args->stem, i);
				err|=SetStemVar(rexxmsg, sn->sn_RemoteDir?sn->sn_RemoteDir : "",
				                "%s.HOTLIST.%ld.REMOTEDIR", args->stem, i);
				err|=SetStemVar(rexxmsg, sn->sn_LocalDir? sn->sn_LocalDir : "",
				                "%s.HOTLIST.%ld.LOCALDIR", args->stem, i);
				err|=SetStemVar(rexxmsg, sn->sn_Proxy? "1" : "0",
				                "%s.HOTLIST.%ld.USEPROXY", args->stem, i);
				++i;
			}
		}

		SNPrintf(buff, sizeof(buff), "%ld", i);
		err|=SetStemVar(rexxmsg, buff,
		                "%s.HOTLIST.COUNT", args->stem);
	}

	if (args->filelist) {
		int i = 0;
		struct dirlist *curr;
		struct Node *node;
DBUG("FILELIST:\n");
		if (FileList)
			for (node=GetHead(FileList); node!=NULL; node=GetSucc(node),i++) {
				curr = (struct dirlist *)node->ln_Name;
//DBUG("Rexx: %s %ld\n", curr->name, curr->size);
				err|=SetStemVar(rexxmsg, curr->name,
				                "%s.FILELIST.%ld.NAME", args->stem, i);
				SNPrintf(buff, sizeof(buff), "%lld", curr->size);
				err|=SetStemVar(rexxmsg, buff,
				                "%s.FILELIST.%ld.SIZE", args->stem, i);
				err|=SetStemVar(rexxmsg, S_ISDIR(curr->mode)? "DIR" : S_ISLNK(curr->mode)?"LINK":"FILE",
				                "%s.FILELIST.%ld.TYPE", args->stem, i);
			}

		SNPrintf(buff, sizeof(buff), "%ld", i);
		err|=SetStemVar(rexxmsg, buff,
		                "%s.FILELIST.COUNT", args->stem);
	}

	if (err) ac->ac_RC=RC_WARN;
	else ac->ac_RC=RC_OK;

	ac->ac_RC2=err;
}

static void rexx_Quit(struct ARexxCmd *ac, struct RexxMsg *rexxmsg)
{
	ARexxQuitBit=TRUE;
	ac->ac_RC=RC_OK;
DBUG("rexx_Quit()\n",NULL);
}

static void rexx_Activate(struct ARexxCmd *ac, struct RexxMsg *rexxmsg)
{
	ac->ac_RC=RC_OK;
	if (!MainWindow) {
		MainWindow=(struct Window *)IDoMethod(MainWin_Object, WM_OPEN);//CA_OpenWindow(MainWin_Object);
	}
}

static void rexx_Deactivate(struct ARexxCmd *ac, struct RexxMsg *rexxmsg)
{
	ac->ac_RC=RC_OK;
	//if (CA_Iconify(MainWin_Object))
	if (IDoMethod(MainWin_Object, WM_ICONIFY)) { MainWindow=NULL; }
}

static void rexx_FTPCommand(struct ARexxCmd *ac, struct RexxMsg *rexxmsg)
{
	struct ArgStruct {
		char *command;
	} *args = (void *)ac->ac_ArgList;

	if (connected) {
		char errcode[10];
		command(args->command);
		SNPrintf(errcode, sizeof(errcode), "%ld", code);
		ac->ac_RC=RC_OK;
		//SetRexxVar(rexxmsg, "RC2", errcode, strlen(errcode));
		SetRexxVarFromMsg("RC2", errcode, rexxmsg);
	}
	else { ac->ac_RC=RC_WARN; }
}

enum {
 REXX_GETFILE, REXX_MGETFILE, REXX_PUTFILE, REXX_MPUTFILE,
 REXX_DELETEFILE, REXX_CONNECT, REXX_DISCONNECT, REXX_LCD,
 REXX_CD, REXX_SITE, REXX_QUIT, REXX_GETATTR, REXX_SETATTR,
 REXX_ACTIVATE, REXX_DEACTIVATE, REXX_VIEW, REXX_FTPCOMMAND
};

static struct ARexxCmd ARexxCommands[] = {
    {"GET",        REXX_GETFILE,    &rexx_GetFile,    "ASCII/S,BIN/S,FILE/A,LOCAL", 0UL},
    {"MGET",       REXX_MGETFILE,   &rexx_MGetFile,   "ASCII/S,BIN/S,FILE/M",  0UL},
    {"PUT",        REXX_PUTFILE,    &rexx_PutFile,    "ASCII/S,BIN/S,FILE/A,REMOTE", 0UL},
    {"MPUT",       REXX_MPUTFILE,   &rexx_MPutFile,   "ASCII/S,BIN/S,FILE/M", 0UL},
    {"DELETE",     REXX_DELETEFILE, &rexx_DeleteFile, "FILE/M", 0UL},
    {"CONNECT",    REXX_CONNECT,    &rexx_Connect,    "NOSCAN/S", 0UL},
    {"DISCONNECT", REXX_DISCONNECT, &rexx_Disconnect,  NULL, 0UL},
    {"LCD",        REXX_LCD,        &rexx_LCD,        "PARENT/S,DIR", 0UL},
    {"CD",         REXX_CD,         &rexx_CD,         "NOSCAN/S,PARENT/S,DIR", 0UL},
    {"SITE",       REXX_SITE,       &rexx_Site,       "SITE/A/F", 0UL},
    {"QUIT",       REXX_QUIT,       &rexx_Quit,        NULL, 0UL},
    {"GETATTR",    REXX_GETATTR,    &rexx_GetAttr,    "STEM/A,HOTLIST/S,FILELIST/S", 0UL},
    {"SETATTR",    REXX_SETATTR,    &rexx_SetAttr,    "HOST/K,PORT/K,PROXYHOST/K,PROXYPORT/K,USEPROXY/S,REMOTEDIR/K,LOCALDIR/K,USERNAME/K,PASSWORD/K,ANONYMOUS/S,QUIET/S", 0UL},
    {"ACTIVATE",   REXX_ACTIVATE,   &rexx_Activate,   NULL, 0UL},
    {"DEACTIVATE", REXX_DEACTIVATE, &rexx_Deactivate, NULL, 0UL},
    {"VIEW",       REXX_VIEW,       &rexx_View,       "ASCII/S,BIN/S,FILE/A",0UL},
    {"FTPCOMMAND", REXX_FTPCOMMAND, &rexx_FTPCommand, "COMMAND/F",0UL},
    {NULL,         0,               NULL,             NULL, 0UL}
};

BOOL InitRexx(void)
{
	int i;
	char portname[50];
	//ULONG error;

	if ((ARexx_Object=NewObject(ARexxClass, NULL,//ARexxObject,
	                            AREXX_HostName, CurrentState.RexxPort,
	                            AREXX_Commands, ARexxCommands,
	                            AREXX_NoSlot, TRUE,
	                            //AREXX_ErrorCode, &error,
	                           TAG_DONE))) {
DBUG("InitRexx() '%s'\n",CurrentState.RexxPort);
		return TRUE;
	}
	else {
		for (i=1; i<100; i++) {
			SNPrintf(portname, sizeof(portname), "%s.%ld", CurrentState.RexxPort, i);
			if ((ARexx_Object=NewObject(ARexxClass, NULL,//ARexxObject,
			                            AREXX_HostName, portname,
			                            AREXX_Commands, ARexxCommands,
			                            AREXX_NoSlot, TRUE,
			                           TAG_DONE))) {
				Strlcpy(CurrentState.RexxPort, portname, sizeof(CurrentState.RexxPort));
DBUG("InitRexx() '%s'\n",CurrentState.RexxPort);
				return TRUE;
			}
		}
	}

	return FALSE;
}
