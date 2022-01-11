
//
// created by AheadLib
// github:https://github.com/strivexjun/AheadLib-x86-x64
//
//Add PRINT_DEBUG from goldberg's emulator.
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#define PRINT_DEBUG(a, ...) do {FILE *t = fopen("Evolve_LOG.txt", "a"); fprintf(t, "%u " a, GetCurrentThreadId(), __VA_ARGS__); fclose(t);} while (0)
//Add detours from goldberg's emulator
#include"detours/detours.h"
#include <winhttp.h>
#include <locale.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <processthreadsapi.h>
#include <windows.h>
#include <direct.h>
#include <iphlpapi.h> // Include winsock2 before this, or winsock2 iphlpapi will be unavailable
#include <shlobj.h>
//FOR INPUT FIX:
#include<imm.h>
#include<cstdio>

#pragma comment (lib ,"imm32.lib")

//#include<winhttp.h>
//#include<libloaderapi.h>
//#include<WS2tcpip.h>

//C++ 
#include<string>
#include<memory>
#include<vector>
#include<tchar.h>
#include<atlstr.h>
#include<fstream>

#include <Shlwapi.h>
#include <WinTrust.h>
using namespace std;

#pragma comment( lib, "detours.lib")
#pragma comment(lib,"ws2_32.lib")
#pragma comment( lib, "Shlwapi.lib")
#pragma comment(linker, "/EXPORT:WinVerifyTrust=AheadLib_WinVerifyTrust,@144")


extern "C" 
{
PVOID pfnAheadLib_WinVerifyTrust;
}
//WinTrust declare
static decltype(WinVerifyTrust)* pfnWinVerifyTrust;
//From Goldberg's emulator:
struct ips_test {
	uint32_t ip_from;
	uint32_t ip_to;
};
inline bool file_exists(const std::string& name) {
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}
//дһ��ȫ�ֱ���
struct sockaddr replaceaddr;
DWORD ProcessID = GetCurrentProcessId();
static std::vector<struct ips_test> adapter_ips;

HMODULE GetSelfModuleHandle()
{
	MEMORY_BASIC_INFORMATION mbi;
	return ((::VirtualQuery(GetSelfModuleHandle, &mbi, sizeof(mbi)) != 0) ? (HMODULE)mbi.AllocationBase : NULL);
}

std::string get_full_program_path(void)
{
	TCHAR path[MAX_PATH] = { 0 };
	memset(path, 0, sizeof(path));
	GetModuleFileName(GetSelfModuleHandle(), path, sizeof(path));
	int l = _tcslen(path);
	TCHAR ch = 0;
	while ('\\' != (ch = *(path + --l)));
	*(path + l + 1) = 0;
	std::string temp = (LPCSTR)(CStringA)(CString(path));
	return temp;
}


void set_adapter_ips(uint32_t* from, uint32_t* to, unsigned num_ips)
{
	adapter_ips.clear();
	for (unsigned i = 0; i < num_ips; ++i) {
		struct ips_test ip_a;
		PRINT_DEBUG("from: %hhu.%hhu.%hhu.%hhu\n", ((unsigned char*)&from[i])[0], ((unsigned char*)&from[i])[1], ((unsigned char*)&from[i])[2], ((unsigned char*)&from[i])[3]);
		PRINT_DEBUG("to: %hhu.%hhu.%hhu.%hhu\n", ((unsigned char*)&to[i])[0], ((unsigned char*)&to[i])[1], ((unsigned char*)&to[i])[2], ((unsigned char*)&to[i])[3]);
		ip_a.ip_from = ntohl(from[i]);
		ip_a.ip_to = ntohl(to[i]);
		if (ip_a.ip_to < ip_a.ip_from) continue;
		if ((ip_a.ip_to - ip_a.ip_from) > (1 << 25)) continue;
		PRINT_DEBUG("added\n");
		adapter_ips.push_back(ip_a);
	}
}

