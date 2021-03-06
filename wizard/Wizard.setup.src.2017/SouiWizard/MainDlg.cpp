// MainDlg.cpp : implementation of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MainDlg.h"
#include <shellapi.h>
		
CMainDlg::CMainDlg() : SHostWnd(_T("LAYOUT:XML_MAINWND"))
{
	m_bLayoutInited = FALSE;
}

CMainDlg::~CMainDlg()
{
}

int CMainDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	
	SetMsgHandled(FALSE);
	return 0;
}
typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

LPFN_ISWOW64PROCESS fnIsWow64Process;

BOOL IsWow64()
{
	BOOL bIsWow64 = FALSE;

	//IsWow64Process is not available on all supported versions of Windows.
	//Use GetModuleHandle to get a handle to the DLL that contains the function
	//and GetProcAddress to get a pointer to the function if available.

	fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
		GetModuleHandle(TEXT("kernel32")), "IsWow64Process");

	if (NULL != fnIsWow64Process)
	{
		if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
		{
			//handle error
		}
	}
	return bIsWow64;
}
BOOL CMainDlg::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	//HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\SxS\VS7
	//Common7\IDE\VC\vcprojects	

// 	TCHAR szDir[MAX_PATH];
// 	GetCurrentDirectory(MAX_PATH, szDir);
	m_strWizardDir = SApplication::getSingleton().GetAppDir();

	TCHAR szPath[MAX_PATH];
	if (GetEnvironmentVariable(_T("SOUIPATH"), szPath, MAX_PATH))
	{
		FindChildByName(L"et_SouiPath")->SetWindowText(szPath);		
	}
	else
	{
		GetCurrentDirectory(MAX_PATH, szPath);
		TCHAR *pUp = _tcsrchr(szPath, _T('\\'));
		if (pUp)
		{
			_tcscpy(pUp, _T("\\SOUI"));
			if (GetFileAttributes(szPath) != INVALID_FILE_ATTRIBUTES)
			{
				*pUp = 0;
				FindChildByName(L"et_SouiPath")->SetWindowText(szPath);
			}
		}
	}

	HKEY hKey;
	LSTATUS ec= RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		IsWow64() ? wowkey[0] : wowkey[1],
		0,
		KEY_READ,&hKey);
	if (ec == ERROR_SUCCESS)
	{		
		DWORD dwType = REG_SZ;
		DWORD dwSize=MAX_PATH;
		wchar_t data[MAX_PATH];
		if (ERROR_SUCCESS == RegQueryValueEx(hKey,L"15.0",0,&dwType,(LPBYTE)data,&dwSize))
		{
			//SMessageBox(m_hWnd, data, L"提示", MB_OK | MB_ICONINFORMATION);
			m_vs2017dir = data;
			m_vs2017dir +=LR"(Common7\IDE\VC\vcprojects)";
			FindChildByName(L"btn_Install")->SetAttribute(L"enable", L"1");
			FindChildByName(L"btn_UnInstall")->SetAttribute(L"enable", L"1");
		}
		else
		{
			SMessageBox(m_hWnd, L"未找到VS2017安装位置！", L"警告", MB_OK| MB_ICONWARNING);
		}
		RegCloseKey(hKey);
	}
	else
	{
		SMessageBox(m_hWnd, L"未找到VS2017安装位置！", L"警告", MB_OK | MB_ICONWARNING);
	}
	m_bLayoutInited = TRUE;
	return 0;
}

