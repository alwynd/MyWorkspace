#include "myworkspace.h"

std::vector<RunningProcess> Processes;
std::vector<ProcessLocation> Locations;
RECT ScreenDimensions;

std::map<const char*, TileLocation> tileMap;

// Declare multimap globally
std::multimap<DWORD, HWND> chromeInstances;

HWND firstChromeInstanceHWND;


// Enum window callback
BOOL CALLBACK EnumWindowCallback(HWND hWnd, LPARAM lparam) 
{
	if (!lparam)
	{
		debug("MyWorkspace.EnumWindowCallback ERROR : lparam is INVALID.");
		return TRUE;
	}
	std::vector<RunningProcess>* processes = (std::vector<RunningProcess>*) lparam;

	int length = GetWindowTextLength(hWnd);
	char buffer[1024];
	ZeroMemory(buffer, 1024);
	DWORD dwProcId = 0;

	// List visible windows with a non-empty title
	if (IsWindowVisible(hWnd) && length != 0) 
	{
		RunningProcess process;

		process.WindowHandle = hWnd;
		int wlen = length + 1;
		GetWindowText(hWnd, process.WindowTitle, wlen);
		GetWindowThreadProcessId(hWnd, &dwProcId);
		if (dwProcId)
		{
			HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcId);
			if (hProc)
			{
				HMODULE hMod;
				DWORD cbNeeded;
				process.IsMinimized = IsIconic(hWnd);
				if (EnumProcessModules(hProc, &hMod, sizeof(hMod), &cbNeeded))
				{
					GetModuleBaseNameA(hProc, hMod, process.ImageName, MAX_PATH);
				} //if
				CloseHandle(hProc);
				processes->push_back(process);		// COPY, this is NOT BY REF
				sprintf_s(buffer, "MyWorkspace.EnumWindowCallback, Window: hwnd: %p, Title: '%-55s', Executable: '%-55s', Min: '%-5s'\0", process.WindowHandle, process.WindowTitle, process.ImageName, BOOL_TEXT(process.IsMinimized)); debug(buffer);

				if (equals(process.ImageName, "chrome.exe"))
				{
					//chromeInstances.insert(std::make_pair(dwProcId, process.WindowHandle));
				}				
			} //if
		} //if
	}

	return TRUE;
}




// Win main!
int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
	char buffer[1024];
	ZeroMemory(buffer, 1024);
	int rc = 0;

	if (!ParseLocations(Locations) || Locations.size() < 1)
	{
		debug("NO LOCATIONS.");
		rc = 99;
		goto DONE;
	}

	GetWindowRect(GetDesktopWindow(), &ScreenDimensions);
	sprintf_s(buffer, "MyWorkspace.WinMain:-- START, ScreenWidth: %i, ScreenHeight: %i\0", ScreenDimensions.right, ScreenDimensions.bottom); debug(buffer);

	Processes.clear();
	EnumWindows(EnumWindowCallback, (LPARAM)&Processes);
	sprintf_s(buffer, "MyWorkspace.WinMain \t Called EnumWindows..., Processes: %I64i\0", Processes.size()); debug(buffer);
	if (Processes.size() < 1)
	{
		debug("MyWorkspace.WinMain ERROR : Could not enumerate any windows.");
		rc = 99;
		goto DONE;
	} //if

	sprintf_s(buffer, "MyWorkspace.WinMain \t Sorting Processes: %I64i\0", Processes.size()); debug(buffer);
	std::sort(std::begin(Processes), std::end(Processes));

	for (int i = 0; i < Processes.size(); i++)
	{
		const RunningProcess& process = Processes[i];
		sprintf_s(buffer, "MyWorkspace.WinMain \t Sorted Processes: %i, process: '%s'\0", i, process.WindowTitle); debug(buffer);
	} //for

	if (!chromeInstances.empty())
	{
		firstChromeInstanceHWND = chromeInstances.begin()->second; // key is PID, value is HWND
	}	
	
	ProcessLocations(Processes, Locations);

DONE:
	debug("MyWorkspace.WinMain:-- DONE");
	return rc;
}




