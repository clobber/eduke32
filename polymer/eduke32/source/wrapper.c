// this is a wrapper to launch EDuke32 properly from Dukester X 1.5
// gcc -o duke3d_w32.exe wrapper.c

#include <windows.h>
#include <string.h>
#include <stdio.h>

#define ISWS(x) ((x == ' ') || (x == '\t') || (x == '\r') || (x == '\n'))

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow)
{
    int i=0,j=0;
    char CmdLine[1024], sCmdLine[1024], szFileName[255];
    LPTSTR szCmdLine;

    FILE * fp=fopen("wrapper.log","w");
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    strcpy(sCmdLine,lpCmdLine);
    szFileName[0] = '\0';

    while(sCmdLine[i] == ' ') i++;
    while(i < (signed)strlen(sCmdLine))
    {
        if(sCmdLine[i] == '-' && sCmdLine[i+1] == 'n' && sCmdLine[i+2] == 'e' && sCmdLine[i+3] == 't')
        {
            CmdLine[i-1] = '\0';
            i += 5;
            while(sCmdLine[i] != ' ' && i < (signed)strlen(sCmdLine))
            {
                szFileName[j] = sCmdLine[i];
                j++,i++;
            }
            szFileName[j] = '\0';
            break;
        }
        else CmdLine[i] = sCmdLine[i];
        i++;
        CmdLine[i] = '\0';
    }

    if(szFileName[0] != '\0' && CmdLine[0] != '\0') sprintf(sCmdLine,"eduke32.exe %s -rmnet %s",CmdLine,szFileName);
    else if(CmdLine[0] != '\0') sprintf(sCmdLine,"eduke32.exe %s",CmdLine);
    else sprintf(sCmdLine,"eduke32.exe");

    szCmdLine = sCmdLine;

    fprintf(fp,"EDuke32 wrapper for Dukester X v0.02\n");
    fprintf(fp,"Copyright (c) 2006 EDuke32 team\n\n");
    fprintf(fp,"Args passed to wrapper: %s\n",lpCmdLine);
    fprintf(fp,"Final command line: %s",szCmdLine);

    fclose(fp);

    ZeroMemory(&si,sizeof(si));
    ZeroMemory(&pi,sizeof(pi));
    si.cb = sizeof(si);

    printf("Launching EDuke32...\n\nPlease leave this window open for the entire duration of your game.");

    if (!CreateProcess(NULL,szCmdLine,NULL,NULL,0,0,NULL,NULL,&si,&pi)) {
        MessageBox(0,"Failed to start eduke32.exe.", "Failure starting game", MB_OK|MB_ICONSTOP);
        return 1;
    } else WaitForSingleObject(pi.hProcess,INFINITE);

    return 0;
}

