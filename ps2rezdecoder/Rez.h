#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

typedef uint32_t uint;

struct Header {
    uint unknown;
    uint iTotalFiles;
    uint iMagicNumber;
    uint iDataOffset;
    uint iTotalBlocks;
};

struct FileData {
    uint iNameHash;
    uint iOffset;   // in 2048-byte units
    uint iSize;
    uint iXSize;
};

class Rez
{
public:
    static const uint TABLE_START    = 0x800;
    static const uint TABLE_STRIDE   = 0x19000;
    static const uint ENTRY_SIZE     = 16;
    static const int  LANGUAGE_COUNT = 5;

    Rez();
    ~Rez();

    // language: 0..4
    bool Open(const std::string& file, int language = 0);
    bool Close();

    // Extract a single file by hash. Returns true if found and written.
    bool Extract(const std::string& filename, uint hash);

    // Extract every hash that is present in the loaded language group
    // but was NOT requested via Extract(). Writes them as unknown_XXXXXXXX.bin
    void ExtractUnknowns(const std::string& outDir = "out/unknown");

    uint GetMagicNumber() const { return m_Header.iMagicNumber; }
    int  GetLoadedLanguage() const { return m_LoadedLanguage; }
    size_t GetFileCount() const { return m_HashMap.size(); }

    // Mark a hash as "already extracted" (used by the main loop)
    void MarkExtracted(uint hash) { m_Extracted.insert(hash); }

protected:
    Header m_Header;
    std::unordered_map<uint, FileData> m_HashMap;
    std::unordered_set<uint> m_Extracted;
    int m_LoadedLanguage = -1;
    std::ifstream m_Stream;
    std::vector<char> m_ExtractBuffer;

    bool WriteFileData(const FileData& fd, const std::string& outputPath);
};
