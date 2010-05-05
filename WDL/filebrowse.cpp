// todo: support win7/vista extensions rather than GetOpenFileName? -- merge win7filedialog into here.


#include "filebrowse.h"

#include "win32_utf8.h"
#include "wdlcstring.h"


#ifdef _WIN32
  #define PREF_DIRCH '\\'
  #ifdef _MSC_VER // todo: win7filedialog.cpp support for mingw32
    #define WDL_FILEBROWSE_WIN7VISTAMODE
  #endif
#else
  #define PREF_DIRCH '/'
#endif




#ifdef WDL_FILEBROWSE_WIN7VISTAMODE // win7/vista file dialog support
  #include "win7filedialog.cpp"

  // stuff since win7filedialog.h collides with shlobj.h below
  #define tagSHCONTF tagSHCONTF___
  #define SHCONTF SHCONTF___
  #define SHCONTF_FOLDERS SHCONTF_FOLDERS___
  #define SHCONTF_NONFOLDERS SHCONTF_NONFOLDERS___
  #define SHCONTF_INCLUDEHIDDEN SHCONTF_INCLUDEHIDDEN___
  #define SHCONTF_SHAREABLE SHCONTF_SHAREABLE__
  #define SHCONTF_INIT_ON_FIRST_NEXT SHCONTF_INIT_ON_FIRST_NEXT__
  #define SHCONTF_NETPRINTERSRCH SHCONTF_NETPRINTERSRCH__
  #define SHCONTF_STORAGE SHCONTF_STORAGE__
#endif


#ifdef _WIN32
// include after win7filedialog.*

  #include <commctrl.h>
  #include <shlobj.h>
#endif

#ifndef BIF_NEWDIALOGSTYLE
  #define BIF_NEWDIALOGSTYLE 0x40
#endif


static void WDL_fixfnforopenfn(char *buf)
{
  char *p=buf;
  while (*p) 
  {
    if (*p == '/' || *p == '\\') *p=PREF_DIRCH;
    p++;
  }
#ifdef _WIN32
  if (buf[0] && buf[1] == ':')
  {
    p=buf+2;
    char *op=p;
    while (*p)
    {
      while (p[0]=='\\' && p[1] == '\\') p++;
      *op++ = *p++;
    }
    *op=0;
  }
#endif
}


#ifdef _WIN32
static int CALLBACK WINAPI WDL_BrowseCallbackProc( HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	switch (uMsg)
	{
		case BFFM_INITIALIZED:
				if (lpData && ((char *)lpData)[0]) SendMessage(hwnd,BFFM_SETSELECTION,1,(LPARAM)lpData);
    break;
	}
	return 0;
}
#endif


bool WDL_ChooseDirectory(HWND parent, const char *text, const char *initialdir, char *fn, int fnsize, bool preservecwd)
{
  char olddir[2048];
  GetCurrentDirectory(sizeof(olddir),olddir);
#ifdef _WIN32
  char name[4096];
  lstrcpyn_safe(name,initialdir?initialdir:"",sizeof(name));
  BROWSEINFO bi={parent,NULL, name, text, BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE, WDL_BrowseCallbackProc, (LPARAM)name,};
  LPITEMIDLIST idlist = SHBrowseForFolder( &bi );
  if (idlist) 
  {
    SHGetPathFromIDList( idlist, name );        
    IMalloc *m;
    SHGetMalloc(&m);
    m->Free(idlist);
    lstrcpyn_safe(fn,name,fnsize);
    return true;
  }
  return false;

#else
  bool r = BrowseForDirectory(text,initialdir,fn,fnsize);
  if (preservecwd) SetCurrentDirectory(olddir);
  return r;
#endif
}

static const char *stristr(const char* a, const char* b)
{
  int i;
  int len = strlen(b);
  int n = strlen(a)-len;
  for (i = 0; i <= n; ++i)
    if (!strnicmp(a+i, b, len)) 
      return a+i;
  return NULL;
}