static bool is_adapter_ip(unsigned char* ip)
{
	uint32_t ip_temp = 0;
	memcpy(&ip_temp, ip, sizeof(ip_temp));
	ip_temp = ntohl(ip_temp);

	for (auto& i : adapter_ips) {
		if (i.ip_from <= ip_temp && ip_temp <= i.ip_to) {
			PRINT_DEBUG("ADAPTER IP %hhu.%hhu.%hhu.%hhu\n", ip[0], ip[1], ip[2], ip[3]);
			return true;
		}
	}

	return false;
}
//static bool is_crack_ip(const sockaddr* addr, int namelen)
//{
//
//}




static bool is_lan_ip(const sockaddr* addr, int namelen)
{
	//�������ٴιرյ����뷨��
	ImmDisableIME(GetCurrentThreadId());
	if (!namelen) return false;
	if (addr->sa_family == AF_INET) {
		struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
		unsigned char ip[4];
		memcpy(ip, &addr_in->sin_addr, sizeof(ip));
		PRINT_DEBUG("CHECK LAN IP %hhu.%hhu.%hhu.%hhu:%u\n", ip[0], ip[1], ip[2], ip[3], ntohs(addr_in->sin_port));
		if (is_adapter_ip(ip)) return true;
		if (ip[0] == 127) return true;
		if (ip[0] == 10) return true;
		if (ip[0] == 192 && ip[1] == 168) return true;
		if (ip[0] == 169 && ip[1] == 254 && ip[2] != 0) return true;
		if (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31) return true;
		if ((ip[0] == 100) && ((ip[1] & 0xC0) == 0x40)) return true;
		if (ip[0] == 239) return true; //multicast
		if (ip[0] == 0) return true; //Current network
		if (ip[0] == 192 && (ip[1] == 18 || ip[1] == 19)) return true; //Used for benchmark testing of inter-network communications between two separate subnets.
		if (ip[0] >= 224) return true; //ip multicast (224 - 239) future use (240.0.0.0�C255.255.255.254) broadcast (255.255.255.255)
	}
	else if (addr->sa_family == AF_INET6) {
		struct sockaddr_in6* addr_in6 = (struct sockaddr_in6*)addr;
		unsigned char ip[16];
		unsigned char zeroes[16] = {};
		memcpy(ip, &addr_in6->sin6_addr, sizeof(ip));
		PRINT_DEBUG("CHECK LAN IP6 %hhu.%hhu.%hhu.%hhu.%hhu.%hhu.%hhu.%hhu...%hhu\n", ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7], ip[15]);
		if (((ip[0] == 0xFF) && (ip[1] < 3) && (ip[15] == 1)) ||
			((ip[0] == 0xFE) && ((ip[1] & 0xC0) == 0x80))) return true;
		if (memcmp(zeroes, ip, sizeof(ip)) == 0) return true;
		if (memcmp(zeroes, ip, sizeof(ip) - 1) == 0 && ip[15] == 1) return true;
		if (ip[0] == 0xff) return true; //multicast
		if (ip[0] == 0xfc) return true; //unique local
		if (ip[0] == 0xfd) return true; //unique local
		//TODO: ipv4 mapped?
	}
	if (addr->sa_family == AF_INET) {
		struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
		unsigned char ip[4];
		memcpy(ip, &addr_in->sin_addr, sizeof(ip));
		PRINT_DEBUG("CHECK NON LAN IP %hhu.%hhu.%hhu.%hhu:%u\n", ip[0], ip[1], ip[2], ip[3], ntohs(addr_in->sin_port));
		//�޸���һ���֣�ʹ��ǿ�ж�λ��ĳ��IP��ĳ��Port�������������ʡ�(����ֻ��http��https���Բ��õ��ģ�
		if (file_exists(get_full_program_path() + "serverip.txt"))
		{
			std::ifstream readFile(get_full_program_path() + "serverip.txt");
			//Read and put it in.
			std::string temp;
			readFile >> temp;
			const std::string& tempip = temp;
			//����õ���String����ʱIP�������������IP��취�Ž�ȥ
			addr_in->sin_addr.s_addr = inet_addr(temp.c_str());
			PRINT_DEBUG("Crack 2K DLC Check - IP\n");
		}
		if (file_exists(get_full_program_path() + "serverport.txt"))
		{
			std::ifstream readFile(get_full_program_path() + "serverport.txt");
			std::string temp;
			readFile >> temp;
			int port = atoi(temp.c_str());
			addr_in->sin_port = htons(port);
			PRINT_DEBUG("Crack 2K DLC Check - PORT\n");
		}
		//�޸ĺ󣬽��޸ĵ�ת��sockaddr,ͨ�����¸�ֵ����ָ�����޸�ָ��ָ���ֵ��
		addr = (struct sockaddr*)addr_in;
		//�ǵ����¶�ȡIP��ֵ����
		memcpy(ip, &addr_in->sin_addr, sizeof(ip));
		PRINT_DEBUG("NOW LAN IP %hhu.%hhu.%hhu.%hhu:%u\n", ip[0], ip[1], ip[2], ip[3], ntohs(addr_in->sin_port));
		return true;
	}

	PRINT_DEBUG("NOT LAN IP\n");
	return false;
}


