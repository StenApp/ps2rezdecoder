#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

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
    uint iOffset;   // Offset in 2048-byte units
    uint iSize;
    uint iXSize;
};

struct RezEntry {
    FileData data;
    uint32_t tableIndex;
};

enum class ExtractResult {
    Written,
    HashNotFound,
    LanguageEntryNotFound,
    ZeroSize,
    WriteFailed
};

struct UnknownStats {
    size_t written = 0;
    size_t zeroSize = 0;
    size_t failed = 0;
};

class Rez
{
public:
    static const uint TABLE_START = 0x800;
    static const uint TABLE_STRIDE = 0x19000;
    static const uint ENTRY_SIZE = 16;
    static const int LANGUAGE_COUNT = 5;

    Rez();
    ~Rez();

    // language: 0..4
    bool Open(const std::string& file, int language = 0);
    bool Close();

    // Extract a known file and report its result.
    ExtractResult Extract(const std::string& filename, uint hash);

    // Extract unknown hashes and return their statistics.
    void ExtractUnknowns(
        const std::string& outDir,
        UnknownStats& stats);

    // Convenience overload for callers that do not need statistics.
    void ExtractUnknowns(
        const std::string& outDir = "out/unknown");

    uint GetMagicNumber() const {
        return m_Header.iMagicNumber;
    }

    int GetLoadedLanguage() const {
        return m_LoadedLanguage;
    }

    size_t GetFileCount() const {
        return m_HashMap.size();
    }

    // Retained for compatibility with existing callers.
    void MarkExtracted(uint hash) {
        m_Extracted.insert(hash);
    }

    void DebugHash(uint hash) const;

protected:
    Header m_Header{};

    std::unordered_map<uint, std::vector<RezEntry>> m_HashMap;

    // Hashes whose known file-list entry matched the selected language.
    // These are excluded from the unknown dump even if writing failed.
    std::unordered_set<uint> m_Requested;

    // Hashes successfully written as known files.
    std::unordered_set<uint> m_Extracted;

    int m_LoadedLanguage = -1;

    std::ifstream m_Stream;
    std::vector<char> m_ExtractBuffer;

    bool WriteFileData(
        const FileData& fd,
        const std::string& outputPath);
};