void CMainDlg::OnInstall()
{
	SetCurrentDirectory(m_strWizardDir);

	if (GetFileAttributes(_T("SouiWizard")) == INVALID_FILE_ATTRIBUTES)
	{
		SMessageBox(m_hWnd,m_strWizardDir+_T("当前目录下没有找到SOUI的向导数据"), _T("错误"), MB_OK | MB_ICONSTOP);
		return ;
	}
	TCHAR szSourCore[MAX_PATH];

	SStringT szSouiDir=FindChildByName(L"et_SouiPath")->GetWindowText();
	szSouiDir.TrimRight(L'\\');
	_tcscpy(szSourCore, szSouiDir);
	_tcscat(szSourCore, _T("\\SOUI"));
	if (GetFileAttributes(szSourCore) == INVALID_FILE_ATTRIBUTES)
	{
		SMessageBox(m_hWnd, _T("当前目录下没有找到SOUI的源代码"), _T("错误"), MB_OK | MB_ICONSTOP);
		return ;
	}
	//设置环境变量

	HKEY envKey;
	if (ERROR_SUCCESS ==RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Control\\Session Manager\\Environment"),0, KEY_SET_VALUE | KEY_QUERY_VALUE,&envKey))
	{
		RegSetValueEx(envKey, _T("SOUIPATH"), 0, REG_SZ,(LPBYTE)(LPCTSTR)szSouiDir, wcslen(szSourCore)*sizeof(TCHAR));
		
		DWORD dwSize = 0;
		DWORD dwType = REG_SZ;
		LONG lRet = RegQueryValueEx(envKey,_T("Path"), NULL,&dwType,NULL, &dwSize);
		if (ERROR_SUCCESS == lRet)
		{//修改path环境变量
			SStringT str;
			TCHAR * pBuf = str.GetBufferSetLength(dwSize);
			lRet = RegQueryValueEx(envKey, _T("Path"), NULL, &dwType, (LPBYTE)pBuf, &dwSize);
			str.ReleaseBuffer();

			SStringT strSouiBin(szSouiDir);
			strSouiBin += _T("\\bin");
			SStringT strSouiBin64(szSouiDir);
			strSouiBin64 += _T("\\bin64");
			SStringT strNewPath=str;
			bool bSet=false;
			if (StrStrI(str, strSouiBin) == NULL)
			{
				bSet = true;
				if (strNewPath.IsEmpty())
					strNewPath = strSouiBin;
				else
					strNewPath = strSouiBin + _T(";") + strNewPath;
			}
			if (IsWow64() && (StrStrI(str, strSouiBin64) == NULL))
			{
				bSet = true;
// 				if (strNewPath.IsEmpty())//最少都应该有strSouiBin了
// 					strNewPath = strSouiBin64;
// 				else
				strNewPath = strSouiBin64 + _T(";") + strNewPath;
			}
			if(bSet)
				RegSetValueEx(envKey, _T("Path"), 0, REG_SZ, (LPBYTE)(LPCTSTR)strNewPath, (strNewPath.GetLength()+1)*sizeof(TCHAR));
		}
		RegCloseKey(envKey);
		DWORD_PTR msgResult = 0;
		//广播环境变量修改消息
		SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, LPARAM(_T("Environment")), SMTO_ABORTIFHUNG, 5000, &msgResult);
	}
	else
	{
		SMessageBox(m_hWnd, _T("添加环境变量失败"), _T("错误"), MB_OK | MB_ICONSTOP);
		return;
	}

	//准备复制文件
	TCHAR szFrom[1024] = { 0 };
	TCHAR szTo[1024] = { 0 };
	SHFILEOPSTRUCT shfo;
	shfo.pFrom = szFrom;
	shfo.pTo = szTo;

	SStringT szVsEntry = m_strWizardDir += LR"(\vs-entry\2017)";
	
		//复制入口数据
		BOOL bOK = TRUE;
		if (bOK)
		{
			shfo.wFunc = FO_COPY;
			shfo.fFlags = FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION;
			memset(szFrom, 0, sizeof(szFrom));
			memset(szTo, 0, sizeof(szTo));
			_tcscpy(szFrom, _T("entry\\*.*"));
			_tcscpy(szTo,m_vs2017dir);
			bOK = 0 == SHFileOperation(&shfo);
		}
		//改写SouiWizard.vsz
		if (bOK)
		{
			_tcscpy(szFrom, szVsEntry);
			_tcscat(szFrom, _T("\\SouiWizard.vsz"));
			_tcscpy(szTo, m_vs2017dir);
			_tcscat(szTo, _T("\\SouiWizard.vsz"));

			CopyFile(szFrom, szTo, FALSE);

			FILE *f = _tfopen(szTo, _T("r"));
			if (f)
			{
				char szBuf[4096];
				int nReaded = fread(szBuf, 1, 4096, f);
				szBuf[nReaded] = 0;
				fclose(f);

				f = _tfopen(szTo, _T("w"));
				if (f)
				{//清空原数据再重新写入新数据
					SStringA str = szBuf;
					str.Replace("%SOUIPATH%", S_CW2A(szSouiDir));
					fwrite((LPCSTR)str, 1, str.GetLength(), f);
					fclose(f);
				}
			}
		}
		SStringT strMsg;
		strMsg.Format(_T("为Vs2017安装SOUI向导:%s"),  bOK ? _T("成功") : _T("失败"));
		SMessageBox(m_hWnd, strMsg, _T("提示"), MB_OK | MB_ICONINFORMATION);
}

void CMainDlg::OnUninstall()
{
	SHFILEOPSTRUCT shfo = { 0 };
	shfo.pTo = NULL;
	shfo.wFunc = FO_DELETE;
	shfo.fFlags = FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION | FOF_SILENT;

		//remove entry files
		SStringT strSource = m_vs2017dir + _T("\\SouiWizard.ico");
		BOOL bOK = DeleteFile(strSource);
		if (bOK)
		{
			strSource = m_vs2017dir + _T("\\SouiWizard.vsdir");
			bOK = DeleteFile(strSource);
		}
		if (bOK)
		{
			strSource = m_vs2017dir + _T("\\SouiWizard.vsz");
			bOK = DeleteFile(strSource);
		}
		SStringT strMsg;		
		strMsg.Format(_T("从Vs2017卸载SOUI向导:%s"), bOK ? _T("成功") : _T("失败"));
		SMessageBox(m_hWnd, strMsg, _T("提示"), MB_OK | MB_ICONINFORMATION);
}
//TODO:消息映射
void CMainDlg::OnClose()
{
	CSimpleWnd::DestroyWindow();
}

void CMainDlg::OnMaximize()
{
	SendMessage(WM_SYSCOMMAND, SC_MAXIMIZE);
}
void CMainDlg::OnRestore()
{
	SendMessage(WM_SYSCOMMAND, SC_RESTORE);
}
void CMainDlg::OnMinimize()
{
	SendMessage(WM_SYSCOMMAND, SC_MINIMIZE);
}