// Processes locations.
bool ProcessLocations(const std::vector<RunningProcess>& processes, const std::vector<ProcessLocation>& locations)
{
	char buffer[1024];
	ZeroMemory(buffer, 1024);
	sprintf_s(buffer, "MyWorkspace.ProcessLocations:-- START, Processes: %I64i, Locations: %I64i\0", processes.size(), locations.size()); debug(buffer);

	// go through all visible windows processes
	for (int pidx = 0; pidx < processes.size(); pidx++)
	{
		const RunningProcess& process = processes[pidx];

		// something we should reposition?
		RepositionProcess(process, locations);
	} //for

	// TASK MGR! I HATE YOU
	HWND hWndTarget = FindWindow("TaskManagerWindow", NULL);
	if (hWndTarget)
	{
		RECT newpos;
		newpos.right = (long)((float)ScreenDimensions.right * 0.3f);
		newpos.bottom = (long)((float)ScreenDimensions.bottom * 0.9f);

		newpos.left = (long)((float)ScreenDimensions.right * 0.69f);
		newpos.top = (long)((float)ScreenDimensions.bottom * 0.01f);

		SetWindowPos(hWndTarget, NULL, newpos.left, newpos.top, newpos.right, newpos.bottom, NULL);
		

	} //if

	return true;
}


bool FindLocationForProcessAndTitle(const RunningProcess& process, const std::vector<ProcessLocation>& locations, ProcessLocation& outLocation)
{
	bool result = false;

	// match by image name and title.
	for (int idx = 0; idx < locations.size(); idx++)
	{
		const ProcessLocation& location = locations[idx];
		if (!equals(process.ImageName, location.ImageName)) continue; //for

		if (strnlen_s(process.WindowTitle, 1024) > 0 &&
			strnlen_s(location.WindowTitle, 1024) > 0)
		{
			if (equals(process.WindowTitle, location.WindowTitle))
			{
				outLocation = location;
				result = true;
				break;
			}
		}
	}

	if (result) return result;

	// could not match by title, find the 1st one without a window title, by image name only.
	for (int idx = 0; idx < locations.size(); idx++)
	{
		const ProcessLocation& location = locations[idx];
		if (!equals(process.ImageName, location.ImageName)) continue; //for

		if (strnlen_s(location.WindowTitle, 1024) < 1)
		{
			outLocation = location;
			result = true;
			break;
		}
	}


	return result;

}


// RepositionProcess
void RepositionProcess(const RunningProcess& process, const std::vector<ProcessLocation>& locations)
{
	char buffer[1024];
	ZeroMemory(buffer, 1024);

	// DO NOT UPDATE PROGRAM MANAGER
	if (equals(process.WindowTitle, "Program Manager")) return;

	// MINIMIZED?
	if (process.IsMinimized)
	{
		ShowWindow(process.WindowHandle, SW_RESTORE);
	} //if

	ProcessLocation location;
	if (!FindLocationForProcessAndTitle(process, locations, location)) return;


	sprintf_s(buffer, "RepositionProcess FOUND location.ImageName: '%s', Window: '%s'\0", location.ImageName, process.WindowTitle);  debug(buffer);

	RECT newpos;
	newpos.right = (long)((float)ScreenDimensions.right * location.PercentageWidth);
	newpos.bottom = (long)((float)ScreenDimensions.bottom * location.PercentageHeight);

	long halfleft = newpos.right / 2L;
	long halftop = newpos.bottom / 2L;

	if (location.AbsoluteLocation)
	{
		halfleft = 0L;
		halftop = 0L;
	} //if

	newpos.left = (long)((float)ScreenDimensions.right * location.PercentageWidthLocation) - halfleft;
	newpos.top = (long)((float)ScreenDimensions.bottom * location.PercentageHeightLocation) - halftop;

	if (process.WindowHandle == firstChromeInstanceHWND)
	{
		newpos.top -= 100;
		newpos.left += 100;
	}
		
	// tiling?
	if (location.Tile)
	{
		if (tileMap.find(location.ImageName) == tileMap.end())
		{
			TileLocation tl;
			tileMap.emplace(location.ImageName, tl);	// copy
		} //if

		if (tileMap[location.ImageName].Left < 1L)
		{
			tileMap[location.ImageName].Left = newpos.left;
			tileMap[location.ImageName].Top = newpos.top;
		} //if
		else
		{
			// tile from bottom to top
			// then from right to left
			long top = tileMap[location.ImageName].Top - newpos.bottom - 3;
			if (top < 0L)
			{
				top = newpos.top;
				tileMap[location.ImageName].Left -= (newpos.right + 3);
			} //if

			newpos.top = top;
			newpos.left = tileMap[location.ImageName].Left;

			tileMap[location.ImageName].Top = newpos.top;
		} //else
	} //if

	sprintf_s(buffer, "RepositionProcess \t NEW POS: width: %d, height: %d, left: %d, top: %d\0", newpos.right, newpos.bottom, newpos.left, newpos.top);  debug(buffer);
	SetWindowPos(process.WindowHandle, NULL, newpos.left, newpos.top, newpos.right, newpos.bottom, NULL);


}




