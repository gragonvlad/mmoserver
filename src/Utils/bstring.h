/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2008 The swgANH Team

---------------------------------------------------------------------------------------
*/

#ifndef ANH_UTILS_BSTRING_H
#define ANH_UTILS_BSTRING_H

#include <vector>

class BString;

typedef std::vector<BString> BStringVector;

//======================================================================================================================

#define BSTRING_ALLOC_BLOCK_SIZE      64

enum BStringType
{
  BSTRType_ANSI,
  BSTRType_Unicode16,
  BSTRType_UTF8
};

//======================================================================================================================

class BString
{
public:
    BString();
    BString(BStringType type, uint16 length);
    ~BString();

    BString(const int8* data);
    BString(const uint16* data);
    BString(const wchar_t* data);
    BString(const BString& data);

		operator int8 * ();

    uint16 initRawBSTR(int8* data, BStringType type);

    // Operator overloads
    bool operator ==(char* string);
    bool operator ==(const char* data);
    bool operator ==(BString& data);

    BString& operator =(const int8* data);
  
    BString& operator =(int8* data);
    BString& operator =(const uint16* data);
    BString& operator =(const wchar_t* data);
    BString& operator =(const BString& data);
  
    BString& operator <<(const int8* data);
    BString& operator <<(int8* data);

    // Accessor Methods
    const int8* getAnsi() const;
   // uint16* getUnicode16();
    wchar_t* getUnicode16();
    int8*   getUTF8();

    void convert(BStringType type);
    void substring(BString& dest, uint16 start, uint16 end);

    int  split(BStringVector& retVec,char delimiter);
    void toLower();
    void toUpper();
    void toUpperFirst();
    void toLowerFirst();
    bool isNumber();

    void setLength(uint16 length);
    void setType(BStringType type);

    uint16 getLength() const;
    uint32 getDataLength() const;
    uint32 getCharacterWidth() const;

    BStringType getType() const;
    int8* getRawData() const;
    uint32 getAllocated() const;
    uint32 getCrc();

    static uint32 CRC(char* data);

private:
    void _allocate();

    uint16 mLength;          // Length of the string itself.  BStrings are NOT null terminated
    uint16 mAllocated;       // Length of the allocated buffer which should be a multiple of
                                                        // BSTRING_ALLOC_BLOCK_SIZE.
    uint16 mCharacterWidth;  // Size of a single character in bytes
    BStringType mType;            // What format is the current string in.
    int8* mString;          // Pointer to the allocated buffer

    static uint32 mCrcTable[256];
};

#endif //MMOSERVER_UTILS_BSTRING_H


