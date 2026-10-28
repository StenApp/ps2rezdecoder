#include "Rez.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <limits>

namespace
{
    bool DirExists(const std::string& path)
    {
        const DWORD attributes = GetFileAttributesA(path.c_str());

        return attributes != INVALID_FILE_ATTRIBUTES &&
               (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    bool CreateDir(const std::string& path)
    {
        if (path.empty() || DirExists(path))
            return true;

        // Create parent directories first.
        const size_t separator = path.find_last_of("\\/");

        if (separator != std::string::npos) {
            const std::string parent = path.substr(0, separator);

            if (!parent.empty() && !DirExists(parent)) {
                if (!CreateDir(parent))
                    return false;
            }
        }

        if (CreateDirectoryA(path.c_str(), nullptr))
            return true;

        // Another operation may have created it in the meantime.
        return DirExists(path);
    }

    std::string NormalizePath(std::string path)
    {
        std::replace(path.begin(), path.end(), '/', '\\');
        return path;
    }

    const FileData* FindLanguageEntry(
        const std::vector<RezEntry>& entries,
        int language)
    {
        const uint32_t firstTable =
            static_cast<uint32_t>(language + 1);

        for (const RezEntry& entry : entries) {
            if (entry.tableIndex >= firstTable &&
                (entry.tableIndex - firstTable) %
                    Rez::LANGUAGE_COUNT == 0) {
                return &entry.data;
            }
        }

        return nullptr;
    }
}

Rez::Rez()
{
}

Rez::~Rez()
{
    Close();
}

bool Rez::Open(const std::string& file, int language)
{
    if (language < 0 || language >= LANGUAGE_COUNT) {
        std::cerr << "ERROR: language must be in the range 0..4\n";
        return false;
    }

    Close();

    m_Stream.open(file, std::ios::binary);

    if (!m_Stream) {
        std::cerr << "ERROR: cannot open REZ file: "
                  << file << '\n';
        return false;
    }

    m_Stream.read(
        reinterpret_cast<char*>(&m_Header),
        sizeof(m_Header));

    if (!m_Stream) {
        std::cerr << "ERROR: failed to read REZ header\n";
        Close();
        return false;
    }

    std::cout
        << "REZ Header:\n"
        << "  Version      : " << m_Header.iVersion << '\n'
        << "  Total Files  : " << m_Header.iTotalFiles << '\n'
        << "  Magic Number : 0x" << std::hex
        << std::uppercase << m_Header.iMagicNumber
        << std::dec << '\n'
        << "  Data Offset  : " << m_Header.iDataOffset << '\n'
        << "  Total Blocks : " << m_Header.iTotalBlocks << '\n';

    m_HashMap.clear();
    m_Requested.clear();
    m_Extracted.clear();

    m_HashMap.reserve(m_Header.iTotalFiles);

    size_t entriesRead = 0;

    for (uint32_t table = 0;
         table < m_Header.iTotalBlocks;
         ++table) {

        const uint64_t tablePos =
            static_cast<uint64_t>(TABLE_START) +
            static_cast<uint64_t>(table) * TABLE_STRIDE;

        for (uint32_t i = 0;
             i < m_Header.iTotalFiles;
             ++i) {

            const uint64_t entryPos =
                tablePos +
                static_cast<uint64_t>(i) * ENTRY_SIZE;

            if (entryPos >
                static_cast<uint64_t>(
                    (std::numeric_limits<std::streamoff>::max)())) {
                std::cerr << "ERROR: table offset is out of range\n";
                Close();
                return false;
            }

            m_Stream.clear();
            m_Stream.seekg(
                static_cast<std::streamoff>(entryPos),
                std::ios::beg);

            if (!m_Stream) {
                std::cerr << "ERROR: failed to seek to table "
                          << table << ", entry " << i << '\n';
                Close();
                return false;
            }

            FileData fd{};

            m_Stream.read(
                reinterpret_cast<char*>(&fd),
                sizeof(fd));

            if (!m_Stream) {
                std::cerr << "ERROR: failed to read table "
                          << table << ", entry " << i << '\n';
                Close();
                return false;
            }

            m_HashMap[fd.iNameHash].push_back({fd, table});
            ++entriesRead;
        }
    }

    m_LoadedLanguage = language;

    std::cout
        << "Loaded all tables: "
        << m_HashMap.size() << " unique hashes, "
        << entriesRead << " entries; language group "
        << m_LoadedLanguage << '\n';

    return true;
}

bool Rez::Close()
{
    if (m_Stream.is_open())
        m_Stream.close();

    m_HashMap.clear();
    m_Requested.clear();
    m_Extracted.clear();
    m_ExtractBuffer.clear();

    m_LoadedLanguage = -1;
    m_Header = {};

    return true;
}

bool Rez::WriteFileData(
    const FileData& fd,
    const std::string& outputPath)
{
    if (!m_Stream.is_open())
        return false;

    const size_t lastSeparator =
        outputPath.find_last_of("\\/");

    if (lastSeparator != std::string::npos) {
        const std::string parent =
            outputPath.substr(0, lastSeparator);

        if (!CreateDir(parent)) {
            std::cerr << "ERROR: cannot create directory: "
                      << parent << '\n';
            return false;
        }
    }

    std::ofstream out(
        outputPath,
        std::ios::binary | std::ios::trunc);

    if (!out) {
        std::cerr << "ERROR: cannot create output file: "
                  << outputPath << '\n';
        return false;
    }

    const uint64_t offset =
        static_cast<uint64_t>(fd.iOffset) * 2048ull;

    if (offset > static_cast<uint64_t>(
            (std::numeric_limits<std::streamoff>::max)())) {
        std::cerr << "ERROR: file offset is out of range: "
                  << outputPath << '\n';
        out.close();
        std::remove(outputPath.c_str());
        return false;
    }

    // Clear previous EOF/failure flags before seeking to another file.
    m_Stream.clear();
    m_Stream.seekg(
        static_cast<std::streamoff>(offset),
        std::ios::beg);

    if (!m_Stream) {
        std::cerr << "ERROR: cannot seek to file data: "
                  << outputPath << '\n';
        out.close();
        std::remove(outputPath.c_str());
        return false;
    }

    const size_t chunkSize = 64 * 1024;

    if (m_ExtractBuffer.size() < chunkSize)
        m_ExtractBuffer.resize(chunkSize);

    uint64_t remaining = fd.iSize;

    while (remaining > 0) {
        const size_t toRead = static_cast<size_t>(
            (std::min)(
                remaining,
                static_cast<uint64_t>(chunkSize)));

        m_Stream.read(
            m_ExtractBuffer.data(),
            static_cast<std::streamsize>(toRead));

        const std::streamsize bytesRead = m_Stream.gcount();

        if (bytesRead > 0) {
            out.write(
                m_ExtractBuffer.data(),
                bytesRead);

            if (!out) {
                std::cerr << "ERROR: output write failed: "
                          << outputPath << '\n';
                out.close();
                std::remove(outputPath.c_str());
                return false;
            }

            remaining -= static_cast<uint64_t>(bytesRead);
        }

        if (bytesRead != static_cast<std::streamsize>(toRead)) {
            std::cerr << "ERROR: incomplete input data for: "
                      << outputPath << '\n';
            out.close();
            std::remove(outputPath.c_str());
            return false;
        }
    }

    out.flush();

    const bool success = static_cast<bool>(out);
    out.close();

    if (!success || out.fail()) {
        std::cerr << "ERROR: failed to finish output file: "
                  << outputPath << '\n';
        std::remove(outputPath.c_str());
        return false;
    }

    return true;
}

ExtractResult Rez::Extract(
    const std::string& filename,
    uint hash)
{
    if (m_LoadedLanguage < 0 ||
        m_LoadedLanguage >= LANGUAGE_COUNT) {
        return ExtractResult::LanguageEntryNotFound;
    }

    const auto it = m_HashMap.find(hash);

    // Der Hash kommt überhaupt nicht in der REZ vor.
    if (it == m_HashMap.end())
        return ExtractResult::HashNotFound;

    const FileData* selected =
        FindLanguageEntry(it->second, m_LoadedLanguage);

    // Der Hash existiert, aber nicht in dieser Sprachgruppe.
    if (selected == nullptr)
        return ExtractResult::LanguageEntryNotFound;

    // Auch bei Größe 0 oder Schreibfehler als bekannt markieren.
    m_Requested.insert(hash);

    if (selected->iSize == 0)
        return ExtractResult::ZeroSize;

    const std::string outputPath =
        NormalizePath(std::string("out\\") + filename);

    if (!WriteFileData(*selected, outputPath))
        return ExtractResult::WriteFailed;

    m_Extracted.insert(hash);

    return ExtractResult::Written;
}

void Rez::ExtractUnknowns(
    const std::string& outDir,
    UnknownStats& stats)
{
    stats = {};

    if (m_LoadedLanguage < 0 ||
        m_LoadedLanguage >= LANGUAGE_COUNT) {
        std::cerr << "ERROR: no language group is loaded\n";
        ++stats.failed;
        return;
    }

    if (!CreateDir(outDir)) {
        std::cerr << "ERROR: cannot create unknown output directory: "
                  << outDir << '\n';
        ++stats.failed;
        return;
    }

    // Write a manifest for successfully extracted unknown files.
    // The list is stored alongside the unknown_XXXXXXXX.bin files.
    const std::string hashListPath =
        NormalizePath(outDir + "\\unknown_hashlist.txt");
    std::ofstream hashList(hashListPath, std::ios::out | std::ios::trunc);

    if (!hashList) {
        std::cerr << "WARNING: cannot create unknown hash list: "
                  << hashListPath << '\n';
    }
    else {
        hashList << "# filename\thash\tsize\toffset\txsize\n";
    }

    for (const auto& pair : m_HashMap) {
        const uint hash = pair.first;

        // Known-list matches must never be dumped as unknowns,
        // including zero-sized files and known write failures.
        if (m_Requested.find(hash) != m_Requested.end())
            continue;

        // Preserve compatibility with callers that manually marked
        // a hash as extracted.
        if (m_Extracted.find(hash) != m_Extracted.end())
            continue;

        const FileData* selected =
            FindLanguageEntry(pair.second, m_LoadedLanguage);

        if (selected == nullptr)
            continue;

        if (selected->iSize == 0) {
            ++stats.zeroSize;
            continue;
        }

        std::ostringstream name;
        name << "unknown_"
             << std::uppercase << std::hex
             << std::setw(8) << std::setfill('0')
             << hash << ".bin";

        const std::string outputPath =
            NormalizePath(outDir + "\\" + name.str());

        if (WriteFileData(*selected, outputPath)) {
            ++stats.written;

            if (hashList) {
                hashList << name.str()
                         << "\t0x"
                         << std::uppercase << std::hex
                         << std::setw(8) << std::setfill('0')
                         << hash
                         << std::dec << std::setfill(' ')
                         << "\t" << selected->iSize
                         << "\t" << selected->iOffset
                         << "\t" << selected->iXSize
                         << '\n';
            }
        }
        else {
            ++stats.failed;

            std::cerr
                << "Failed unknown hash 0x"
                << std::uppercase << std::hex << hash
                << std::dec
                << ", offset=" << selected->iOffset
                << ", size=" << selected->iSize
                << ", xsize=" << selected->iXSize
                << '\n';
        }
    }

    if (hashList) {
        hashList.flush();
        if (!hashList) {
            std::cerr << "WARNING: failed while writing unknown hash list: "
                      << hashListPath << '\n';
        }
    }
}

void Rez::ExtractUnknowns(const std::string& outDir)
{
    UnknownStats stats;
    ExtractUnknowns(outDir, stats);

    std::cout
        << "Unknown files successfully written: "
        << stats.written << '\n'
        << "Unknown files with size 0: "
        << stats.zeroSize << '\n'
        << "Unknown files with write errors: "
        << stats.failed << '\n';
}

void Rez::DebugHash(uint hash) const
{
    const auto it = m_HashMap.find(hash);

    if (it == m_HashMap.end()) {
        std::cout << "Hash 0x"
                  << std::uppercase << std::hex << hash
                  << std::dec << " not found\n";
        return;
    }

    std::cout << "Hash 0x"
              << std::uppercase << std::hex << hash
              << std::dec << " has "
              << it->second.size() << " entries:\n";

    for (const RezEntry& entry : it->second) {
        std::cout
            << "  Table " << entry.tableIndex
            << ", offset=" << entry.data.iOffset
            << ", size=" << entry.data.iSize
            << ", xsize=" << entry.data.iXSize
            << '\n';
    }
}