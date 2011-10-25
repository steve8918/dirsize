#include <stdio.h>
#include <stdlib.h> //for exit
#include <time.h>
#include <math.h> //for ceil
#include "CMP3Info.h"


#include <windows.h>
#define MAXPATH 1024
#define VERSION 1.1
#define BAD_PARAM                  10000
#define TOO_MANY_DIRECTORIES_SPECIFIED   10001
#define TOO_MANY_CRC_FLAGS            10002
#define BAD_DIRECTORY_NAME            10003

int crc32(char *filename);
unsigned short int crc16(char *filename);

BOOL BitCompare(DWORD target, DWORD source);
int DirectorySize(void);
DWORDLONG FileCalculation(WIN32_FIND_DATA *fd);
void DirectoryChange(WIN32_FIND_DATA *fd);
void ErrorMessage(int ErrNum);
int ProcessArgs(int argc, char *argv[]);
char *PrintFormattedOutput(DWORDLONG Size);

//Global Counters
DWORDLONG TotalSize=0;
DWORDLONG HiddenSize = 0;
DWORDLONG SystemSize = 0;
DWORDLONG HidAndSysSize = 0;
DWORDLONG NumDirectories = 0;
DWORDLONG NumFiles = 0;
DWORD ClusterSize=0;
DWORDLONG NumberOfClusters=0;

//Global Flags
BOOL CRC32Flag = FALSE;
BOOL CRC16Flag = FALSE;
BOOL SpecificDirectoryFlag = FALSE;
BOOL VerboseFlag = FALSE;

//Global Pointers to Strings
char *TargetDirectory=NULL;

int main (int argc, char *argv[])
{
   char LocalDirectory[5] = ".";
   char CurrentDirectory[MAXPATH];
   char *LocalDrive=NULL;
   DWORD SectorsPerCluster;   // address of sectors per cluster 
   DWORD BytesPerSector;   // address of bytes per sector 
   DWORD NumberOfFreeClusters;   // address of number of free clusters  
   DWORD TotalNumberOfClusters;    // address of total number of clusters  

   time_t time1,time2;//used for timers
   
   time1 = time(NULL);
   
   if (ProcessArgs(argc,argv) < 0)
      return(-1); //Encountered an error

   if (SpecificDirectoryFlag==FALSE)
      TargetDirectory=LocalDirectory;

   if (SetCurrentDirectory(TargetDirectory)!=TRUE)
   {
      printf("\nCould not change directory to %s", TargetDirectory);
      return(-1);
   }

   GetCurrentDirectory(MAXPATH, CurrentDirectory);
   LocalDrive = strtok(CurrentDirectory,"\\");
   LocalDrive[2] = '\\';
   LocalDrive[3] = '\0';

   GetDiskFreeSpace(LocalDrive,
                    &SectorsPerCluster,   // address of sectors per cluster 
                    &BytesPerSector,   // address of bytes per sector 
                    &NumberOfFreeClusters,
                    &TotalNumberOfClusters);

   ClusterSize = SectorsPerCluster*BytesPerSector;

   if (VerboseFlag==TRUE)
   {
      printf("\nBytes per Cluster = %d", ClusterSize);
      printf("\n");
   }

   DirectorySize();

   //printf("\nThe total size used by directories is %s bytes",PrintFormattedOutput(NumDirectories*ClusterSize));
   printf("\n\n\tHidden files = %s bytes", PrintFormattedOutput(HiddenSize));
   printf("\n\tSystem files = %s bytes", PrintFormattedOutput(SystemSize));
   printf("\n\tBoth hidden and system files = %s bytes", PrintFormattedOutput(HidAndSysSize));
   printf("\n\tNormal files = %s bytes", PrintFormattedOutput(TotalSize-HiddenSize-SystemSize-HidAndSysSize));
   printf("\n\nThe total size of all files is %s bytes", PrintFormattedOutput(TotalSize));
   printf("\n");

   if (VerboseFlag==TRUE)
   {
      printf("\nThe total number of clusters used is %s", PrintFormattedOutput(NumberOfClusters+NumDirectories));
      printf("\nThe total disk space used by these files is %s bytes", PrintFormattedOutput((NumberOfClusters+NumDirectories)*ClusterSize));
      printf("\nThere are %d clusters left",TotalNumberOfClusters - (NumberOfClusters+NumDirectories));
   //PrintFormattedOutput(TotalSize);

      time2 = time(NULL);
      printf("\nElapsed time = %d seconds", time2-time1);
   }


   return(0);
}

