/*
	名称: OpenDesktop   桌面快开
	版本: v1.0
	用途: 自动检测硬盘上所有分区, 并打开用户桌面目录
	作者: bluer
	邮箱: 905750221@qq.com

	程序原理: 
		检索硬盘上所有分区, 检测该分区是否系统分区, 如果是, 则查找该系统分区是否有没有被排除的用户目录, 
		如果有, 则打开该用户桌面, 如果系统分区没有用户桌面, 则查找非系统分区是否有用户目录, 如果有, 则
		打开该用户桌面

*/


#include <windows.h>
#include <tchar.h>
#include <iostream>
#include <fstream>
#include <map>
#include <deque>
#include <string>
using namespace std;

#ifdef UNICODE
typedef wstring tstring;
typedef wifstream tifstream;
#define tcout std::wcout
#else
typedef string tstring;
typedef ifstream tifstream;
#define tcout std::cout
#endif



class COpenDesktop
{
public:
	COpenDesktop();
	~COpenDesktop();

	INT m_isOpenExplorer;					//是否在打开用户桌面后, 又打开一个新的资源管理器
	INT OpenDesktop();
	INT IsSysDrive(LPCTSTR drive);
	tstring& FindUserFile(const tstring& path);
	INT FindFileExist(LPCTSTR fileName, bool isFolder);
	BOOL ReadSpecialFile();
	TCHAR* Trim(const TCHAR *str);
	tstring& TransformStr(tstring& str, bool tolower = true);
	INT ShellExe(
		TCHAR* operate, TCHAR* file, TCHAR* path,
		TCHAR* parameters, INT show, INT timeout,
		_Out_ HANDLE* hProcess);

private:
	PVOID OldRedirectionValue;
	map<tstring, int>m_fileMap;				//存储系统用户名列表
	deque<tstring> m_sysDrvDesktopPath;		//存储系统分区用户桌面路径
	deque<tstring> m_notSysDrvDesktopPath;	//存储非系统分区用户桌面路径

	static const LPTSTR SYS_FILE[2];		//用于判断是否系统分区的特定文件
	static const PTSTR SYS_USER_FILE[2];	//用户文件夹的父目录
	static const LPTSTR CONFIG_FILE;		//设置文件, 记录了要排除的系统用户名
	static const LPTSTR DEFAULT_SYS_USER;	//默认系统用户
};

const LPTSTR COpenDesktop::CONFIG_FILE = TEXT("桌面快开设置.txt");		//设置文件, 记录了要排除的系统用户名
const LPTSTR COpenDesktop::DEFAULT_SYS_USER = TEXT("administrator");	//默认系统用户

const LPTSTR COpenDesktop::SYS_FILE[2] =
{
	//用于判断是否系统分区的特定文件
	TEXT("Windows\\System32\\cmd.exe"),
	TEXT("Windows\\explorer.exe")
};

const LPTSTR COpenDesktop::SYS_USER_FILE[2] =
{
	//用户文件夹的父目录
	TEXT("Users"),
	TEXT("Documents and Settings")
};

COpenDesktop::COpenDesktop() : 
	OldRedirectionValue(NULL), 
	m_isOpenExplorer(FALSE)			//默认在打开用户桌面后, 不打开一个新的资源管理器
{
	//获取对system32目录操作的权限
	//Wow64DisableWow64FsRedirection(&OldRedirectionValue);
};

COpenDesktop::~COpenDesktop()
{
	//还原对system32目录操作的权限
	//Wow64RevertWow64FsRedirection(OldRedirectionValue);
};