// Does file exist.
bool FileExists(const char* file)
{
	FILE* f = NULL; 
	fopen_s(&f, file, "rb");
	if (f)
	{
		fclose(f);
		return  true;
	} //if

	return false;
}

// Get current ms
const long long CurrentTimeMS()
{
	chrono::system_clock::time_point currentTime = chrono::system_clock::now();

	long long transformed = currentTime.time_since_epoch().count() / 1000000;
	long long millis = transformed % 1000;

	return millis;
}





// Format date
void FormatDate(char* buffer)
{
	buffer[0] = '\0';
	time_t timer;
	tm tm_info;

	timer = time(NULL);
	localtime_s(&tm_info, &timer);

	strftime(buffer, 26, "%Y-%m-%d %H:%M:%S\0", &tm_info);
}





// Debug string to console
void debug(const char* msg)
{
	char buffer[27];
	ZeroMemory(buffer, 27);
	FormatDate(buffer);

	printf("%s - %s\n", buffer, msg); fflush(stdout);
	
}



void split(const char* splitString, const char splitChar, std::vector<string>& strings)
{

	char logbuffer[1024];
	ZeroMemory(logbuffer, 1024);

	size_t bufferidx = 0;
	char buffer[1024];
	ZeroMemory(buffer, 1024);

	int len = (int)strnlen_s(splitString, 1024);
	for (int i = 0; i < len; i++)
	{
		if (splitString[i] == splitChar)
		{
			//sprintf_s(logbuffer, "Split \t : Adding: '%s'\0", buffer); debug(logbuffer);
			if (strnlen_s(buffer, 1024) > 0) strings.push_back(string(buffer));
			bufferidx = 0;
			ZeroMemory(buffer, 1024);
			continue;
		}
		buffer[bufferidx++] = splitString[i];
	}
	if (strnlen_s(buffer, 1024) > 0) strings.push_back(string(buffer));

}



// Endswith string
bool endswith(const char* string, const char* search)
{
	char logbuffer[1024];
	ZeroMemory(logbuffer, 1024);

	int stringlen = (int)strnlen_s(string, 1024);
	int searchlen = (int)strnlen_s(search, 1024);
	//sprintf_s(logbuffer, "endswith: \t stringlen: %i, searchlen: %i\0", stringlen, searchlen); debug(logbuffer);

	if (stringlen < searchlen) return false;
	const char* buffer = string + (stringlen - searchlen);

	bool match = strcmp(buffer, search) == 0;
	//sprintf_s(logbuffer, "endswith: \t\t search len: %i, buffer: '%s', match: '%s'\0", searchlen, buffer, BOOL_TEXT(match)); debug(logbuffer);


	return match;
}