bool WDL_ChooseFileForSave(HWND parent, 
                                      const char *text, 
                                      const char *initialdir, 
                                      const char *initialfile, 
                                      const char *extlist,
                                      const char *defext,
                                      bool preservecwd,
                                      char *fn, 
                                      int fnsize,
                                      const char *dlgid,
                                      void *dlgProc,
#ifdef _WIN32
                                      HINSTANCE hInstance
#else
                                      struct SWELL_DialogResourceIndex *reshead
#endif
                                      )
{
  char cwd[2048];
  GetCurrentDirectory(sizeof(cwd),cwd);

#ifdef _WIN32
  char temp[4096];
  memset(temp,0,sizeof(temp));
  if (initialfile) lstrcpyn_safe(temp,initialfile,sizeof(temp));
  WDL_fixfnforopenfn(temp);

#ifdef WDL_FILEBROWSE_WIN7VISTAMODE
  {
    Win7FileDialog fd(text, 1);
    if(fd.inited())
    {
      fd.addOptions(FOS_DONTADDTORECENT);
      //vista+ file open dialog
      char olddir[2048];
      GetCurrentDirectory(sizeof(olddir),olddir);

      fd.setFilterList(extlist);
      if (defext) 
      {
        fd.setDefaultExtension(defext);

        int i = 0;
        const char *p = extlist;
        while(*p)
        {
          if(*p) p+=strlen(p)+1;
          if(!*p) break;
          if(stristr(p, defext)) 
          {
            fd.setFileTypeIndex(i+1);
            break;
          }
          i++;
          p+=strlen(p)+1;
        }
      }
      fd.setFolder(initialdir?initialdir:olddir, 0);
      if(initialfile) 
      {
        //check for folder name
        char *p = temp+strlen(temp);
        while(p>temp && *p!='/' && *p!='\\') p--;
        if(*p=='/'||*p=='\\')
        {
          //folder found
          *p=0;
          fd.setFolder(temp, 0);
          fd.setFilename(p+1);
        }
        else
          fd.setFilename(temp);
      }
      fd.setTemplate(hInstance, dlgid, (LPOFNHOOKPROC)dlgProc);
      
      if(fd.show(parent))
      {
        //ifilesavedialog saves the last folder automatically
        fd.getResult(fn, fnsize);
        
        if (preservecwd) SetCurrentDirectory(olddir);
        return true;
      }
      
      if (preservecwd) SetCurrentDirectory(olddir);
      return NULL;
    }
  }
#endif


  OPENFILENAME l={sizeof(l),parent, hInstance, extlist, NULL,0, 0, temp, sizeof(temp)-1, NULL, 0, initialdir&&initialdir[0] ? initialdir : cwd, text, 
                  OFN_HIDEREADONLY|OFN_EXPLORER|OFN_OVERWRITEPROMPT,0,0,defext, 0, (LPOFNHOOKPROC)dlgProc, dlgid};

  if (hInstance&&dlgProc&&dlgid) l.Flags |= OFN_ENABLEHOOK|OFN_ENABLETEMPLATE|OFN_ENABLESIZING;
  if (preservecwd) l.Flags |= OFN_NOCHANGEDIR;

  if (!GetSaveFileName(&l)||!temp[0]) 
  {
    if (preservecwd) SetCurrentDirectory(cwd);
    return false;
  }
  if (preservecwd) SetCurrentDirectory(cwd);
  lstrcpyn_safe(fn,temp,fnsize);
  return true;

#else
  BrowseFile_SetTemplate(dlgid,(DLGPROC)dlgProc,reshead);
  char if_temp[4096];
  if (initialfile) 
  {
    lstrcpyn_safe(if_temp,initialfile,sizeof(if_temp));
    WDL_fixfnforopenfn(if_temp);
    initialfile = if_temp;
  }

  bool r = BrowseForSaveFile(text,initialdir,initialfile,extlist,fn,fnsize);

  if (preservecwd) SetCurrentDirectory(cwd);

  return r;
#endif
}