INT COpenDesktop::ShellExe(
	TCHAR* operate, TCHAR* file, TCHAR* path,
	TCHAR* parameters, INT show, INT timeout,
	_Out_ HANDLE* hProcess)
{
	//函数按传进来的参数运行自定义的程序

	//该函数是阻塞型函数
	TCHAR m_path[MAX_PATH + 1] = { 0 };
	if (!_tcscmp(path, TEXT("%temp%"))
		|| !_tcscmp(path, TEXT("%tmp%")))
	{
		GetTempPath(MAX_PATH, m_path);			//返回 %TMP% 目录, 如C:\Users\hello\LOCALS~1\TMP
	}
	else if (!_tcscmp(path, TEXT("%system32%")))
	{
		GetWindowsDirectory(m_path, MAX_PATH + 1);		//取得windows目录,如C:\Windows
		_tcscat_s(m_path, MAX_PATH + 1, TEXT("\\System32"));
	}
	else if(!_tcscmp(path, TEXT("%windir%")))
	{
		GetWindowsDirectory(m_path, MAX_PATH + 1);		//取得windows目录,如C:\Windows
		_tcscpy_s(m_path, MAX_PATH + 1, path);
	}

	SHELLEXECUTEINFO sei;
	// 启动进程 
	ZeroMemory(&sei, sizeof(SHELLEXECUTEINFO));
	sei.cbSize = sizeof(SHELLEXECUTEINFO);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS;
	sei.hwnd = NULL;
	sei.lpVerb = operate;
	sei.lpFile = file;
	sei.lpParameters = parameters;
	sei.lpDirectory = m_path;
	sei.nShow = show;		//SW_HIDE;SW_NORMAL
	sei.hInstApp = NULL;
	ShellExecuteEx(&sei);	//运行进程
	//   加入下面这句就是等待该进程结束 
	if(hProcess)
		*hProcess = sei.hProcess;
	return WaitForSingleObject(sei.hProcess, timeout);
}

tstring& COpenDesktop::FindUserFile(const tstring& path)
{
	//path 应该形如 C:\\Users 或者 C:\\Users\\Documents and Settings
	//函数返回可能的用户目录名,用\n分隔,
	//如一个用户,则bluer\n  如两个用户,如blue\nzlin\n
	//假如找不到特殊的用户目录,且有administrator目录,则返回administrator\n
	//否则返回""

	static tstring retStr;
	retStr.clear();
	tstring desktopPath, fileName;
	bool isUser = false, isAdmin = false;		//isUser是否有用户目录, isAdmin是否有管理员目录
	WIN32_FIND_DATA pNextInfo;	//搜索得到的文件信息将储存在pNextInfo中;
	HANDLE hFile = NULL;
	hFile = FindFirstFile((path + TEXT("\\*.*")).c_str(), &pNextInfo);
	map<tstring, int >::iterator it;;
	do 
	{
		if (*(pNextInfo.cFileName) == TEXT('.'))
			continue;		//除掉. 和.. 目录

		fileName.assign(pNextInfo.cFileName);
		this->TransformStr(fileName);
		if (pNextInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			//用户目录是目录,不是文件
			//验证用户目录下是否存在 "桌面" 或者 desktop 目录
			desktopPath.clear();
			desktopPath = path + TEXT("\\") + pNextInfo.cFileName + TEXT("\\桌面");
			INT zhuomian = this->FindFileExist(desktopPath.c_str(), true);
			desktopPath.clear();
			desktopPath = path + TEXT("\\") + pNextInfo.cFileName + TEXT("\\desktop");
			INT desktop = this->FindFileExist(desktopPath.c_str(), true);
			if (zhuomian || desktop)
			{
				//有 "桌面" 或者 desktop 目录, 则是用户目录或者系统用户目录
				it = m_fileMap.find(fileName);
				if (it == m_fileMap.end())
				{
					//如果文件名匹配不了用户列表,表示这是非系统的目录,即用户目录
					//如果有Desktop或者 "桌面" 目录, 则该目录就是用户目录
					retStr.append(pNextInfo.cFileName);
					retStr.append(TEXT("\n"));
					isUser = true;
				}
				else if (!(it->first.compare(DEFAULT_SYS_USER)) && !isUser)
				{
					//匹配成系统默认用户,则是系统用户目录
					isAdmin = true;
				}
			}

		}
	} while (FindNextFile(hFile, &pNextInfo));

	FindClose(hFile);

	if (isAdmin && !isUser)
	{
		//如果有系统用户目录, 没有个人用户目录, 则返回系统默认用户目录名
		retStr = retStr.assign(DEFAULT_SYS_USER) + TEXT("\n");
	}
	else if (!isUser && !isAdmin)
	{
		//如果没有系统用户目录, 没有个人用户目录, 则返回""
		retStr.assign(TEXT(""));
	}

	return retStr;
}


