/*
	����: OpenDesktop   ����쿪
	�汾: v1.0
	��;: �Զ����Ӳ�������з���, �����û�����Ŀ¼
	����: bluer
	����: 905750221@qq.com
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
	map<tstring, int>m_fileMap;				//�洢ϵͳ�û����б�
	deque<tstring> m_sysDrvDesktopPath;		//�洢ϵͳ�����û�����·��
	deque<tstring> m_notSysDrvDesktopPath;	//�洢��ϵͳ�����û�����·��

	static const LPTSTR SYS_FILE[2];
	static const PTSTR SYS_USER_FILE[2];
	static const LPTSTR CONFIG_FILE;
	static const LPTSTR DEFAULT_SYS_USER;	//Ĭ��ϵͳ�û�
};

const LPTSTR COpenDesktop::CONFIG_FILE = TEXT("����쿪����.txt");
const LPTSTR COpenDesktop::DEFAULT_SYS_USER = TEXT("administrator");	//Ĭ��ϵͳ�û�

const LPTSTR COpenDesktop::SYS_FILE[2] =
{
	TEXT("Windows\\System32\\cmd.exe"),
	TEXT("Windows\\explorer.exe")
};

const LPTSTR COpenDesktop::SYS_USER_FILE[2] =
{
	TEXT("Users"),
	TEXT("Documents and Settings")
};

COpenDesktop::COpenDesktop() : OldRedirectionValue(NULL)
{
	//��ȡ��system32Ŀ¼������Ȩ��
	//Wow64DisableWow64FsRedirection(&OldRedirectionValue);
};

COpenDesktop::~COpenDesktop()
{
	//��ԭ��system32Ŀ¼������Ȩ��
	//Wow64RevertWow64FsRedirection(OldRedirectionValue);
};

INT COpenDesktop::ShellExe(
	TCHAR* operate, TCHAR* file, TCHAR* path,
	TCHAR* parameters, INT show, INT timeout,
	_Out_ HANDLE* hProcess)
{
	//�ú����������ͺ���
	TCHAR m_path[MAX_PATH + 1] = { 0 };
	if (!_tcscmp(path, TEXT("%temp%"))
		|| !_tcscmp(path, TEXT("%tmp%")))
	{
		GetTempPath(MAX_PATH, m_path);			//���� %TMP% Ŀ¼, ��C:\Users\hello\LOCALS~1\TMP
	}
	else if (!_tcscmp(path, TEXT("%system32%")))
	{
		GetWindowsDirectory(m_path, MAX_PATH + 1);		//ȡ��windowsĿ¼,��C:\Windows
		_tcscat_s(m_path, MAX_PATH + 1, TEXT("\\System32"));
	}
	else if(!_tcscmp(path, TEXT("%windir%")))
	{
		GetWindowsDirectory(m_path, MAX_PATH + 1);		//ȡ��windowsĿ¼,��C:\Windows
		_tcscpy_s(m_path, MAX_PATH + 1, path);
	}

	SHELLEXECUTEINFO sei;
	// �������� 
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
	ShellExecuteEx(&sei);
	//   �������������ǵȴ��ý��̽��� 
	if(hProcess)
		*hProcess = sei.hProcess;
	return WaitForSingleObject(sei.hProcess, timeout);
}

tstring& COpenDesktop::FindUserFile(const tstring& path)
{
	//path Ӧ������ C:\\Users ���� C:\\Users\\Documents and Settings
	//�������ؿ��ܵ��û�Ŀ¼��,��\n�ָ�,
	//��һ���û�,��bluer\n  �������û�,��blue\nzlin\n
	//�����Ҳ���������û�Ŀ¼,����administratorĿ¼,�򷵻�administrator\n
	//���򷵻�""

	static tstring retStr;
	retStr.clear();
	tstring desktopPath, fileName;
	bool isUser = false, isAdmin = false;		//isUser�Ƿ����û�Ŀ¼, isAdmin�Ƿ��й���ԱĿ¼
	WIN32_FIND_DATA pNextInfo;	//�����õ����ļ���Ϣ��������pNextInfo��;
	HANDLE hFile = NULL;
	hFile = FindFirstFile((path + TEXT("\\*.*")).c_str(), &pNextInfo);
	map<tstring, int >::iterator it;;
	do 
	{
		if (*(pNextInfo.cFileName) == TEXT('.'))
			continue;		//����. ��.. Ŀ¼

		fileName.assign(pNextInfo.cFileName);
		this->TransformStr(fileName);
		if (pNextInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			//�û�Ŀ¼��Ŀ¼,�����ļ�
			//��֤�û�Ŀ¼���Ƿ���� "����" ���� desktop Ŀ¼
			desktopPath.clear();
			desktopPath = path + TEXT("\\") + pNextInfo.cFileName + TEXT("\\����");
			INT zhuomian = this->FindFileExist(desktopPath.c_str(), true);
			desktopPath.clear();
			desktopPath = path + TEXT("\\") + pNextInfo.cFileName + TEXT("\\desktop");
			INT desktop = this->FindFileExist(desktopPath.c_str(), true);
			if (zhuomian || desktop)
			{
				//�� "����" ���� desktop Ŀ¼, �����û�Ŀ¼����ϵͳ�û�Ŀ¼
				it = m_fileMap.find(fileName);
				if (it == m_fileMap.end())
				{
					//����ļ���ƥ�䲻���û��б�,��ʾ���Ƿ�ϵͳ��Ŀ¼,���û�Ŀ¼
					//�����Desktop���� "����" Ŀ¼, ���Ŀ¼�����û�Ŀ¼
					retStr.append(pNextInfo.cFileName);
					retStr.append(TEXT("\n"));
					isUser = true;
				}
				else if (!(it->first.compare(DEFAULT_SYS_USER)) && !isUser)
				{
					//ƥ���ϵͳĬ���û�,����ϵͳ�û�Ŀ¼
					isAdmin = true;
				}
			}

		}
	} while (FindNextFile(hFile, &pNextInfo));

	FindClose(hFile);

	if (isAdmin && !isUser)
	{
		//�����ϵͳ�û�Ŀ¼, û�и����û�Ŀ¼, �򷵻�ϵͳĬ���û�Ŀ¼��
		retStr = retStr.assign(DEFAULT_SYS_USER) + TEXT("\n");
	}
	else if (!isUser && !isAdmin)
	{
		//���û��ϵͳ�û�Ŀ¼, û�и����û�Ŀ¼, �򷵻�""
		retStr.assign(TEXT(""));
	}

	return retStr;
}


INT COpenDesktop::OpenDesktop()
{
	TCHAR Drive[MAX_PATH] = { 0 };
	DWORD len = ::GetLogicalDriveStrings(sizeof(Drive) / sizeof(TCHAR), Drive);
	if (len <= 0)
	{
		std::cout << "�Ҳ������з���" << endl;
		MessageBox(NULL, TEXT("���ź�, �Ҳ���Ӳ�̷���"), TEXT("����"), MB_OK);
		return FALSE;
	}

	DWORD i = 0;
	tstring path, user, desktoppath;
	while (Drive[i])
	{
		tcout << endl << TEXT("��ʼ���: ") << Drive[i]<<TEXT(" ��") << endl;
		for (int j = 0;
			j < sizeof(SYS_USER_FILE) / sizeof(SYS_USER_FILE[0]);
			j++)
		{
			path.clear();
			path = path.assign(&Drive[i]) + SYS_USER_FILE[j];
			if (!this->FindFileExist(path.c_str(), true))
				continue;

			if ((user = this->FindUserFile(path)).compare(TEXT("")))
			{
				int begin = 0, end = 0;
				while ((end = user.find(TEXT('\n'), begin)) > 0)
				{
					desktoppath = path + TEXT("\\") + user.substr(begin, end - begin) + TEXT("\\");
					if (j == 0)
					{
						//userĿ¼�µ��û�������desktop
						desktoppath = desktoppath + TEXT("Desktop");
					}
					else if(j == 1)
					{
						//Documents and SettingsĿ¼���û�������"����"
						desktoppath = desktoppath + TEXT("����");
					}
					//�����Ƿ�ϵͳ�����洢����·��
					if (this->IsSysDrive(&Drive[i]))
						m_sysDrvDesktopPath.push_back(desktoppath);
					else
						m_notSysDrvDesktopPath.push_back(desktoppath);

					tcout << TEXT("�ҵ���������: ") << desktoppath << endl;
					begin = end + 1;
				};
				break;
			}
		}
		i += (_tcslen(&Drive[i]) + 1);		//�л�����һ������
	}

	tcout << endl;
	//����, �û�����Ѱ�����, ��ʼ������
	tstring parameters;
	deque<tstring>::iterator it;
	if (this->m_sysDrvDesktopPath.size() > 0)
	{
		for (it = this->m_sysDrvDesktopPath.begin(); 
				it != this->m_sysDrvDesktopPath.end(); 
				it++)
		{
			parameters.assign(*it);
			tcout << TEXT("��ʼ������: ") << parameters << endl;
			this->ShellExe(TEXT("open"), (TCHAR*)parameters.c_str(), TEXT("%windir%"),
				TEXT(""), SW_NORMAL, 0, NULL);
		}

	}
	else if(this->m_notSysDrvDesktopPath.size() > 0)
	{
		for (it = this->m_notSysDrvDesktopPath.begin();
		it != this->m_notSysDrvDesktopPath.end();
			it++)
		{
			parameters.assign(*it);
			tcout << TEXT("��ʼ������: ") << parameters << endl;
			this->ShellExe(TEXT("open"), (TCHAR*)parameters.c_str(), TEXT("%windir%"),
				TEXT(""), SW_NORMAL, 0, NULL);
		}
	}
	else
	{
		tcout << TEXT("\n\n���ź�, �Ҳ����û�����Ŀ¼") << endl;
		MessageBox(NULL, TEXT("���ź�, �Ҳ����û�����Ŀ¼"), TEXT("����"), MB_OK);
		return FALSE;
	}

	return TRUE;
}

tstring& COpenDesktop::TransformStr(tstring& str, bool tolower /*= true*/)
{
	tstring::iterator it;
	//ת����Сд
	for (it = str.begin(); it != str.end(); it++)
	{
		if (tolower)
		{
			//ת��Сд
			if ('A' <= *it && *it <= 'Z')
				*it = *it + 32;
		}
		else
		{
			//ת�ɴ�д
			if ('a' <= *it && *it <= 'z')
				*it = *it - 32;
		}
	}
	return str;
}