int (WINAPI* Real_SendTo)(SOCKET s, const char* buf, int len, int flags, const sockaddr* to, int tolen) = sendto;
int (WINAPI* Real_Connect)(SOCKET s, const sockaddr* addr, int namelen) = connect;
int (WINAPI* Real_WSAConnect)(SOCKET s, const sockaddr* addr, int namelen, LPWSABUF lpCallerData, LPWSABUF lpCalleeData, LPQOS lpSQOS, LPQOS lpGQOS) = WSAConnect;


// These are hooked ws2_32.dll. The game with ca-bundle.crt will use these.

static int WINAPI Mine_SendTo(SOCKET s, const char* buf, int len, int flags, const sockaddr* to, int tolen) {
	PRINT_DEBUG("Mine_SendTo\n");
	if (is_lan_ip(to, tolen)) {
		return Real_SendTo(s, buf, len, flags, to, tolen);
	}
	else {
		return len;
	}
}

static int WINAPI Mine_Connect(SOCKET s, const sockaddr* addr, int namelen)
{
	PRINT_DEBUG("Mine_Connect\n");
	if (is_lan_ip(addr, namelen)) {
		return Real_Connect(s, addr, namelen);
	}
	else {
		WSASetLastError(WSAECONNREFUSED);
		return SOCKET_ERROR;
	}
}

static int WINAPI Mine_WSAConnect(SOCKET s, const sockaddr* addr, int namelen, LPWSABUF lpCallerData, LPWSABUF lpCalleeData, LPQOS lpSQOS, LPQOS lpGQOS)
{
	PRINT_DEBUG("Mine_WSAConnect\n");
	if (is_lan_ip(addr, namelen)) {
		return Real_WSAConnect(s, addr, namelen, lpCallerData, lpCalleeData, lpSQOS, lpGQOS);
	}
	else {
		WSASetLastError(WSAECONNREFUSED);
		return SOCKET_ERROR;
	}
}

HINTERNET(WINAPI* Real_WinHttpConnect)(
	IN HINTERNET     hSession,
	IN LPCWSTR       pswzServerName,
	IN INTERNET_PORT nServerPort,
	IN DWORD         dwReserved
	);


LPCWSTR stringToLPCWSTR(std::string orig)
{
	//���õ�ǰ����ҳ
	//setlocale(LC_ALL, ".936"); //936:���� 950:����
	size_t origsize = orig.length() + 1;
	const size_t newsize = 100;
	size_t convertedChars = 0;
	wchar_t* wcstring = (wchar_t*)malloc(sizeof(wchar_t) * (orig.length() - 1));

	mbstowcs_s(&convertedChars, wcstring, origsize, orig.c_str(), _TRUNCATE);

	return wcstring;
}

