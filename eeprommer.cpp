// eeprommer
//
// Quick command-line utility for Windows, for reading ROMs or reading/writing EEPROMs with my
// Arduino-based programmer.
//
// http://danceswithferrets.org/geekblog/?p=903
//
// This utility is specific to Windows (mostly because of how it accesses the serial port). However,
// it might be useful as reference for anyone making their own utility.
//
// It's also good programming reference, if you want to see what lazy code looks like. :)
//

#include "stdafx.h"
#include <Windows.h>
#include "eeprommer.h"

HANDLE g_hComm;
ROMSize g_eROMSize;
Mode g_eMode;
int g_iComPortNumber;
bool g_bVerify;
bool g_bUnprotect;
bool g_bProtect;
bool g_bIsFlashMemory;

char g_Filename[255];

void SetROMSize(char* word)
{
    if (!strcmpi(word, "1K"))    { g_eROMSize = Size_1K;    return; }
    if (!strcmpi(word, "2K"))    { g_eROMSize = Size_2K;    return; }
    if (!strcmpi(word, "4K"))    { g_eROMSize = Size_4K;    return; }
    if (!strcmpi(word, "8K"))    { g_eROMSize = Size_8K;    return; }
    if (!strcmpi(word, "16K"))   { g_eROMSize = Size_16K;   return; }
    if (!strcmpi(word, "32K"))   { g_eROMSize = Size_32K;   return; }
    if (!strcmpi(word, "64K"))   { g_eROMSize = Size_64K;   return; }
    //if (!strcmpi(word, "128K"))  { g_eROMSize = Size_128K;  return; }
    //if (!strcmpi(word, "256K"))  { g_eROMSize = Size_256K;  return; }
    //if (!strcmpi(word, "512K"))  { g_eROMSize = Size_512K;  return; }
    //if (!strcmpi(word, "1024K")) { g_eROMSize = Size_1024K; return; }
}