// Starts with string
bool startswith(const char* string, const char* search)
{
	char logbuffer[1024];
	ZeroMemory(logbuffer, 1024);

	char buffer[1024];
	ZeroMemory(buffer, 1024);

	// copy string, into buffer, for length of search
	int len = (int)strnlen_s(search, 1024);
	//sprintf_s(logbuffer, "startswith: \t search len: %i\0", len); debug(logbuffer);

	CopyMemory(buffer, string, len);
	//sprintf_s(logbuffer, "startswith: \t search len: %i, buffer: '%s'\0", len, buffer); debug(logbuffer);

	bool match = strcmp(buffer, search) == 0;
	//sprintf_s(logbuffer, "startswith: \t\t search len: %i, buffer: '%s', match: '%s'\0", len, buffer, BOOL_TEXT(match)); debug(logbuffer);

	return match;
}





// Parses locations
bool ParseLocations(std::vector<ProcessLocation>& locations)
{
	string line;
	char buffer[1024];
	ZeroMemory(buffer, 1024);

	debug("MyWorkspace.ParseLocations:-- START");
	const char* filename = "myworkspace.properties";
	if (!FileExists(filename)) 
	{
		sprintf_s(buffer, "MyWorkspace.ParseLocations ERROR : File does not exist: '%s'\0", filename); debug(buffer);
		return false;
	} //if

	sprintf_s(buffer, "MyWorkspace.ParseLocations: Opening file: '%s'\0", filename); debug(buffer);
	ifstream File(filename);
	if (!File)
	{
		sprintf_s(buffer, "MyWorkspace.ParseLocations ERROR : File does not exist(2): '%s'\0", filename); debug(buffer);
		return false;
	} //if

	ProcessLocation location;		// will be reused
	while (getline(File, line))
	{
		const char* linebuffer = line.c_str(); // ptr only
		if (startswith(line.c_str(), "location."))
		{
			sprintf_s(buffer, "MyWorkspace.ParseLocations \t Line: '%s'\0", line.c_str()); debug(buffer);
			std::vector<std::string> splits;
			split(line.c_str(), '=', splits);
			if (splits.size() < 2)
			{
				sprintf_s(buffer, "MyWorkspace.ParseLocations: ERRR :Invalid property line. Line: '%s'\0", line.c_str()); debug(buffer);
				continue;
			} //if

			const char* key = splits[0].c_str();
			const char* value = splits[1].c_str();
			sprintf_s(buffer, "MyWorkspace.ParseLocations \t\t Key: '%s' = '%s'\0", key, value); debug(buffer);
			if (endswith(key, ".imagename"))
			{
				//debug("MyWorkspace.ParseLocations \t\t\t KEY ENDSWITH .imagename");
				int len = (int)strnlen_s(splits[1].c_str(), 1024);
				CopyMemory(location.ImageName, splits[1].c_str(), len);
				//sprintf_s(buffer, "MyWorkspace.ParseLocations \t\t\t KEY ENDSWITH .imagename: '%s'\0", location.ImageName); debug(buffer);
			}
			if (endswith(key, ".title"))
			{
				//debug("MyWorkspace.ParseLocations \t\t\t KEY ENDSWITH .title");
				int len = (int)strnlen_s(splits[1].c_str(), 1024);
				CopyMemory(location.WindowTitle, splits[1].c_str(), len);
				//sprintf_s(buffer, "MyWorkspace.ParseLocations \t\t\t KEY ENDSWITH .title: '%s'\0", location.WindowTitle); debug(buffer);
			}
			if (endswith(key, ".percentagewidth"))
			{
				//debug("MyWorkspace.ParseLocations \t\t\t KEY ENDSWITH .percentagewidth");
				int len = (int)strnlen_s(splits[1].c_str(), 1024);
				location.PercentageWidth = (float)atof(splits[1].c_str());
				//sprintf_s(buffer, "MyWorkspace.ParseLocations \t\t\t KEY ENDSWITH .percentagewidth: '%f'\0", location.PercentageWidth); debug(buffer);
			}
			if (endswith(key, ".percentageheight"))
			{
				//debug("MyWorkspace.ParseLocations \t\t\t KEY ENDSWITH .percentageheight");
				int len = (int)strnlen_s(splits[1].c_str(), 1024);
				location.PercentageHeight = (float)atof(splits[1].c_str());
				//sprintf_s(buffer, "MyWorkspace.ParseLocations \t\t\t KEY ENDSWITH .percentageheight: '%f'\0", location.PercentageHeight); debug(buffer);
			}
			if (endswith(key, ".percentageheightlocation"))
			{
				//debug("MyWorkspace.ParseLocations \t\t\t KEY ENDSWITH .percentageheightlocation");
				int len = (int)strnlen_s(splits[1].c_str(), 1024);
				location.PercentageHeightLocation = (float)atof(splits[1].c_str());
				//sprintf_s(buffer, "MyWorkspace.ParseLocations \t\t\t KEY ENDSWITH .percentageheightlocation: '%f'\0", location.PercentageHeightLocation); debug(buffer);
			}
			if (endswith(key, ".absolute"))
			{
				location.AbsoluteLocation = true;
			}
			if (endswith(key, ".tile"))
			{
				location.Tile = true;
			}
			if (endswith(key, ".percentagewidthlocation"))
			{
				//debug("MyWorkspace.ParseLocations \t\t\t KEY ENDSWITH .percentagewidthlocation");
				int len = (int)strnlen_s(splits[1].c_str(), 1024);
				location.PercentageWidthLocation = (float)atof(splits[1].c_str());
				//sprintf_s(buffer, "MyWorkspace.ParseLocations \t\t\t KEY ENDSWITH .percentagewidthlocation: '%f'\0", location.PercentageWidthLocation); debug(buffer);
				locations.push_back(location);			// copy
				location.Reset();
			}
		} //if
	} //while


	File.close();

	// print locations.
	for (int i = 0; i < locations.size(); i++)
	{
		const ProcessLocation& location = locations[i];
		sprintf_s(buffer, "MyWorkspace.ParseLocations \t #%i: location.ImageName: '%s'\0"				, i, location.ImageName); debug(buffer);
		sprintf_s(buffer, "MyWorkspace.ParseLocations \t #%i: location.WindowTitle: '%s'\0"				, i, location.WindowTitle); debug(buffer);
		sprintf_s(buffer, "MyWorkspace.ParseLocations \t #%i: location.Tile: '%s'\0"					, i, BOOL_TEXT(location.Tile)); debug(buffer);
		sprintf_s(buffer, "MyWorkspace.ParseLocations \t #%i: location.AbsoluteLocation: '%s'\0"		, i, BOOL_TEXT(location.AbsoluteLocation)); debug(buffer);
		sprintf_s(buffer, "MyWorkspace.ParseLocations \t #%i: location.PercentageWidth: %.2f\0"			, i, location.PercentageWidth); debug(buffer);
		sprintf_s(buffer, "MyWorkspace.ParseLocations \t #%i: location.PercentageHeight: %.2f\0"		, i, location.PercentageHeight); debug(buffer);
		sprintf_s(buffer, "MyWorkspace.ParseLocations \t #%i: location.PercentageWidthLocation: %.2f\0"	, i, location.PercentageWidthLocation); debug(buffer);
		sprintf_s(buffer, "MyWorkspace.ParseLocations \t #%i: location.PercentageHeightLocation: %.2f\0", i, location.PercentageHeightLocation); debug(buffer);
	} //for

	return true;
}




// is equal (CI)?
bool equals(const char* string1, const char* string2)
{
	char buffer[1024];
	ZeroMemory(buffer, 1024);

	char str1[1024];
	char str2[1024];
	ZeroMemory(str1, 1024);
	ZeroMemory(str2, 1024);

	int len = (int)strnlen_s(string1, 1024);
	int len2 = (int)strnlen_s(string2, 1024);

	if (len != len2) return false;

	CopyMemory(str1, string1, len);
	CopyMemory(str2, string2, len2);

	_strlwr_s(str1, len+1);
	_strlwr_s(str2, len2+1);

	bool match = strcmp(str1, str2) == 0;
	//sprintf_s(buffer, "equals: \t\t search len: %i, str1: '%s', str2: '%s', match: '%s'\0", len, str1, str2, BOOL_TEXT(match)); debug(buffer);

	return match;
	
}