HINTERNET WINAPI Mine_WinHttpConnect(
	IN HINTERNET     hSession,
	IN LPCWSTR       pswzServerName,
	IN INTERNET_PORT nServerPort,
	IN DWORD         dwReserved
) {
	PRINT_DEBUG("Mine_WinHttpConnect %ls %u\n", pswzServerName, nServerPort);
	struct sockaddr_in ip4;
	struct sockaddr_in6 ip6;
	ip4.sin_family = AF_INET;
	ip6.sin6_family = AF_INET6;

	if ((InetPtonW(AF_INET, pswzServerName, &(ip4.sin_addr)) && is_lan_ip((sockaddr*)&ip4, sizeof(ip4))) || (InetPtonW(AF_INET6, pswzServerName, &(ip6.sin6_addr)) && is_lan_ip((sockaddr*)&ip6, sizeof(ip6)))) {
		return Real_WinHttpConnect(hSession, pswzServerName, nServerPort, dwReserved);
	}
	else {
		pswzServerName = L"127.1.33.7";
		if (file_exists(get_full_program_path() + "serverip.txt"))
		{
			//Pinenut:Sorry!I am not good at C++!
			PRINT_DEBUG("Crack MultiPlayer Check - IP\n");
			std::string aaa = get_full_program_path();
			std::ifstream readFile(get_full_program_path() + "serverip.txt");
			//Read and put it in.
			std::string temp;
			readFile >> temp;
			const std::string& tempip = temp;
			//std::wstring strr = ANSIToUnicode(tempip);
			//LPCWSTR strs = //strr.c_str();
			pswzServerName = stringToLPCWSTR(temp);
		}
		if (file_exists(get_full_program_path() + "serverport.txt"))
		{
			PRINT_DEBUG("Crack MultiPlayer Check - Port\n");
			std::ifstream readFile(get_full_program_path() + "serverport.txt");
			std::string temp;
			readFile >> temp;
			//atoi((char*)strPort.c_str());
			int port = atoi((char*)temp.c_str());
			nServerPort = port;
		}
		PRINT_DEBUG("LAN_WinHttpConnect %ls %u\n", pswzServerName, nServerPort);
		return Real_WinHttpConnect(hSession, pswzServerName, nServerPort, dwReserved);
	}
}

HINTERNET(WINAPI* Real_WinHttpOpenRequest)(
	IN HINTERNET hConnect,
	IN LPCWSTR   pwszVerb,
	IN LPCWSTR   pwszObjectName,
	IN LPCWSTR   pwszVersion,
	IN LPCWSTR   pwszReferrer,
	IN LPCWSTR* ppwszAcceptTypes,
	IN DWORD     dwFlags

	);

HINTERNET WINAPI Mine_WinHttpOpenRequest(
	IN HINTERNET hConnect,
	IN LPCWSTR   pwszVerb,
	IN LPCWSTR   pwszObjectName,
	IN LPCWSTR   pwszVersion,
	IN LPCWSTR   pwszReferrer,
	IN LPCWSTR* ppwszAcceptTypes,
	IN DWORD     dwFlags
) {
	PRINT_DEBUG("Mine_WinHttpOpenRequest %ls %ls %ls %ls %i\n", pwszVerb, pwszObjectName, pwszVersion, pwszReferrer, dwFlags);
	if (dwFlags & WINHTTP_FLAG_SECURE) {
		dwFlags ^= WINHTTP_FLAG_SECURE;
	}
	return Real_WinHttpOpenRequest(hConnect, pwszVerb, pwszObjectName, pwszVersion, pwszReferrer, ppwszAcceptTypes, dwFlags);
}