int main(int argc, char** argv)
{
    g_hComm = INVALID_HANDLE_VALUE;
    g_eMode = Mode_Unset;
    g_eROMSize = Size_Unknown;
    g_iComPortNumber = -1;
    g_Filename[0] = 0;
    g_bVerify = false;
    g_bUnprotect = false;
    g_bProtect = false;
    g_bIsFlashMemory = false;

    bool bWriteCacheFileAfter = false;

    if (argc == 1)
    {
        // executed with no parameters. Just print help and then exit
        printf("EEPROMmer 10th Dec 2017. http://danceswithferrets.org/geekblog/?p=903\n\n");
        printf("Syntax: eeprommer -romsize <size in Kbytes> -comport <num> [-blank] [-isflash] [-write <filename>]\n");
        printf("                  [-diff <filename>] [-read <filename>] [-verify] [-unprotect] [-protect]\n\n");
        printf("Where:\n\n");
        printf("  -romsize <size>   - can be 1K, 2K, 4K, 8K, 16K, 32K or 64K\n");
        printf("  -comport <num>    - for example, use 11 to access COM11\n");
        printf("  -blank            - writes 0xFFs to the EEPROM\n");
        //printf("  -isflash          - use this if EEPROM is ATF0x0 flash memory\n");
        printf("  -write <filename> - writes a binary file to the EEPROM. If the ROM is larger than the file, then\n");
        printf("                      the file will be repeated. This switch may be used multiple times to\n");
        printf("                      append ROM images together on a larger EEPROM\n");
        printf("  -read <filename>  - reads EEPROM or ROM into binary file\n");
        printf("  -diff <filename>  - when writing, compare against cached ROM image file and only send diffs\n");
        printf("  -unprotect        - clear write-protect bit (Atmel) before writing\n");
        printf("  -protect          - set write-protect bit (Atmel) after writing\n");
        printf("  -verify           - if writing or blanking, this will read the data back as it writes\n\n");
        return(0);
    }

    char cachefilename[255];
    *cachefilename = 0;
    static const int kWorkspaceSize = 1024 * 1024;
    unsigned char* pWorkspace = (unsigned char*)malloc(kWorkspaceSize);
    unsigned char* pCachedWorkspace = (unsigned char*)malloc(kWorkspaceSize);
    char poo[] = { "DEAD" };
    for (int x = 0; x < kWorkspaceSize; ++x)
    {
        pWorkspace[x] = poo[x&3];
        pCachedWorkspace[x] = poo[x&3];
    }

    int iBytesWrittenToWorkspace = 0;

    bool bDiffAgainstCache = false;

    // parse any passed parameters ...
    for (int i = 1; i < argc; ++i)
    {
        //if (!strcmpi(argv[i], "-isflash"))
        //{
        //    g_bIsFlashMemory = true;
        //    continue;
        //}

        if (!strcmpi(argv[i], "-verify"))
        {
            g_bVerify = true;
            continue;
        }

        if (!strcmpi(argv[i], "-unprotect"))
        {
            g_bUnprotect = true;
            continue;
        }

        if (!strcmpi(argv[i], "-protect"))
        {
            g_bProtect = true;
            continue;
        }

        if (!strcmpi(argv[i], "-romsize"))
        {
            SetROMSize(argv[i+1]);
            i++;
            continue;
        }

        if (!strcmpi(argv[i], "-comport"))
        {
            g_iComPortNumber = atoi(argv[i+1]);
            i++;
            continue;
        }
        
        if (!strcmpi(argv[i], "-read"))
        {
            sprintf_s(g_Filename, 255, "%s", argv[i+1]);
            g_eMode = Mode_Read;
            ++i;
            continue;
        }

        if (!strcmpi(argv[i], "-diff"))
        {
            char fn[255];
            sprintf_s(fn, 255, "%s", argv[i+1]);
            int x = strlen(fn);
            sprintf_s(&fn[x], 255-x, ".cachedrom");
            FILE* h = fopen(fn, "rb");
            if (h)
            {
                x = 0;
                unsigned char c = fgetc(h);
                while (!feof(h) && x < kWorkspaceSize)
                {
                    pCachedWorkspace[x++] = c;
                    c = fgetc(h);
                }
                bDiffAgainstCache = true;
                fclose(h);
            }
			sprintf_s(cachefilename, 255, "%s", fn); // keep a copy of this filename, as we'll write-out a new cache at the end
            bWriteCacheFileAfter = true;
        }
        
        if (!strcmpi(argv[i], "-write"))
        {
            char fn[255];
            sprintf_s(fn,255,  "%s", argv[i+1]);
            g_eMode = Mode_Write;
            ++i;

            FILE* h = fopen(fn, "rb");
            if (h)
            {
                printf("Opened %s. Storing at offset %08x ...\n", fn, iBytesWrittenToWorkspace);
                int iCount = 0;
                unsigned char c = fgetc(h);
                while (!feof(h) && iBytesWrittenToWorkspace < kWorkspaceSize)
                {
                    pWorkspace[iBytesWrittenToWorkspace++] = c;
                    c = fgetc(h);
                    ++iCount;
                }

                fclose(h);
                printf("Read %x bytes.\n", iCount);
            }
            else
            {
                printf("Couldn't open file %s.\n", fn);
                return(0);
            }

            continue;
        }

        if (!strcmpi(argv[i], "-blank"))
        {
            g_eMode = Mode_Blank;
            continue;
        }
    }

    if (g_eMode == Mode_Unset && (g_bProtect || g_bUnprotect))
    {
        // JUST doing a protect or unprotect - not doing any reading or writing.
        g_eMode = Mode_Misc;
    }

    if (g_eMode == Mode_Unset)
    {
        printf("No mode specified!\n");
        return(0);
    }

    if (g_eROMSize == Size_Unknown && g_bProtect == false && g_bUnprotect == false)
    {
        printf("ROM size not specified!\n");
        return(0);
    }

    if (g_iComPortNumber == -1)
    {
        printf("COM port number not specified!\n");
        return(0);
    }
	
	if (g_bVerify && bDiffAgainstCache)
	{
		g_bVerify = false;
	}

    if (!SetupSerial(g_iComPortNumber))
    {
        printf("Failed to initialise com port!\n");
        return(0);
    }

    char text[255];

    SendString("V\n"); // request version
    if (ReadString(text, 255))
    {
        //printf("Version string from hardware: %s\n", text);
    }

    if (g_bUnprotect)
    {
        printf("Clearing write-protect status (SDP) ...\n");
        SendString("U\n");

        *text = 0;
        while (*text == 0)
        {
            ReadString(text, 255);
        }

        if (text[0] == 'O' && text[1] == 'K')
        {
            printf("Done.\n");
        } 
        else
        {
            printf("Hmm ... command not accepted. Should you upgrade the firmware on your Arduino?\n");
            printf("(Return message was '%s')\n", text);
            exit(0);
        }
    }

    if (g_eMode == Mode_Write && g_bIsFlashMemory)
    {
        // we need to erase the memory before writing it!
        printf("Sending 'erase' command ...\n");
        SendString("E\n");

        *text = 0;
        while (*text == 0)
        {
            ReadString(text, 255);
        }

        if (text[0] == 'O' && text[1] == 'K')
        {
            printf("Done.\n");
        } 
        else
        {
            printf("Hmm ... 'erase' command not accepted. Should you upgrade the firmware on your Arduino?\n");
            printf("(Return message was '%s')\n", text);
            exit(0);
        }
    }

    if (g_eMode != Mode_Misc)
    {
        if (g_eMode == Mode_Read) printf("Reading");
        if (g_eMode == Mode_Write) printf("Writing");
        if (g_eMode == Mode_Blank) printf("Blanking");
        printf(" %dK bytes ...\n", (g_eROMSize >> 10));

        int addr = 0;
        while (addr < g_eROMSize)
        {
            if (g_eMode == Mode_Write || g_eMode == Mode_Blank)
            {
                bool bSkipTheseSixteen = false;

                if (bDiffAgainstCache)
                {
                    // we're about to write sixteen bytes to the ROM. But we should check if those sixteen bytes match
                    // those in the cache. If they do, we can skip them.
                    bSkipTheseSixteen = true;
                    for (int offset = 0; offset < 16; ++offset)
                    {
                        if (pWorkspace[addr + offset] != pCachedWorkspace[addr + offset])
                        {
                            bSkipTheseSixteen = false;
                        }
                    }
                }

                if (bSkipTheseSixteen == false)
                {
                    sprintf_s(text, 255, "%c%04x:", (g_bIsFlashMemory)?'F':'W', addr);
                    unsigned char checksum = 0;
                    for (int offset = 0; offset < 16; ++offset)
                    {
                        int x = strlen(text);
                        sprintf_s(&text[x], 255-x, "%02x", pWorkspace[addr + offset]);
                        checksum = checksum ^ pWorkspace[addr + offset];
                    }
                    int x = strlen(text);
                    sprintf_s(&text[x], 255-x, ",%02x\n", checksum);
                    //printf("Sending: %s", text);

                    SendString(text);

                    bool goodtogo = false;
                    while (!goodtogo)
                    {
                        ReadString(text, 255);
                        if (text[0] == 'O' && text[1] == 'K')
                        {
                            goodtogo = true;
                        }
                    }
                }
            }

            if (g_eMode == Mode_Read || g_bVerify)
            {
                sprintf_s(text, 255, "R%04x\n", addr);
                SendString(text);
                bool failed = false;
                do
                {
                    ReadString(text, 255);
                    int iColonPos = 0;
                    while (text[iColonPos] && text[iColonPos] != ':') ++iColonPos;
                    // todo here: decode address returned, and double-check it matches with the address we requested!
                    if (text[iColonPos] == ':')
                    {
                        //printf("%s\n", text);
                        if (g_bVerify)
                        {
                            unsigned char tempspace[20];
                            int bytesdecoded = DecodeStringInto(&text[iColonPos], tempspace, 16);
                            if (bytesdecoded == -1)
                            {
                                // decoded it, but checksum failed. Ask again.
                                failed = true;
                                printf("Checksum fail at address $%04x. Retrying.\n", addr);
                                bDiffAgainstCache = false; // so that we don't write this back as a "good" cache file
                            }
                            else
                            {
                                // decoded OK. Compare it against our workspace
                                bool bOk = true;
                                for (int x = 0; x < bytesdecoded && !failed; ++x)
                                {
                                    if (pWorkspace[addr + x] != tempspace[x])
                                    {
                                        failed = true;
                                        printf("Verification failed around address $%04x.\n", addr + x);
                                        bDiffAgainstCache = false; // so that we don't write this back as a "good" cache file
                                    }
                                }
                            }
                        }
                        else
                        {
                            // not verifying, just reading
                            if (DecodeString(text, pWorkspace) == false)
                            {
                                // decoded it, but checksum failed. Ask for it again.
                                failed = true;
                                printf("Checksum fail at address $%04x. Retrying.\n", addr);
                            }
                        }
                    }

                } while (text[0] != 'O' && text[0] != 'E');
            }

            addr += 16;
            if ((addr & 0x3FF) == 0)
            {
                printf("Done %dK.\n", (addr >> 10));
            }
        }
    }

    if (g_bProtect)
    {
        printf("Setting write-protect status (SDP) ...\n");
        SendString("P\n");

        *text = 0;
        while (*text == 0)
        {
            ReadString(text, 255);
        }

        if (text[0] == 'O' && text[1] == 'K')
        {
            printf("Done.\n");
        } 
        else
        {
            printf("Hmm ... command not accepted. Should you upgrade the firmware on your Arduino?\n");
            // (no point exiting here, as we're done anyway.)
        }
    }

    printf("Closing COM port ...\n");
    CloseHandle(g_hComm);

    if (g_eMode == Mode_Read)
    {
        FILE* h = fopen(g_Filename, "wb");
        if (h)
        {
            for (int x = 0; x < g_eROMSize; ++x)
            {
                fputc(pWorkspace[x], h);
            }
            fclose(h);
        }
    }

    if (bWriteCacheFileAfter && cachefilename[0] && g_eMode == Mode_Write)
    {
        // we have a cache file to write!
        FILE* h = fopen(cachefilename, "wb");
        if (h)
        {
            for (int x = 0; x < g_eROMSize; ++x)
            {
                fputc(pWorkspace[x], h);
            }

            fclose(h);
        }
    }

    free(pWorkspace);
    free(pCachedWorkspace);

	return 0;
}

