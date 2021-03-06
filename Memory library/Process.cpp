#include "Process.h"

#include <AtlBase.h>
#include <atlconv.h>
#include <algorithm>
#include <cctype>
#include <string.h>
#include <cstring>
#include <Psapi.h>

#include <iostream>
#include "Process.h"

#include <DbgHelp.h>

#pragma comment(lib, "dbghelp.lib")

Process::PROCESS Process::GetProcess(std::string const& processname, bool openprocess, uint32_t processAccess)
{
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap == 0)
		return {};

	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(entry);
	
	std::string pname = processname;

	CA2W wStr(pname.c_str());
	
	if (Process32First(hSnap, &entry))
	{
		while (Process32Next(hSnap, &entry))
		{
			std::wstring entryName = entry.szExeFile;

			if (StrCmpW(entry.szExeFile, wStr.m_szBuffer) == 0)
			{
				/* Close snapshot handle */
				CloseHandle(hSnap);

				PROCESS process;
				process.m_pid = entry.th32ProcessID;
				process.m_name = CW2A(entry.szExeFile);
				
				if (openprocess)
					process.m_handle = OpenProcess(processAccess, 0, entry.th32ProcessID);

				return process;
			}
		}
	}
	CloseHandle(hSnap);

	return {};
}

Process::PROCESS Process::GetProcess(int const& pid, bool openprocess, uint32_t processAccess)
{
	HANDLE hHandle = OpenProcess(processAccess, 0, pid);
	if (hHandle == INVALID_HANDLE_VALUE)
		return {};

	HMODULE hModule = { 0 };

	TCHAR szName[MAX_PATH];

	GetModuleBaseName(hHandle, hModule,szName,sizeof(szName) / sizeof(TCHAR));

	PROCESS process;
	process.m_pid = pid;
	process.m_name = CW2A(szName);
	
	if (openprocess)
	{
		process.m_handle = OpenProcess(processAccess, 0, pid);
	}

	return process;
}

std::vector<Process::PROCESS> Process::GetProcesses()
{
	/* Iterate through all processes */
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (!hSnap)
		return {};

	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(entry);

	std::vector<Process::PROCESS> Processes = {};

	if (Process32First(hSnap, &entry))
	{
		while (Process32Next(hSnap, &entry))
		{

			PROCESS process;
			process.m_pid = entry.th32ProcessID;
			process.m_name = CW2A(entry.szExeFile);
		
			Processes.push_back(process);
		}
	}

	CloseHandle(hSnap);

	return Processes;
}

bool Process::PROCESS::kill() const
{
	if (m_pid == 0)
		return false;

	/* Open Process if it's not already open */
	if (!m_handle)
	{
		HANDLE hHandle = OpenProcess(PROCESS_ALL_ACCESS, 0, m_pid);

		if (hHandle == 0 || hHandle == INVALID_HANDLE_VALUE)
			return false;

		/* Terminate process */
		bool status = TerminateProcess(hHandle, 1);

		/* Close handle */
		CloseHandle(hHandle);

		return status;

	}
	else
	{
		bool status = TerminateProcess(m_handle, 1);
		
		return status;
	}

	return false;
}

bool Process::PROCESS::running() const
{
	if (WaitForSingleObject(m_handle, 0) == WAIT_TIMEOUT)
		return true;
	else
		return false;
}

HANDLE Process::PROCESS::open() const
{
	return OpenProcess(PROCESS_ALL_ACCESS, 0, m_pid);
}

std::vector<Process::MODULE> Process::PROCESS::GetModules() const
{
	/* Get snapshot handle */
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, m_pid);
	if (hSnap == 0)
		return {};

	MODULEENTRY32W entry = { 0 };
	entry.dwSize = sizeof(entry); // 64 bit moduleentry size.
	
	std::vector<MODULE> Modules = {};

	if (Module32FirstW(hSnap, &entry))
	{
		while (Module32NextW(hSnap, &entry))
		{
			MODULE Module;
			Module.size = entry.modBaseSize;
			Module.base = (uintptr_t)entry.hModule;
			Module.m_name = CW2A(entry.szModule);

			Modules.push_back(Module);
		}
	}

	CloseHandle(hSnap);
	return Modules;
}

Process::MODULE Process::PROCESS::GetModule(std::string const& modulename) const
{
	auto modules = GetModules();

	if (modulename.empty() && modules.size() > 0)
	{
		return modules[0];
	}
	
	/* To lowercase.. */
	std::string modname = modulename;
	std::transform(modname.begin(), modname.end(), modname.begin(), [](unsigned char c) {return _mbctolower(c);});

	for (auto const& Module : modules)
	{
		std::string name = Module.m_name;
		std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {return _mbctolower(c);});
		
		if (name == modname)
			return Module;
	}

	return {};
}

bool Process::PROCESS::Is64Bit() const
{
	return false;
}

uintptr_t Process::MODULE::GetAddress(std::string const& str)
{
	return (uintptr_t)GetProcAddress((HMODULE)this->base, str.c_str());
}