INT COpenDesktop::OpenDesktop()
{
	//函数用于检测硬盘分区, 并打开桌面目录

	TCHAR Drive[MAX_PATH] = { 0 };
	DWORD len = ::GetLogicalDriveStrings(sizeof(Drive) / sizeof(TCHAR), Drive);
	if (len <= 0)
	{
		std::cout << "找不到所有分区" << endl;
		MessageBox(NULL, TEXT("很遗憾, 找不到硬盘分区"), TEXT("提醒"), MB_OK);
		return FALSE;
	}

	DWORD i = 0;
	tstring path, user, desktoppath;
	//开始逐个分区检查是否有桌面目录
	while (Drive[i])
	{
		tcout << endl << TEXT("开始检查: ") << Drive[i]<<TEXT(" 盘") << endl;
		for (int j = 0;
			j < sizeof(SYS_USER_FILE) / sizeof(SYS_USER_FILE[0]);
			j++)
		{
			path.clear();
			path = path.assign(&Drive[i]) + SYS_USER_FILE[j];
			if (!this->FindFileExist(path.c_str(), true))
				continue;

			//检测该分区中是否有 有效的用户目录
			if ((user = this->FindUserFile(path)).compare(TEXT("")))
			{
				int begin = 0, end = 0;
				while ((end = user.find(TEXT('\n'), begin)) > 0)
				{
					desktoppath = path + TEXT("\\") + user.substr(begin, end - begin) + TEXT("\\");
					if (j == 0)
					{
						//user目录下的用户桌面是desktop
						desktoppath = desktoppath + TEXT("Desktop");
					}
					else if(j == 1)
					{
						//Documents and Settings目录下用户桌面是"桌面"
						desktoppath = desktoppath + TEXT("桌面");
					}
					//根据是否 系统分区 来存储 桌面路径
					if (this->IsSysDrive(&Drive[i]))
						m_sysDrvDesktopPath.push_back(desktoppath);
					else
						m_notSysDrvDesktopPath.push_back(desktoppath);

					tcout << TEXT("找到的桌面是: ") << desktoppath << endl;
					begin = end + 1;
				};
				break;
			}
		}
		i += (_tcslen(&Drive[i]) + 1);		//切换到下一个分区
	}

	tcout << endl;
	//至此, 用户桌面寻找完毕, 开始打开桌面
	//假如系统分区中有桌面目录,则打开, 没有才打开非系统分区的桌面目录

	if (this->m_isOpenExplorer &&
		(this->m_sysDrvDesktopPath.size() > 0 ||
		this->m_notSysDrvDesktopPath.size() > 0))
	{
		this->ShellExe(TEXT("open"), TEXT("explorer.exe"), TEXT("%windir%"),
			TEXT(""), SW_NORMAL, 1000, NULL);
	}

	tstring parameters;
	deque<tstring>::iterator it;
	if (this->m_sysDrvDesktopPath.size() > 0)
	{
		//假如系统分区中有桌面目录,则打开
		for (it = this->m_sysDrvDesktopPath.begin(); 
				it != this->m_sysDrvDesktopPath.end(); 
				it++)
		{
			parameters.assign(*it);
			tcout << TEXT("开始打开桌面: ") << parameters << endl;
			this->ShellExe(TEXT("open"), (TCHAR*)parameters.c_str(), TEXT("%windir%"),
				TEXT(""), SW_NORMAL, 0, NULL);
		}

	}
	else if(this->m_notSysDrvDesktopPath.size() > 0)
	{
		// 系统分区没有桌面目录, 才打开非系统分区的桌面目录
		for (it = this->m_notSysDrvDesktopPath.begin();
		it != this->m_notSysDrvDesktopPath.end();
			it++)
		{
			parameters.assign(*it);
			tcout << TEXT("开始打开桌面: ") << parameters << endl;
			this->ShellExe(TEXT("open"), (TCHAR*)parameters.c_str(), TEXT("%windir%"),
				TEXT(""), SW_NORMAL, 0, NULL);
		}
	}
	else
	{
		//系统分区和非系统分区都没有桌面目录, 进行提醒
		tcout << TEXT("\n\n很遗憾, 找不到用户桌面目录") << endl;
		MessageBox(NULL, TEXT("很遗憾, 找不到用户桌面目录"), TEXT("提醒"), MB_OK);
		return FALSE;
	}
	
	return TRUE;
}