//��Ӷ�GetEnvironmentVariableA��Hook(ԭװ������ ������ֹ��Ϸ��Ϊ�Լ�û����
DWORD (WINAPI* Real_GetEnvironmentVariableA)(
	IN  LPCSTR lpName,
	OUT LPSTR  lpBuffer,
	IN  DWORD  nSize
);

DWORD WINAPI Mine_GetEnvironmentVariableA(
	IN  LPCSTR lpName,
	OUT LPSTR  lpBuffer,
	IN  DWORD  nSize
) {
	//��ȡ��ǰ��ȡ����Ϣ
	string envname = lpName;
	std::string s = "273350";
	//�����SteamAppid������ȡ����Ϣ���273350��
	if (envname == "SteamAppId"||envname == "SteamGameId")
	{
		if (lpBuffer != NULL)
		{
			PRINT_DEBUG("Set Environment to appid!\n");
			lpBuffer[0] = '2';
			lpBuffer[1] = '7';
			lpBuffer[2] = '3';
			lpBuffer[3] = '3';
			lpBuffer[4] = '5';
			lpBuffer[5] = '0';
			lpBuffer[6] = '\0';
			return (long)6;
		}
		else {
			PRINT_DEBUG("Nothing should be set!\n");
			return Real_GetEnvironmentVariableA(lpName, lpBuffer, nSize);
		}
	}
	else {
		PRINT_DEBUG("Nothing should be set!\n");
		return Real_GetEnvironmentVariableA(lpName, lpBuffer, nSize);
	}
	

}
//�ض���SteamClient·��������·����Ϊ������·
//Ŀǰ������Steam�����õ�ֻ���ĸ�ѡ�����ֻģ�����ĸ�ѡ�������ȫ�����ܣ�Ӧ�þ�������
LSTATUS(WINAPI* Real_RegQueryValueExA)(
	IN                HKEY    hKey,
	IN     LPCSTR  lpValueName,
	IN  LPDWORD lpReserved,
	OUT     LPDWORD lpType,
	OUT     LPBYTE  lpData,
	IN LPDWORD lpcbData
	);

LSTATUS WINAPI Mine_RegQueryValueExA(
	IN                HKEY    hKey,
	IN     LPCSTR  lpValueName,
	IN  LPDWORD lpReserved,
	OUT     LPDWORD lpType,
	OUT     LPBYTE  lpData,
	IN LPDWORD lpcbData
) {
	PRINT_DEBUG("MY_RegQueryValueExA! is successfully hook!\n");
	//���棺����޸Ļ�Ӱ�쵽����ĺ�����
	//��ȡ��ǰ#	Time of Day	Thread	Module	API	Return Value	Error	Duration
	//10018	9:18 : 34.570 PM	1	Evolve.exe	RegQueryValueExW(0x0000000000000394, "SteamClientDll64", NULL, NULL, 0x000000685034f9f0, 0x000000685034f9e0)	ERROR_SUCCESS		0.0000169
	string keyname = lpValueName;
	if (keyname == "SteamClientDll64")
	{
		if (lpData != NULL)
		{
			PRINT_DEBUG("Game is getting Steam Client Dll location, redirect to local file!\n");
			//ѭ����ֵ
			std::string steamclient = get_full_program_path() + "pinenut64.dll";
			for (int i = 0; i < steamclient.length(); i++)
			{
				//��λ�趨
				lpData[i] = steamclient[i];
			}
			//���ó���(*���������)
			*lpcbData = (LONG)steamclient.length();
			return ERROR_SUCCESS;
		}

	}
	else if (keyname == "pid")
	{
		//ʵ�ڽ�����ˣ��Ҵ�������ֱ������ԭװ�ĺ���������pid������
		HKEY Registrykey;
		if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam\\ActiveProcess", 0, KEY_ALL_ACCESS, &Registrykey) == ERROR_SUCCESS)
		{
			RegSetValueExA(Registrykey, "pid_fake", NULL, REG_DWORD, (LPBYTE)&ProcessID, sizeof(DWORD));
			PRINT_DEBUG("Set pid_fake successfully!");
			return Real_RegQueryValueExA(hKey, "pid_fake", lpReserved, lpType, lpData, lpcbData);
		}

		
	}
	else if (keyname == "ActiveUser")
	{
		PRINT_DEBUG("activeuser check\n");
		if (lpData != NULL)
		{
			PRINT_DEBUG("Game is getting ActiveUser!\n");
			//ѭ����ֵ
			//std::string steamclient = get_full_program_path() + "pinenut64.dll";
			//��ColdClientLoader��������
			DWORD UserId = 0x03100004771F810D & 0xffffffff;
			//������֮��ֵ
			*lpcbData = UserId;
			return ERROR_SUCCESS;
		}
	}
		//�����޸ģ����ز��޸ĵ�ֵ
		return Real_RegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}

