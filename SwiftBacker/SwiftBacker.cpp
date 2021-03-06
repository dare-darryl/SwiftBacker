// SwAutobacker.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "windows.h" //fileapi.h documentation
#include <exception>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <experimental/filesystem>
#include <filesystem>
#include <time.h>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
using namespace std;

bool verbose = false;
vector<std::string> sourceFiles;
string destFile;
string logFile;
string errorFile;
int fileScanned;
int fileCopied;
int fileReplaced;
ofstream logStream;
ofstream errorStream;

void controlPrint(std::string output) {
	if (verbose) {
		cout << output << endl;
	}
}

void logError() {

}

void writeLog(string input) {
	//ofstream logStream(logFile, ofstream::out | ofstream::app);
	logStream << input << endl;;
	//logStream.close();
}

void deleteFile(string fileName) {
	bool success = DeleteFileA(fileName.c_str());
	controlPrint("" + success);
}

void copyFile(string source, string dest) {
	try
	{
		experimental::filesystem::path sourceFile = source;
		experimental::filesystem::path destFile = dest;
	
		experimental::filesystem::create_directories(destFile.parent_path());
		experimental::filesystem::copy_file(sourceFile, destFile, experimental::filesystem::copy_options::none);
	}
	catch (exception e) {
		controlPrint(e.what());
		errorStream << "COPYFILE FAIL" << endl;
		errorStream << "[SOURCE] " << source << endl;
		errorStream << "[DEST] " << dest << endl << endl;
	}
}

string getDestFromSource(string source) {
	// Remove drive from source
	string cleanSource;

	istringstream line(source);
	getline(line, cleanSource, ':');
	getline(line, cleanSource);

	string dest = destFile + cleanSource;

	return dest;

}

