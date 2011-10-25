#include <stdio.h>
#include <stdlib.h> //for exit
#include <time.h>

#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <exception>

#include <windows.h>

#include "option.h"

#include <stack>
#include <map>
#include <queue>

long long numFiles;
long long numDirectories;

class DirectoryInfo
{
public:
   std::string directoryName;
   long long totalSize;
   long long hiddenSize;
   long long systemSize;
   long long hidAndSysSize;
   long long numDirectories;
   long long numFiles;

   DirectoryInfo()
   {
      totalSize = hiddenSize = systemSize = hidAndSysSize = numDirectories = numFiles = 0;
   }

   DirectoryInfo(std::string name, long long _totalSize, long long _hiddenSize, long long _systemSize, 
                 long long _hidAndSysSize, long long _numDirectories, long long _numFiles)
   {
      directoryName = name;
      totalSize = _totalSize;
      hiddenSize = _hiddenSize;
      systemSize = _systemSize;
      hidAndSysSize = _hidAndSysSize;
      numDirectories = _numDirectories;
      numFiles = _numFiles;
   }

   DirectoryInfo &operator+=(const DirectoryInfo &_d)
   {
      this->totalSize += _d.totalSize;
      this->hiddenSize += _d.hiddenSize;
      this->systemSize += _d.systemSize;
      this->hidAndSysSize += _d.hidAndSysSize;
      this->numDirectories += _d.numDirectories;
      this->numFiles += _d.numFiles;

      return *this;
   }

   DirectoryInfo operator+(const DirectoryInfo &_d) const
   {
      return DirectoryInfo(*this) += _d;
   }

   const bool operator<(const DirectoryInfo &_d) const
   {
      return this->totalSize < _d.totalSize;
   }
};


//taken from StackOverflow 
//http://stackoverflow.com/questions/7276826/c-format-number-with-commas
template<class T>
std::string FormatWithCommas(T value)
{
    std::stringstream ss;
    ss.imbue(std::locale(""));
    ss << std::fixed << value;
    return ss.str();
}

struct OptionFlags
{
   bool verbose;
   bool setDirectory;
   char directory[MAX_PATH + 1];
   int topNum;
};

void GetOptionFlags(int argc, char *argv[], OptionFlags &o) 
{
   memset(&o, 0, sizeof(o));
   //get all the options
   o.verbose = scan_option(argc, argv, "-v") != 0;
   o.setDirectory = scan_option(argc, argv, "-d %s", o.directory) != 0;
   o.topNum = 5;
   scan_option(argc, argv, "-top %d", &o.topNum);
}

BOOL BitCompare(DWORD target, DWORD source)
{
   return((target&source) == source);
}

void FileCalculation(WIN32_FIND_DATA &fd, DirectoryInfo &d)
{
   long long fileSize = 0;
   long long maxDwordSize = MAXDWORD;
   maxDwordSize++;

   fileSize = fd.nFileSizeHigh * maxDwordSize + fd.nFileSizeLow;

   d.numFiles++;
   d.totalSize += fileSize;

   if (BitCompare(fd.dwFileAttributes, FILE_ATTRIBUTE_HIDDEN) == TRUE && 
       BitCompare(fd.dwFileAttributes, FILE_ATTRIBUTE_SYSTEM) == TRUE)
      d.hidAndSysSize += fileSize;
   else if (BitCompare(fd.dwFileAttributes, FILE_ATTRIBUTE_HIDDEN) == TRUE)
      d.hiddenSize += fileSize;
   else if (BitCompare(fd.dwFileAttributes, FILE_ATTRIBUTE_SYSTEM) == TRUE)
      d.systemSize += fileSize;
      
   return;
}


DirectoryInfo GetDirectoryInfo(std::string baseDirectoryName, OptionFlags &o, std::priority_queue<DirectoryInfo> &directoryQueue)
{
   DirectoryInfo d;
   WIN32_FIND_DATA fd;
   HANDLE h;

   d.directoryName = baseDirectoryName;

   if (SetCurrentDirectory(baseDirectoryName.c_str()) == FALSE)
   {
      int errorCode = GetLastError();
      printf("Unable to set directory to %s, error code %d\n", baseDirectoryName.c_str(), errorCode);
      return d;
   }

   h = FindFirstFile("*.*", &fd);

   if (h == INVALID_HANDLE_VALUE)
   {
      printf("Invalid handle value for %s\n", baseDirectoryName.c_str());
      return d;
   }

   std::stack<std::string> directoryStack;

   do
   {
      if (_stricmp(fd.cFileName, ".") == 0 || _stricmp(fd.cFileName, "..") == 0)
         continue;

      if (BitCompare(fd.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY) == TRUE)
      {
         directoryStack.push(fd.cFileName);
         numDirectories++;
      }
      else
      {
         FileCalculation(fd, d);
         numFiles++;
      }

      printf("Number of files = %I64u Number of subdirectories = %I64u\r", numFiles, numDirectories);

   } while (FindNextFile(h, &fd) != FALSE);

   FindClose(h);

   directoryQueue.push(d);

   while(directoryStack.size())
   {
      std::string childDirName = baseDirectoryName + "\\" + directoryStack.top();
      d += GetDirectoryInfo(childDirName, o, directoryQueue);
      directoryStack.pop();
   }

   return d;
}//end DirectorySize

int main (int argc, char *argv[])
{
   time_t time1,time2;//used for timers
   
   time1 = time(NULL);
   
   OptionFlags o;
   DirectoryInfo d;

   GetOptionFlags(argc,argv, o);

   if (o.setDirectory == false)
      strcpy(o.directory, ".");

   if (SetCurrentDirectory(o.directory)!=TRUE)
   {
      printf("Could not change directory to %s\n", o.directory);
      return -1;
   }

   char fullDirName[MAX_PATH + 1];
   GetCurrentDirectory(MAX_PATH, fullDirName);
   std::string baseDirectory = fullDirName;

   std::priority_queue<DirectoryInfo> directoryQueue;
   d = GetDirectoryInfo(baseDirectory, o, directoryQueue);

   printf("\n");
   printf("    Hidden files = %s bytes\n", FormatWithCommas<long long>(d.hiddenSize).c_str());
   printf("    System files = %s bytes\n", FormatWithCommas<long long>(d.systemSize).c_str());
   printf("    Both hidden and system files = %s bytes\n", FormatWithCommas<long long>(d.hidAndSysSize).c_str());
   printf("    Normal files = %s bytes\n", FormatWithCommas<long long>(d.totalSize - d.hiddenSize - d.systemSize - d.hidAndSysSize).c_str());
   printf("The total size of all files is %s bytes\n", FormatWithCommas<long long>(d.totalSize).c_str());

   time2 = time(NULL);

   if (o.verbose == true)
      printf("Operation took %d seconds\n", time2 - time1);


   for (int i = 1; i <= o.topNum; i++)
   {
      DirectoryInfo currentInfo = directoryQueue.top();
      printf("%d: %s - %s files %s totalSize\n", 
             i, currentInfo.directoryName.c_str(), 
             FormatWithCommas<long long>(currentInfo.numFiles).c_str(), 
             FormatWithCommas<long long>(currentInfo.totalSize).c_str());
      directoryQueue.pop();
   }

   return 0;
}