INT COpenDesktop::IsSysDrive(LPCTSTR drive)
{
	//driveӦ����D:\��
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
	//ȥ���ַ������ߵĿո�
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
	//���ļ�
	tifstream rfile(CONFIG_FILE, std::ios::in);
	if (!rfile)
	{
		std::cout << "����:�򲻿������ļ�" << std::endl;
		return FALSE;
	}
	
	//���ļ�
	tstring str(100, TEXT('\0'));
	std::cout << "��ʼ��ȡ�����ļ�\nҪ�ų���ϵͳ�û�Ŀ¼����:" << std::endl;
	int i = 0;
	while (getline(rfile, str))		//ÿ�ζ�һ��, ��\n  ���з�����
	{
		std::wcout << str.c_str() << std::endl;
		if (*(str.c_str()))
		{
			this->m_fileMap.insert(pair<tstring, int>(Trim(TransformStr(str, true).c_str()), ++i));
		}
	};

	//�ر��ļ�
	rfile.close();
	return TRUE;
}




void main()
{
	std::wcout.imbue(std::locale("chinese"));
	tcout << TEXT("---++++++---��ӭʹ�� �����Э�� ����쿪 v1.0---++++++---") << endl << endl;
	COpenDesktop my;
	my.ReadSpecialFile();
	my.OpenDesktop();
}