bool SetupSerial(int comport)
{
    TCHAR szPort[32];
    szPort[0] = _T('\0');
    _stprintf_s(szPort, _T("\\\\.\\COM%u"), comport);

    g_hComm = CreateFile(szPort, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (g_hComm == INVALID_HANDLE_VALUE)
    {
        //printf("Failed to open COM port.\n");
        return(false);
    }
    
    DCB dcb;
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(g_hComm, &dcb))
    {
        //printf("GetCommState failed.\n");
        CloseHandle(g_hComm);
        return(false);
    }

    dcb.BaudRate = CBR_9600;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    dcb.ByteSize = 8;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = TRUE;
    dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;

    if (!SetCommState(g_hComm, &dcb))
    {
        printf("SetCommState failed!\n");
        CloseHandle(g_hComm);
        return(false);
    }

    return true;
}

void SendString(char* text)
{
    int l = strlen(text);
    DWORD byteswritten = 0;
    WriteFile(g_hComm, text, l, &byteswritten, NULL);
}

int ReadString(char* text, int maxlen)
{
    int bytesread = 0;

    DWORD dw;
    do
    {
        dw = 0;

        if (ReadFile(g_hComm,         // handle of file to read
            &text[bytesread],           // pointer to buffer
            1,         // max number of bytes to read
            &dw,     // pointer to number of bytes read
            NULL) == 0)     // pointer to structure for data
        {
            return 0;
            //AfxMessageBox("Reading of serial communication has problem.");
            //return FALSE;
        }

        if (dw == 0) break;
        bytesread += dw;
    } while (bytesread < maxlen && text[bytesread-dw] != 0 && text[bytesread-dw]!=13 && text[bytesread-dw]!=10);

    int z = bytesread - 1;
    while (z >= 0 && text[z]<32) { text[z--] = 0; }

    return z;
}

