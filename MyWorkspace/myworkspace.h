#pragma once
#ifndef _MYWORKSPACE_H_
#define _MYWORKSPACE_H_


#include <windows.h>
#include <tchar.h>
#include <iostream>

#include <fstream>
#include <ctime>
#include <mutex>

#include <stdio.h>
#include <time.h>
#include <vector>
#include <map>
#include <string>
#include <algorithm>

#include <Psapi.h>
#pragma comment(lib, "Kernel32.lib")
#pragma comment(lib, "Psapi.lib")

#ifndef DLLEXPORT
#define DLLEXPORT __declspec(dllexport)
#endif

#ifndef DLLIMPORT
#define DLLIMPORT __declspec(dllimport)
#endif

#define BOOL_TEXT(x) (x ? "true" : "false")

using namespace std;

const long long CurrentTimeMS();							// Get current ms
void FormatDate(char* buffer);								// Format date
void debug(const char* msg);								// Debug string to console

BOOL CALLBACK EnumWindowCallback(HWND hWnd, LPARAM lparam);	// Enum window callback

// A running process.
struct RunningProcess
{
	RunningProcess()
	{
		WindowHandle = NULL;
		ZeroMemory(WindowTitle, sizeof(char) * 1024);
		ZeroMemory(ImageName, sizeof(char) * 1024);
		IsMinimized = false;
	};

	bool operator < (const RunningProcess& a) const
	{
		std::string aWindowTitle(WindowTitle);
		std::string bWindowTitle(a.WindowTitle);
		
		return (aWindowTitle < bWindowTitle);
	}


	HWND WindowHandle;
	char WindowTitle[1024];
	char ImageName[1024];
	bool IsMinimized;
};



// Tile location.
struct TileLocation
{
	TileLocation()
	{
		Left = 0L;
		Top = 0L;
	};

	long Left;
	long Top;
};


// Provide process location.
struct ProcessLocation
{
	ProcessLocation()
	{
		Reset();
	};

	void Reset()
	{
		Tile = false;
		AbsoluteLocation = false;
		ZeroMemory(ImageName, sizeof(char) * 1024);
		ZeroMemory(WindowTitle, sizeof(char) * 1024);

		PercentageHeight = 0.5f;
		PercentageWidth = 0.5f;

		PercentageHeightLocation = 0.0f;
		PercentageWidthLocation = 0.0f;
	};

	char ImageName[1024];
	char WindowTitle[1024];
	bool Tile;
	bool AbsoluteLocation;

	float PercentageHeight;
	float PercentageWidth;

	float PercentageHeightLocation;
	float PercentageWidthLocation;

};


bool ProcessLocations(const std::vector<RunningProcess>& processes, const std::vector<ProcessLocation>& locations);		// Processes locations.
void RepositionProcess(const RunningProcess& process, const std::vector<ProcessLocation>& locations);				// Reposition the process
bool FindLocationForProcessAndTitle(const RunningProcess& process, const std::vector<ProcessLocation>& locations, ProcessLocation& outLocation);

bool FileExists(const char* file);								// Does file exist.
bool ParseLocations(std::vector<ProcessLocation>& locations);	// Parses locations
void split(const char* splitString, const char splitChar, std::vector<string>& strings);
bool startswith(const char* string, const char* search);	// Startswith string
bool endswith(const char* string, const char* search);		// Endswith string
bool equals(const char* string1, const char* string2);		// is equal (CI)?

#endif
