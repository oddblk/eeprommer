// eeprommer.h

#pragma once

enum ROMSize // code assumes that this value denotes BYTES, not bits
{
    Size_Unknown = 0,
    Size_1K = 1 * 1024,
    Size_2K = 2 * 1024,
    Size_4K = 4 * 1024,
    Size_8K = 8 * 1024,
    Size_16K = 16 * 1024,
    Size_32K = 32 * 1024,
    Size_64K = 64 * 1024,
    //Size_128K = 128 * 1024,
    //Size_256K = 256 * 1024,
    //Size_512K = 512 * 1024,
    //Size_1024K = 1024 * 1024,
};

enum Mode
{
    Mode_Unset,
    Mode_Read,
    Mode_Write,
    Mode_Blank,
    Mode_Misc, // set this when we're doing something trivial, like a protect/unprotect ONLY
};

bool SetupSerial(int comport);
void SendString(char* text);
int ReadString(char* text, int maxlen);
bool DecodeString(char* text, unsigned char* pData);
int DecodeStringInto(char* text, unsigned char* pData, int maxlen);