char HexToVal(char b)
{
  if (b >= '0' && b <= '9') return(b - '0');
  if (b >= 'A' && b <= 'F') return((b - 'A') + 10);
  if (b >= 'a' && b <= 'f') return((b - 'a') + 10);
  return(0);
}

// should return true if the string was decoded, and the checksum (if there was one)
// passed. Should return false if the string was badly formed or if the checksum
// failed.
bool DecodeString(char* text, unsigned char* pData)
{
    int addr = 0;
    int x = 0;

    while (text[x] && text[x] != ':')
    {
        addr = addr << 4;
        addr |= HexToVal(text[x++]);
    }

    if (text[x] != ':') return(false);

    ++x; // now pointing to beginning of data

    int bytesdecoded = DecodeStringInto(&text[x], &pData[addr], 16);

    if (bytesdecoded == -1) return(false); // checksum failure

    return(true);
}

int DecodeStringInto(char* text, unsigned char* pData, int maxlen)
{
    unsigned char chk = 0;
    int addr = 0;
    int x = 0;
    while (text[x] && text[x + 1] && text[x] != ',' && addr < maxlen)
    {
        pData[addr] = (HexToVal(text[x]) << 4) | HexToVal(text[x+1]);
        chk = chk ^ pData[addr];
        x += 2;
        addr++;
    }

    if (text[x] == ',' && text[x + 1] && text[x + 2])
    {
        // ooo! Checksum!
        x++;
        unsigned char their_checksum = (HexToVal(text[x]) << 4) | HexToVal(text[x+1]);

        if (their_checksum != chk)
        {
            return(-1);
        }
    }

    return(addr);
}