char *WDL_ChooseFileForOpen(HWND parent,
                                        const char *text, 
                                        const char *initialdir,  
                                        const char *initialfile, 
                                        const char *extlist,
                                        const char *defext,

                                        bool preservecwd,
                                        bool allowmul, 

                                        const char *dlgid, 
                                        void *dlgProc, 
#ifdef _WIN32
                                        HINSTANCE hInstance
#else
                                        struct SWELL_DialogResourceIndex *reshead
#endif
                                        )
{
  char olddir[2048];
  GetCurrentDirectory(sizeof(olddir),olddir);

#ifdef _WIN32

#ifdef WDL_FILEBROWSE_WIN7VISTAMODE
  if (!allowmul) // todo : check impl of multiple select, too?
  {
    Win7FileDialog fd(text);
    if(fd.inited())
    {
      //vista+ file open dialog
      fd.addOptions(FOS_FILEMUSTEXIST);
      fd.setFilterList(extlist);
      if (defext) 
      {
        fd.setDefaultExtension(defext);
        
        int i = 0;
        const char *p = extlist;
        while(*p)
        {
          if(*p) p+=strlen(p)+1;
          if(!*p) break;
          if(stristr(p, defext)) 
          {
            fd.setFileTypeIndex(i+1);
            break;
          }
          i++;
          p+=strlen(p)+1;
        }
      }
      fd.setFolder(initialdir?initialdir:olddir, 0);
      fd.setTemplate(hInstance, dlgid, (LPOFNHOOKPROC)dlgProc);
      if(initialfile) 
      {
        char temp[4096];
        lstrcpyn_safe(temp,initialfile,sizeof(temp));
        //check for folder name
        char *p = temp+strlen(temp);
        while(p>temp && *p!='/' && *p!='\\') p--;
        if(*p=='/'||*p=='\\')
        {
          //folder found
          *p=0;
          fd.setFolder(temp, 0);
          fd.setFilename(p+1);
        }
        else
          fd.setFilename(temp);
      }

      if(fd.show(parent))
      {
        char temp[4096];
        temp[0]=0;
        //ifileopendialog saves the last folder automatically
        fd.getResult(temp, sizeof(temp)-1);



        if (preservecwd) SetCurrentDirectory(olddir);
        return temp[0] ? strdup(temp) : NULL;
      }

      if (preservecwd) SetCurrentDirectory(olddir);
      return NULL;
    }
  }
#endif

  int temp_size = allowmul ? 256*1024-1 : 4096-1;
  char *temp = (char *)calloc(temp_size+1,1);

  OPENFILENAME l={sizeof(l), parent, hInstance, extlist, NULL, 0, 0, temp, temp_size, NULL, 0, initialdir, text,
    OFN_HIDEREADONLY|OFN_EXPLORER|OFN_FILEMUSTEXIST,0,0, (char *)(defext ? defext : ""), 0, (LPOFNHOOKPROC)dlgProc, dlgid};

  if (hInstance&&dlgProc&&dlgid) l.Flags |= OFN_ENABLEHOOK|OFN_ENABLETEMPLATE|OFN_ENABLESIZING;
  if (allowmul) l.Flags|=OFN_ALLOWMULTISELECT;
  if (preservecwd) l.Flags|=OFN_NOCHANGEDIR;

  if (initialfile) lstrcpyn_safe(temp,initialfile,temp_size);

  WDL_fixfnforopenfn(temp);

  if (!l.lpstrInitialDir||!l.lpstrInitialDir[0]) l.lpstrInitialDir=olddir;

  int r = GetOpenFileName(&l);
  if (preservecwd) SetCurrentDirectory(olddir);

  if (!r) free(temp);
  return r?temp:NULL;

#else  
  char if_temp[4096];
  if (initialfile) 
  {
    lstrcpyn_safe(if_temp,initialfile,sizeof(if_temp));
    WDL_fixfnforopenfn(if_temp);
    initialfile = if_temp;
  }
  
  // defext support?
  BrowseFile_SetTemplate(dlgid,(DLGPROC)dlgProc,reshead);
  char *ret = BrowseForFiles(text,initialdir,initialfile,allowmul,extlist);
  if (preservecwd) SetCurrentDirectory(olddir);

  return ret;
#endif
}