tstring& COpenDesktop::TransformStr(tstring& str, bool tolower /*= true*/)
{
	//函数将传入的str字符串中全部转成大/小写, 
	//tolower为true表示转成小写, 否则转成大写, 
	//函数返回转换成功的字符串

	tstring::iterator it;
	//转换大小写
	for (it = str.begin(); it != str.end(); it++)
	{
		if (tolower)
		{
			//转成小写
			if ('A' <= *it && *it <= 'Z')
				*it = *it + 32;
		}
		else
		{
			//转成大写
			if ('a' <= *it && *it <= 'z')
				*it = *it - 32;
		}
	}
	return str;
}

INT COpenDesktop::IsSysDrive(LPCTSTR drive)
{
	//函数判断传入的drive分区, 是否系统分区
	//drive应该是D:\等
	tstring path(drive);
	for (int i = sizeof(SYS_FILE) / sizeof(SYS_FILE[0]); i; i--)
	{
		path.append(SYS_FILE[i-1]);
		if (!this->FindFileExist(path.c_str(), FALSE))
			return FALSE;
		path.assign(drive);
	}

	return TRUE;
}

INT COpenDesktop::FindFileExist(LPCTSTR fileName, bool isFolder)
{
	//寻找文件/文件夹是否存在, 存在返回true, 否则false
	//fileName是文件/夹的长路径, isFolder表示是否文件夹
	WIN32_FIND_DATA fd = { 0 };
	DWORD dwFilter = FALSE;
	if (isFolder) dwFilter = FILE_ATTRIBUTE_DIRECTORY;
	HANDLE hFind = FindFirstFile(fileName, &fd);
	BOOL bFilter = (FALSE == dwFilter) ? TRUE : (fd.dwFileAttributes & dwFilter);
	BOOL RetValue = ((hFind != INVALID_HANDLE_VALUE) && bFilter) ? TRUE : FALSE;
	FindClose(hFind);
	return RetValue;
}


TCHAR* COpenDesktop::Trim(const TCHAR *str)
{
	//去掉字符串两边的空格, 并返回去掉两边空格后的字符串
	static TCHAR line[MAX_PATH];
	const TCHAR *pbegin;
	TCHAR *p = NULL, *pend = NULL;

	pbegin = str;
	while (*pbegin == ' ')
		pbegin++;

	p = line;
	while (*p = *pbegin) {
		if ((*p == ' ') && (*(p - 1) != ' '))
			pend = p;
		p++; pbegin++;
	}
	if (*(p - 1) != ' ') 
		pend = p;
	*pend = 0;
	return line;

}

BOOL COpenDesktop::ReadSpecialFile()
{
	//函数用户读取设置文件, 并返回是否读取成功

	//打开设置文件
	tifstream rfile(CONFIG_FILE, std::ios::in);
	if (!rfile)
	{
		std::cout << "错误:打不开设置文件" << std::endl;
		return FALSE;
	}
	
	//读文件
	tstring str(100, TEXT('\0'));
	std::cout << "开始读取设置文件\n要排除的系统用户目录如下:" << std::endl;
	int i = 0;
	//读取要排除的系统用户名
	while (getline(rfile, str))		//每次读一行, 以\n  换行符结束
	{
		std::wcout << str.c_str() << std::endl;
		if (*(str.c_str()))
		{
			this->m_fileMap.insert(pair<tstring, int>(Trim(TransformStr(str, true).c_str()), ++i));
		}
	};

	//关闭文件
	rfile.close();
	return TRUE;
}




int main(int argc, char *argv[], char *envp[])
{
	COpenDesktop my;
	for (int i = argc; i; i--)
	{
		if (strcmp(argv[i - 1], "/explorer") == 0)
		{
			my.m_isOpenExplorer = TRUE;
			break;
		}
	}

	//试着字符集, 使wcont能顺利输出字符
	std::wcout.imbue(std::locale("chinese"));
	tcout << TEXT("---++++++---欢迎使用 计算机协会 桌面快开 v1.0---++++++---") << endl << endl;

	//读取设置文件, 设置要排除的系统用户名
	my.ReadSpecialFile();
	//根据设置, 开始检索硬盘分区, 并打开找到的用户桌面
	my.OpenDesktop();
	
	return 0;
}


