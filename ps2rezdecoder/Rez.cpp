#include "Rez.h"
#include <windows.h>
#include <iostream>
#include <algorithm>
#include <cstring>

// ------------------------------------------------------------------
// Constructor / Destructor
// ------------------------------------------------------------------
Rez::Rez()
    : m_Header{}
    , m_LoadedBlock(-1)
{
    // Pre-allocate a reasonable buffer (4 MB). Will grow if needed.
    m_ExtractBuffer.reserve(4 * 1024 * 1024);
}

Rez::~Rez()
{
    Close();
}

// ------------------------------------------------------------------
// Open – only loads the requested language block
// ------------------------------------------------------------------
bool Rez::Open(const std::string& file, int desiredBlock)
{
    m_Stream.open(file, std::ios_base::binary);
    if (!m_Stream.is_open()) {
        return false;
    }

    // Read header
    m_Stream.read(reinterpret_cast<char*>(&m_Header), sizeof(m_Header));
    if (!m_Stream) {
        return false;
    }

    if (desiredBlock < 0 || desiredBlock >= static_cast<int>(m_Header.iTotalBlocks)) {
        std::cerr << "ERROR: Invalid block number " << desiredBlock
                  << " (valid range: 0 .. " << (m_Header.iTotalBlocks - 1) << ")\n";
        return false;
    }

    // Skip to the start of the block tables (original magic offset)
    m_Stream.seekg(1312);

    // Skip all blocks before the one we want
    for (int i = 0; i < desiredBlock; ++i) {
        // Skip padding
        m_Stream.seekg(736, std::ios_base::cur);

        // Skip file table
        const std::streamoff fileTableSize =
            static_cast<std::streamoff>(m_Header.iTotalFiles) * sizeof(FileData);
        m_Stream.seekg(fileTableSize, std::ios_base::cur);
    }

    // Now read only the desired block
    m_Block = BlockData{};
    m_Stream.read(m_Block.padding, sizeof(m_Block.padding));

    m_Block.Files.reserve(m_Header.iTotalFiles);
    m_Block.HashMap.reserve(m_Header.iTotalFiles);

    for (uint j = 0; j < m_Header.iTotalFiles; ++j) {
        FileData fd;
        m_Stream.read(reinterpret_cast<char*>(&fd), sizeof(fd));
        if (!m_Stream) {
            return false;
        }
        m_Block.Files.push_back(fd);
        m_Block.HashMap[fd.iNameHash] = fd;   // build fast lookup
    }

    m_LoadedBlock = desiredBlock;
    return true;
}

bool Rez::Close()
{
    if (m_Stream.is_open()) {
        m_Stream.close();
    }
    m_Block.Files.clear();
    m_Block.HashMap.clear();
    m_LoadedBlock = -1;
    return true;
}

size_t Rez::GetFileCount() const
{
    return m_Block.Files.size();
}

// ------------------------------------------------------------------
// Extract a single file by hash (uses the hash map → O(1))
// ------------------------------------------------------------------
bool Rez::Extract(const std::string& filename, uint hash, int /*blockNumber*/)
{
    // We only ever have one block loaded, so blockNumber is ignored
    auto it = m_Block.HashMap.find(hash);
    if (it == m_Block.HashMap.end()) {
        return false;   // not found
    }

    const FileData& file = it->second;

    // ---------- Create output directories ----------
    // Very simple path handling (works with both / and \)
    std::string outputDir = "out/";
    size_t lastSlash = filename.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        outputDir += filename.substr(0, lastSlash);
    }

    // Create directories recursively (Windows)
    {
        std::string path = outputDir;
        // Replace all \ with / for easier tokenization
        std::replace(path.begin(), path.end(), '\\', '/');

        std::string current;
        size_t start = 0;
        while (start < path.size()) {
            size_t pos = path.find('/', start);
            if (pos == std::string::npos) pos = path.size();

            current += path.substr(start, pos - start);
            if (!current.empty() && current.back() != ':') {
                CreateDirectoryA(current.c_str(), NULL); // ignore errors (already exists is fine)
            }
            current += '/';
            start = pos + 1;
        }
    }

    // ---------- Read & write the file ----------
    const uint size = file.iSize;
    const std::streamoff seekPos = static_cast<std::streamoff>(file.iOffset) * 2048;

    // Grow buffer if necessary
    if (m_ExtractBuffer.size() < size) {
        m_ExtractBuffer.resize(size);
    }

    m_Stream.clear(); // clear any previous eof/fail flags
    m_Stream.seekg(seekPos);
    if (!m_Stream) {
        return false;
    }

    m_Stream.read(m_ExtractBuffer.data(), size);
    if (m_Stream.gcount() != static_cast<std::streamsize>(size)) {
        return false;
    }

    std::string outputFilename = "out/" + filename;
    // Normalize separators
    std::replace(outputFilename.begin(), outputFilename.end(), '\\', '/');

    std::ofstream fileOut(outputFilename, std::ios_base::binary);
    if (!fileOut.is_open()) {
        return false;
    }

    fileOut.write(m_ExtractBuffer.data(), size);
    fileOut.close();

    return true;
}
