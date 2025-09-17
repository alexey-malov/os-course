#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <windows.h>

int main()
{
	STARTUPINFOA si{};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi{};

	// Командная строка должна быть ИЗМЕНЯЕМОЙ (CreateProcess может её править)
	char cmd[] = "notepad.exe C:\\Windows\\win.ini";

	BOOL ok = CreateProcessA(/*App name*/ nullptr, cmd,
		/* processSecurityAttrs */ nullptr, /* threadSecurityAttrs */ nullptr,
		/* inheritHandles */ FALSE,
		/* creationFlags */ 0, /* environment */ nullptr,
		/* currentDirectory */ nullptr, &si, &pi);
	if (!ok)
	{
		std::cerr << "CreateProcess failed (" << GetLastError() << ")\n";
		return 1;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD code = 0;
	if (GetExitCodeProcess(pi.hProcess, &code))
		std::cout << "child exit code = " << code << "\n";
	else
		std::cerr << "GetExitCodeProcess failed (" << GetLastError() << ")\n";

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
}
