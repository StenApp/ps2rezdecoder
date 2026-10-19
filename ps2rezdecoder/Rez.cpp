#include "Rez.h"
#include <windows.h>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>

// ------------------------------------------------------------------
// Small helpers (from original NOLF WinUtil)
// ------------------------------------------------------------------
static BOOL DirExist(char* strPath)
{
    if (!strPath || !*strPath) return FALSE;

    BOOL bRemovedBackSlash = FALSE;
    size_t len = strlen(strPath);
    if (strPath[len - 1] == '/' || strPath[len - 1] == '\\') {
        strPath[len - 1] = '\0';
        bRemovedBackSlash = TRUE;
    }

    UINT oldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    struct stat statbuf;
    int error = stat(strPath, &statbuf);
    SetErrorMode(oldErrorMode);

    if (bRemovedBackSlash)
        strPath[strlen(strPath)] = '\\';

    return (error != -1) ? TRUE : FALSE;
}

static BOOL CreateDir(char* strPath)
{
    if (DirExist(strPath)) return TRUE;
    if (strPath[strlen(strPath) - 1] == ':') return FALSE;

    char strPartialPath[MAX_PATH];
    strPartialPath[0] = '\0';

    // Work on a copy so we don't destroy the original
    char temp[MAX_PATH];
    strncpy_s(temp, strPath, MAX_PATH - 1);

    char* context = nullptr;
    char* token = strtok_s(temp, "/\\", &context);
    while (token) {
        strcat_s(strPartialPath, MAX_PATH, token);
        if (!DirExist(strPartialPath) && strPartialPath[strlen(strPartialPath) - 1] != ':') {
            if (!CreateDirectoryA(strPartialPath, NULL)) return FALSE;
        }
        strcat_s(strPartialPath, MAX_PATH, "\\");
        token = strtok_s(nullptr, "/\\", &context);
    }
    return TRUE;
}

// ------------------------------------------------------------------
// Rez implementation
// ------------------------------------------------------------------
Rez::Rez()
{
    m_ExtractBuffer.reserve(4 * 1024 * 1024);
}

Rez::~Rez()
{
    Close();
}

bool Rez::Open(const std::string& file, int language)
{
    if (language < 0 || language >= LANGUAGE_COUNT) {
        std::cerr << "ERROR: language must be 0..4\n";
        return false;
    }

    Close();

    m_Stream.open(file, std::ios::binary);
    if (!m_Stream.is_open()) {
        std::cerr << "ERROR: cannot open " << file << "\n";
        return false;
    }

    m_Stream.read(reinterpret_cast<char*>(&m_Header), sizeof(m_Header));
    if (!m_Stream) {
        std::cerr << "ERROR: failed to read header\n";
        return false;
    }

    std::cout << "Header: files=" << m_Header.iTotalFiles
              << " blocks=" << m_Header.iTotalBlocks
              << " magic=" << m_Header.iMagicNumber << "\n";

    m_HashMap.clear();
    m_HashMap.reserve(m_Header.iTotalFiles);
    m_Extracted.clear();

    // Load every table belonging to this language group
    // (language, language+5, language+10, ...)
    for (uint t = static_cast<uint>(language); t < m_Header.iTotalBlocks; t += LANGUAGE_COUNT)
    {
        const uint tableOffset = TABLE_START + t * TABLE_STRIDE;
        m_Stream.seekg(tableOffset);

        for (uint i = 0; i < m_Header.iTotalFiles; ++i)
        {
            FileData fd{};
            m_Stream.read(reinterpret_cast<char*>(&fd), sizeof(fd));
            if (!m_Stream) {
                std::cerr << "ERROR: failed reading table " << t << " entry " << i << "\n";
                return false;
            }

            // Keep the first occurrence of each hash
            if (m_HashMap.find(fd.iNameHash) == m_HashMap.end()) {
                m_HashMap.emplace(fd.iNameHash, fd);
            }
        }
    }

    m_LoadedLanguage = language;
    std::cout << "Loaded language group " << language
              << " (" << m_HashMap.size() << " unique hashes)\n";
    return true;
}

bool Rez::Close()
{
    if (m_Stream.is_open())
        m_Stream.close();
    m_HashMap.clear();
    m_Extracted.clear();
    m_LoadedLanguage = -1;
    return true;
}

bool Rez::WriteFileData(const FileData& fd, const std::string& outputPath)
{
    // Create directories
    size_t lastSlash = outputPath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        std::string dir = outputPath.substr(0, lastSlash);
        char dirBuf[MAX_PATH];
        strncpy_s(dirBuf, dir.c_str(), MAX_PATH - 1);
        if (!CreateDir(dirBuf)) {
            std::cerr << "ERROR: cannot create directory for " << outputPath << "\n";
            return false;
        }
    }

    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "ERROR: cannot create " << outputPath << "\n";
        return false;
    }

    const uint64_t dataPos = static_cast<uint64_t>(fd.iOffset) * 2048ull;
    m_Stream.seekg(static_cast<std::streamoff>(dataPos));
    if (!m_Stream) {
        std::cerr << "ERROR: seek failed for " << outputPath << "\n";
        return false;
    }

    uint remaining = fd.iSize;
    constexpr size_t kBufSize = 64 * 1024;
    if (m_ExtractBuffer.size() < kBufSize)
        m_ExtractBuffer.resize(kBufSize);

    while (remaining > 0) {
        const size_t toRead = (std::min)(static_cast<size_t>(remaining), kBufSize);
        m_Stream.read(m_ExtractBuffer.data(), static_cast<std::streamsize>(toRead));
        const auto bytesRead = m_Stream.gcount();
        if (bytesRead <= 0)
            break;
        out.write(m_ExtractBuffer.data(), bytesRead);
        remaining -= static_cast<uint>(bytesRead);
    }

    return remaining == 0;
}

bool Rez::Extract(const std::string& filename, uint hash)
{
    auto it = m_HashMap.find(hash);
    if (it == m_HashMap.end())
        return false;

    std::string outputPath = "out/" + filename;
    // Normalise slashes
    for (char& c : outputPath)
        if (c == '/') c = '\\';

    bool ok = WriteFileData(it->second, outputPath);
    if (ok)
        m_Extracted.insert(hash);
    return ok;
}

void Rez::ExtractUnknowns(const std::string& outDir)
{
    char dirBuf[MAX_PATH];
    strncpy_s(dirBuf, outDir.c_str(), MAX_PATH - 1);
    CreateDir(dirBuf);

    size_t count = 0;
    for (const auto& pair : m_HashMap) {
        if (m_Extracted.count(pair.first))
            continue; // already extracted by name

        std::ostringstream name;
        name << outDir << "\\unknown_"
             << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
             << pair.first << ".bin";

        if (WriteFileData(pair.second, name.str()))
            ++count;
    }

    std::cout << "Extracted " << count << " unknown files to " << outDir << "\n";
}