bool sourceIsUpdated(string source, string dest, bool &new_write) {
	SetLastError(0);

	HANDLE hSource;
	hSource = CreateFileA(source.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	
	if (GetLastError() & ERROR_FILE_NOT_FOUND) {
		// logError()
		controlPrint("Source not found!");
		CloseHandle(hSource);
		return false;
	}
	
	SetLastError(0);
	HANDLE hDest;
	hDest = CreateFileA(dest.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (GetLastError() & ERROR_FILE_NOT_FOUND) {
		controlPrint("Dest not found!");
		CloseHandle(hSource);
		CloseHandle(hDest);
		new_write = true;
		return true;
	}
	

	/*
	 * At this point, both source and destination exists
	 */

	FILETIME sourceLastMod;
	FILETIME destLastMod;

	GetFileTime(hSource, NULL, NULL, &sourceLastMod);
	GetFileTime(hDest, NULL, NULL, &destLastMod);
	CloseHandle(hSource);
	CloseHandle(hDest);
	// If source last modified is later than dest
	if (CompareFileTime(&sourceLastMod, &destLastMod) >= 1) {
		return true;
	}
	return false;
}

void recursiveBackFile(std::string fileName) {
	DWORD ftyp = GetFileAttributesA(fileName.c_str());

	// Check that fileName exists
	if (ftyp == INVALID_FILE_ATTRIBUTES) {
		// Write to log at current directory
		return;
	}

	// Is a directory, recursively list file
	if (ftyp & FILE_ATTRIBUTE_DIRECTORY) {
		WIN32_FIND_DATAA currFile;
		HANDLE hFind;

		// Print Directory
		controlPrint("DIRECTORY" + fileName);
		//cout << "DIRECTORY:" << fileName << endl;

		std::string fileNameSearch = fileName + "\\*";

		if ((hFind = FindFirstFileA(fileNameSearch.c_str(), &currFile)) != INVALID_HANDLE_VALUE) {
			do {
				//If fileName is "." or "..", skip
				if (strncmp(currFile.cFileName, ".", 2) == 0 || strncmp(currFile.cFileName, "..", 2) == 0) {
					continue;
				}
				//cout << currFile.cFileName << endl;
				recursiveBackFile(fileName + "\\" + currFile.cFileName);
				
			} while (FindNextFileA(hFind, &currFile) != 0);
			FindClose(hFind);
		}
	}
	else { // Is a valid file, not a directory
		controlPrint(fileName);
		fileScanned += 1;

		bool new_write = false;
		string dest = getDestFromSource(fileName);

		if (sourceIsUpdated(fileName, dest, new_write)) {
			if (new_write) {
				copyFile(fileName, dest);
				fileCopied += 1;
				writeLog("[COPIED] " + dest);
			}
			else {
				deleteFile(dest);
				copyFile(fileName, dest);
				fileReplaced += 1;
				writeLog("[REPLACED] " + dest);
			}
		}

		if (fileScanned % 250 == 0) {
			cout << fileScanned << " files processed..." << endl;
		}
		//cout << fileName << endl;
	}

}

// trim from start
static inline std::string &ltrim(std::string &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(),
		std::not1(std::ptr_fun<int, int>(std::isspace))));
	return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
	s.erase(std::find_if(s.rbegin(), s.rend(),
		std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
	return ltrim(rtrim(s));
}

void readConfig(std::string fileName) {
	ifstream input;
	input.open(fileName);

	string currLine;

	while (getline(input, currLine)) {
		if ((currLine.size() != 0) && (currLine.at(0) == '#')) {
			continue;
		}
		controlPrint(currLine);
		
		// Tokenize input
		string currTok;
		istringstream line(currLine);

		// Get variable
		getline(line, currTok, ':');

		if (currTok.compare("verbose") == 0) {
			getline(line, currTok, ':');
			trim(currTok);
			
			if (currTok.compare("true") == 0) {
				verbose = true;
			}
			else
			{
				verbose = false;
			}
		}
		else if (currTok.compare("source") == 0) {
			getline(line, currTok);
			trim(currTok);

			sourceFiles.push_back(currTok);
		}
		else if (currTok.compare("dest") == 0) {
			getline(line, currTok);
			trim(currTok);

			destFile = currTok;
		}
	}
	controlPrint("");

}

void init() {
	std::time_t rawtime;
	std::tm* timeinfo;
	char buffer[80];

	std::time(&rawtime);
	timeinfo = localtime(&rawtime);

	std::strftime(buffer, 80, "%Y-%m-%d-%H-%M", timeinfo);
	
	logFile = buffer;
	logFile += "-SwAutobacker.log";
	errorFile = buffer;
	errorFile += "-SwError.log";
	logStream = ofstream(logFile, ofstream::out | ofstream::app);
	errorStream = ofstream(errorFile, ofstream::out | ofstream::app);

	writeLog("Backup started: " + string(buffer));

	readConfig("swconfig.ini");

	fileScanned = 0;
	fileCopied = 0;
	fileReplaced = 0;
}

void endTask(string endTimeMessage) {
	writeLog("\n");
	writeLog("PROCESSED: " + to_string(fileScanned));
	writeLog("COPIED: " + to_string(fileCopied));
	writeLog("REPLACED: " + to_string(fileReplaced));
	writeLog("UNTOUCHED: " + to_string(fileScanned - fileCopied - fileReplaced));

	writeLog(endTimeMessage);

	logStream.close();
	errorStream.close();
}

int main()
{
	clock_t tStart = clock();
	init();

	cout << "Proceed with backup? Ctrl+C to terminate, else, <enter>." << endl;
	getchar();
	cout << "Backup started!" << endl << endl;
	
	for (int i = 0; i < sourceFiles.size(); i++) {
		string currSource = sourceFiles.at(i);
		recursiveBackFile(currSource);
	}

	endTask("SwAutobacker completed in " + to_string((double)(clock() - tStart) / CLOCKS_PER_SEC) + " seconds.");
	
	cout << "Time Taken: " << (double)(clock() - tStart) / CLOCKS_PER_SEC << " seconds." << endl;
	getchar();
	
	return 0;
}