//WIN TRUST FAKE
LONG Mine_WinVerifyTrust(
	HWND   hwnd,
	GUID* pgActionID,
	LPVOID pWVTData
) {
	return (LONG)0;
};

BOOL WINAPI Init()
{
	//Fix WinTrust check
	pfnAheadLib_WinVerifyTrust = Mine_WinVerifyTrust;//GetAddress("WinVerifyTrust");
	return TRUE;
}	

DWORD WINAPI ThreadProc(LPVOID lpThreadParameter)
{
	HANDLE hProcess;

	PVOID addr1 = reinterpret_cast<PVOID>(0x00401000);
	BYTE data1[] = { 0x90, 0x90, 0x90, 0x90 };

	//
	// �ƹ�VMP3.x ���ڴ汣��
	//
	hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, GetCurrentProcessId());
	if (hProcess)
	{
		WriteProcessMemory(hProcess, addr1, data1, sizeof(data1), NULL);

		CloseHandle(hProcess);
	}

	return 0;
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, PVOID pvReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hModule);
		//Load() &&
		if (Init())
		{
			TCHAR szAppName[MAX_PATH] = TEXT("Evolve.exe");//���޸�����������
			TCHAR szCurName[MAX_PATH];

			GetModuleFileName(NULL, szCurName, MAX_PATH);
			PathStripPath(szCurName);

			//�Ƿ��ж����������� g
			if (StrCmpI(szCurName, szAppName) == 0)
			{
				int x;
				x = MessageBox(GetForegroundWindow(), L"�ò��������ߣ�Pinenut;Bվ��Ϊ��Pinenutn���ò������������뷨����Ϸ��֤�ض�����Ϸ��֤�Ƴ��ȶ๦�ܡ�\n����������л��Nemirtingas��schmogmog,nemerod,kiagam���Լ����ڽ�������ȺȺ��pikapika�͸�λ����Ⱥ�ѡ�\n�����������ò�����ѣ��Ͻ�������ʹ���䲻��������������Ȩ�鱾�����С�\n�������������ͬ����Ϊ���������߸�����Ŭ�������Ե�Bվ�ҵ�������������֤������Ŀǰ���ұ��˳е����������������ޡ���\n���ȷ����ʼ��Ϸ�����ı���ÿ�ζ�����ʾ��\n��վ��firehomework.github.io��", L"��ע�����", 1);
				printf("%d\n", x);
				if (x != 1)
				{
					exit(0);
				}
				//�ر����뷨����
				ImmDisableIME(GetCurrentThreadId());
				if (file_exists(get_full_program_path() + "evolvecrack.txt"))
				{
					//���������̻߳�����������
					DetourTransactionBegin();
					DetourUpdateThread(GetCurrentThread());
					//Hook Them:
					DetourAttach(&(PVOID&)Real_SendTo, Mine_SendTo);
					DetourAttach(&(PVOID&)Real_Connect, Mine_Connect);
					DetourAttach(&(PVOID&)Real_WSAConnect, Mine_WSAConnect);
					HMODULE winhttp = GetModuleHandle(L"winhttp.dll");
					HMODULE kernel32 = GetModuleHandle(L"kernel32.dll");
					HMODULE advapi32 = GetModuleHandle(L"ADVAPI32.dll");
					if (file_exists(get_full_program_path() + "evolvelan.txt"))
					{
						if (advapi32)
						{
						PRINT_DEBUG("advapi32 HOOK SUCCESSFULLY.\n");
						Real_RegQueryValueExA = (decltype(Real_RegQueryValueExA))GetProcAddress(advapi32, "RegQueryValueExA");
						DetourAttach(&(PVOID&)Real_RegQueryValueExA, Mine_RegQueryValueExA);
						PRINT_DEBUG("kernel32 HOOK END.\n");
						}
					}
					if (kernel32)
					{
						PRINT_DEBUG("kernel32 HOOK SUCCESSFULLY.\n");
						Real_GetEnvironmentVariableA = (decltype(Real_GetEnvironmentVariableA))GetProcAddress(kernel32, "GetEnvironmentVariableA");
						DetourAttach(&(PVOID&)Real_GetEnvironmentVariableA, Mine_GetEnvironmentVariableA);
						PRINT_DEBUG("kernel32 HOOK END.\n");
					}
					
					if (winhttp) {
						PRINT_DEBUG("WINHTTP HOOK SUCCESSFULLY.\n");
						Real_WinHttpConnect = (decltype(Real_WinHttpConnect))GetProcAddress(winhttp, "WinHttpConnect");
						DetourAttach(&(PVOID&)Real_WinHttpConnect, Mine_WinHttpConnect);
						//Open this OpenRequest Function(Pinenut)
						Real_WinHttpOpenRequest = (decltype(Real_WinHttpOpenRequest))GetProcAddress(winhttp, "WinHttpOpenRequest");
						DetourAttach(&(PVOID&)Real_WinHttpOpenRequest, Mine_WinHttpOpenRequest);
					}
					DetourTransactionCommit();
				}

				//network_functions_attached = true;
				//HANDLE hThread = CreateThread(NULL, NULL, ThreadProc, NULL, NULL, NULL);
				//if (hThread)
				//{
				//	CloseHandle(hThread);
				//}
			}
		}
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		if (file_exists(get_full_program_path() + "evolvecrack.txt"))
		{
			//Free();
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourDetach(&(PVOID&)Real_SendTo, Mine_SendTo);
			DetourDetach(&(PVOID&)Real_Connect, Mine_Connect);
			DetourDetach(&(PVOID&)Real_WSAConnect, Mine_WSAConnect);
			PRINT_DEBUG("WS2_32 UNHOOK SUCCESSFULLY.\n");
			if (Real_GetEnvironmentVariableA)
			{
				PRINT_DEBUG("KERNEL32 UNHOOK SUCCESSFULLY.\n");
				DetourDetach(&(PVOID&)Real_GetEnvironmentVariableA, Mine_GetEnvironmentVariableA);
			}
			if (Real_WinHttpConnect) {
				PRINT_DEBUG("WINHTTP UNHOOK SUCCESSFULLY.\n");
				DetourDetach(&(PVOID&)Real_WinHttpConnect, Mine_WinHttpConnect);
				DetourDetach(&(PVOID&)Real_WinHttpOpenRequest, Mine_WinHttpOpenRequest);
			}
			if (file_exists(get_full_program_path() + "evolvelan.txt"))
			{ 
			if (Real_RegQueryValueExA)
			{
				PRINT_DEBUG("ADVAPI32 UNHOOK SUCCESSFULLY.\n");
				DetourDetach(&(PVOID&)Real_RegQueryValueExA, Mine_RegQueryValueExA);
			}
			}
			DetourTransactionCommit();
		}
	}

	return TRUE;
}

