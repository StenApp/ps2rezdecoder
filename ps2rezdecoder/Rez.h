#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <cstdint>

typedef uint32_t uint;

// Pascally-cased, because of habit. Sorry.
struct Header {
    uint unknown;
    uint iTotalFiles;
    uint iMagicNumber;
    uint iDataOffset;
    uint iTotalBlocks;
};

struct FileData {
    uint iNameHash;
    uint iOffset;
    uint iSize;
    uint iXSize;
};

struct BlockData {
    char padding[736];
    std::vector<FileData> Files;
    // Fast lookup: hash -> FileData
    std::unordered_map<uint, FileData> HashMap;
};

//
// Thanks to https://zenhax.com/viewtopic.php?t=11139 and aluigi for his original extraction script!
//
class Rez
{
public:
    Rez();
    ~Rez();

    // desiredBlock: which language block to load (0-4). Default 0 = English
    bool Open(const std::string& file, int desiredBlock = 0);
    bool Close();

    bool Extract(const std::string& filename, uint hash, int blockNumber = 0);

    uint GetMagicNumber() const { return m_Header.iMagicNumber; }
    int  GetLoadedBlock() const { return m_LoadedBlock; }
    size_t GetFileCount() const;

protected:
    Header m_Header;
    BlockData m_Block;          // Only one block is kept in memory
    int m_LoadedBlock;          // Which block is currently loaded
    std::ifstream m_Stream;

    // Reusable buffer for extraction (avoids many small allocations)
    std::vector<char> m_ExtractBuffer;
};