int DirectorySize(void)
{

   WIN32_FIND_DATA fd;
   HANDLE h;
   char szBuffer[MAXPATH+1];
   DWORD NameSize = MAXPATH + 1;
   DWORDLONG TotalDirectorySize = 0;
   BOOL rc = TRUE;
   int crc=0;

   GetCurrentDirectory(NameSize,(char *)&szBuffer);
   
   h = FindFirstFile("*.*", &fd);

   if (h == INVALID_HANDLE_VALUE)
   {
      printf("\nInvalid handle value");
      return(-1);
   }

   do
   {
      if (((stricmp(fd.cFileName,".")==0)||stricmp(fd.cFileName,"..")==0)&&(rc==TRUE))
         continue;

      if (BitCompare(fd.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY)==TRUE)
         DirectoryChange(&fd);
      else
         TotalDirectorySize += FileCalculation(&fd);

   }while (rc = FindNextFile(h,&fd)!= FALSE);

   if (VerboseFlag == TRUE)
   {
      //char *totalDirSize = 
      printf("\nTotal Size of %s = %s bytes",szBuffer, PrintFormattedOutput(TotalDirectorySize));
   }
   else
   {
      printf("\rNumber of files = %I64u Number of subdirectories = %I64u",NumFiles, NumDirectories);
   }
   TotalSize += TotalDirectorySize;

   FindClose(h);

   return(0);
}//end DirectorySize


BOOL BitCompare(DWORD target, DWORD source)
{
   return((target&source) == source);
}

DWORDLONG FileCalculation(WIN32_FIND_DATA *fd)
{
   int crc;
   DWORDLONG FileSize=0;

   NumFiles += 1;

   FileSize = fd->nFileSizeHigh << sizeof(DWORD)*8;
   FileSize += fd->nFileSizeLow;

   NumberOfClusters += FileSize/ClusterSize;
   if ((FileSize % ClusterSize) > 0)
      NumberOfClusters += 1;

   //if (VerboseFlag == TRUE)
   //   printf("\n%s = %lu bytes", fd->cFileName, FileSize);
    
   if (CRC32Flag == TRUE)
   {
      crc = crc32(fd->cFileName);
      if (VerboseFlag == TRUE)
         printf("\nCRC32 for %s = %08lX", fd->cFileName, crc);
   }
   else if (CRC16Flag == TRUE)
   {
      crc = crc16(fd->cFileName);
      if (VerboseFlag == TRUE)
         printf("\nCRC16 for %s = %08lX", fd->cFileName, crc);
   } 


   if ((BitCompare(fd->dwFileAttributes,FILE_ATTRIBUTE_HIDDEN)==TRUE)&&(BitCompare(fd->dwFileAttributes,FILE_ATTRIBUTE_SYSTEM)==TRUE))
      HidAndSysSize += FileSize;
   else if (BitCompare(fd->dwFileAttributes,FILE_ATTRIBUTE_HIDDEN)==TRUE)
      HiddenSize += FileSize;
   else if (BitCompare(fd->dwFileAttributes,FILE_ATTRIBUTE_SYSTEM)==TRUE)
      SystemSize += FileSize;

        CMP3Info mp3Info = new CMP3Info();

        int loadstate = mp3Info->loadInfo(fd->cFileName); 
        
        if (loadstate!=0) return loadstate;

        // the lengths of the following char arrays are
        // all exponents of 2 and the way I figured them
        // out, was by trial and error, so no guarrantue
        // here... :)

        // this is the same values as used inside other
        // functions in the class...
        char tracknumber[4] = {"n/a"};

        char year[8] = {"n/a"};

        char bitrate[16] = {"n/a"};
        char filesize[16] = {"n/a"};
        char frequency[16] = {"n/a"};
        char length[16] = {"n/a"};
        char numberofframes[16] = {"n/a"};

        char album[32] = {"n/a"};
        char artist[32] = {"n/a"};
        char comment[32] = {"n/a"};
        char genre[32] = {"n/a"};
        char mode[32] = {"n/a"};
        char mpegversion[32] = {"n/a"};
        char title[32] = {"n/a"};

        // get version like "MPEG v1.0 Layer III"
        mp3Info->getVersion(mpegversion);

        // get the bitrate like "128 kbps"
        sprintf(bitrate, "%d kbps", mp3Info->getBitrate() );

        // get frequncy like "44100 Hz"
        sprintf(frequency, "%d Hz", mp3Info->getFrequency() );

        // get length like "mm:ss"
        mp3Info->getFormattedLength(length);

        // get file size like "1253 kB"
        sprintf(filesize, "%d kB", (mp3Info->getFileSize()/1024) );

        // get playing mode like "Stereo"
        mp3Info->getMode(mode);

        // get song title like "Californication"
        if ( mp3Info->isTagged() ) mp3Info->getTitle(title);

        // get artist name like "Red Hot Chili Peppers"
        if ( mp3Info->isTagged() ) mp3Info->getArtist(artist);

        // get album name like "One Hot Minute"
        if ( mp3Info->isTagged() ) mp3Info->getAlbum(album);
        
        // get name of genre like "Grunge"
        if ( mp3Info->isTagged() ) mp3Info->getGenre(genre);

        // get comment like "encoded by me..."
        if ( mp3Info->isTagged() ) mp3Info->getComment(comment);

        // get Number of frames like "123432"
        sprintf( numberofframes, "%d", mp3Info->getNumberOfFrames() );

        // get Year like "1999"
        if ( mp3Info->isTagged() ) sprintf( year, "%d", mp3Info->getYear() );

        // get track number (obly for id3v1.1) like "1"
        if ( !(mp3Info->getTrackNumber()==-1) ) sprintf( tracknumber, "%d", mp3Info->getTrackNumber() );









   return(FileSize);
}//end FileCalculation

void DirectoryChange(WIN32_FIND_DATA *fd)
{
   if (SetCurrentDirectory(fd->cFileName)==TRUE)
   {
      NumDirectories += 1;
      DirectorySize();
      SetCurrentDirectory("..");
   }
   else
   {
      printf("\r                                                                                ");
      printf("\rCouldn't change directory to %s\n",fd->cFileName);
   }
}//end DirectoryChange

int ProcessArgs(int argc, char *argv[])
{
      int NumArgs = argc;
      char *s;
      char *s2;

      while (--NumArgs>0)
      {
         s = (++argv)[0]; //increment to the next argument
         
         switch(*s)
         {
            //Process all the flags starting with /
            case '/':
            {
               switch(*(++s))
               {
                  case 'v':
                  case 'V':
                     VerboseFlag = TRUE;
                     break;
                  case 'c':
                  case 'C':
                     if ((CRC32Flag==TRUE)||(CRC16Flag==TRUE))
                     {
                        ErrorMessage(TOO_MANY_CRC_FLAGS);
                        return(-1);
                     }
                     s2 = s;
                     if (stricmp(++s2,"32")==0)
                     {
                        CRC32Flag = TRUE;
                        break;
                     }
                     else if(stricmp(s2,"16")==0)
                     {
                        CRC16Flag = TRUE;
                        break;
                     }
                  //Go directly to default if neither
                  default:
                     printf("\nUnknown parameter %s", --s);
                     ErrorMessage(BAD_PARAM);
                     return(-1);
               }//end switch
               break;
            }//end case '/'
            //Assume a parameter without / is the Target directory
            default:
               if ((stricmp(".",s)==0)||(stricmp("..",s)==0))
               {
                  ErrorMessage(BAD_DIRECTORY_NAME);
                  return(-1);
               }

               if (SpecificDirectoryFlag == TRUE)
               {
                  printf("\nTarget directory = %s", s);
                   ErrorMessage(TOO_MANY_DIRECTORIES_SPECIFIED);
                  return(-1);
               }
               TargetDirectory = s;
               printf("\nTarget directory = %s", TargetDirectory);
               SpecificDirectoryFlag = TRUE;
               break;
         }//end switch (*s)
      }//end while
return(0);

}//end ProcessArgs

void ErrorMessage(int ErrNum)
{
   switch(ErrNum)
   {
   case BAD_DIRECTORY_NAME:
      printf("\nError:  Cannot use \".\" or \"..\" as target directory.");
      break;
   case TOO_MANY_DIRECTORIES_SPECIFIED:
      printf("\nError:  Too many target directories specified.");
      break;
   case BAD_PARAM:
      printf("\nError:  Unsupported parameter.");
      break;
   case TOO_MANY_CRC_FLAGS:
      printf("\nError:  Too many CRC parameters specified.");
      break;
   default:
      printf("\nUnknown Error %s.", ErrNum);
      break;
   }

   printf("\n\nDIRSIZE Version %.2f\nDIRSIZE [directory] [/C32|/C16] [/V]",VERSION);
   
   printf("\n\n\t[directory]\tTo specify a particular directory.");  
   printf("\n\t\t\tAssumes current directory otherwise.");
   printf("\n\t/C32\t\tFor a CRC32 check on each file.");
   printf("\n\t/C16\t\tFor a CRC16 check on each file.");
   printf("\n\t/V\t\tFor a verbose listing of every file.");


}//end ErrorMessage


char *PrintFormattedOutput(DWORDLONG Size)
{
   char *OutputString=NULL;
   char *BackwardsString= (char *) calloc(1,256);
   int i=0,q=0,digit=0;
   DWORDLONG Size2 = Size;

   q = (int) Size2 - (Size2/10) * 10;
   Size2 = Size2 / 10;
   BackwardsString[i++] = '0' + q;
   digit += 1;

   while (Size2 > 0)
   {
      if (digit%3 == 0)
      {
         BackwardsString[i++] = ',';
      }

      q = (int) Size2 - (Size2/10) * 10;
      BackwardsString[i++] = '0' + q;
      Size2 = (DWORDLONG) Size2 / 10;
      digit += 1;
   }
   BackwardsString[i] = '\0';
   OutputString = strrev(BackwardsString);
   return(OutputString);


}//end PrintFormattedOutput
