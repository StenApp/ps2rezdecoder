// ps2rezdecoder.cpp
// Improved version – C++14 / Visual Studio 2019 compatible
//
// Changes vs original:
//  - Only the requested language block is loaded (saves a lot of RAM)
//  - Hash → FileData map for O(1) lookup
//  - Single large buffer for extraction (no per-chunk new/delete)
//  - Progress counter
//  - Cleaner path handling
//

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cctype>

#include "Rez.h"
#include "cxxopts.hpp"

// ------------------------------------------------------------------
// Hash function (unchanged – extracted from LTIOREZ.IRX)
// ------------------------------------------------------------------
unsigned int rezCalcHash(const char* szFileName, int magic)
{
    unsigned int result = 0;
    int i = 0;

    while (szFileName[i] != '\0') {
        unsigned int c = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(szFileName[i])));
        if (c == '/') c = '\\';          // normalize separators
        result = result + c + result * static_cast<unsigned int>(magic);
        ++i;
    }
    return result;
}

// ------------------------------------------------------------------
// Extract everything from a file list
// ------------------------------------------------------------------
void extractRez(Rez& rez, const std::string& fileListPath, int block)
{
    std::ifstream infile(fileListPath);
    if (!infile.is_open()) {
        std::cerr << "ERROR: Could not open file list \"" << fileListPath << "\"\n";
        return;
    }

    // First pass: count lines for progress
    size_t totalLines = 0;
    {
        std::string dummy;
        while (std::getline(infile, dummy)) {
            if (!dummy.empty()) ++totalLines;
        }
        infile.clear();
        infile.seekg(0);
    }

    std::cout << "Extracting from block " << block
              << "  (" << rez.GetFileCount() << " files in REZ, "
              << totalLines << " entries in list)\n\n";

    size_t processed = 0;
    size_t extracted = 0;
    size_t missing   = 0;

    std::string line;
    while (std::getline(infile, line)) {
        // Trim leading/trailing whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);
        if (line.empty()) continue;

        ++processed;

        unsigned int hash = rezCalcHash(line.c_str(), static_cast<int>(rez.GetMagicNumber()));
        bool ok = rez.Extract(line, hash, block);

        if (ok) {
            ++extracted;
        } else {
            ++missing;
        }

        // Progress every 50 files or at the end
        if (processed % 50 == 0 || processed == totalLines) {
            double pct = totalLines ? (100.0 * processed / totalLines) : 100.0;
            std::cout << "\r[" << std::setw(3) << static_cast<int>(pct) << "%] "
                      << processed << " / " << totalLines
                      << "   extracted: " << extracted
                      << "   missing: " << missing
                      << "          " << std::flush;
        }
    }

    std::cout << "\n\nDone.\n"
              << "  Processed : " << processed << "\n"
              << "  Extracted : " << extracted << "\n"
              << "  Missing   : " << missing << "\n";
}

// ------------------------------------------------------------------
// Just print a hash
// ------------------------------------------------------------------
void checkHash(Rez& rez, const std::string& fileName)
{
    unsigned int hash = rezCalcHash(fileName.c_str(), static_cast<int>(rez.GetMagicNumber()));
    std::cout << fileName << "  →  hash = " << hash << "\n";
}

// ------------------------------------------------------------------
// main
// ------------------------------------------------------------------
int main(int argc, char* argv[])
{
    std::string fileList = "file list.txt";
    std::string rezFile  = "LITH_PS2.REZ";
    std::string mode     = "extract";
    bool showHelp        = false;
    int block            = 0;

    cxxopts::Options options("PS2 Rez Decoder and Extractor",
                             "Decodes and extracts PS2 Lithtech REZ files (C++14 / VS2019)");

    options.add_options()
        ("h,help",  "Show help",                         cxxopts::value(showHelp))
        ("f,file",  "File list",                         cxxopts::value(fileList))
        ("r,rez",   "REZ file",                          cxxopts::value(rezFile))
        ("b,block", "Language block (0=EN 1=FR 2=DE 3=ES 4=IT)", cxxopts::value(block))
        ("m,mode",  "Mode: extract | hash",              cxxopts::value(mode));

    auto result = options.parse(argc, argv);

    if (showHelp) {
        std::cout << options.help() << "\n";
        std::cout << "Default settings:\n"
                  << "  File List     : " << fileList << "\n"
                  << "  REZ File      : " << rezFile  << "\n"
                  << "  Block Number  : " << block    << "\n"
                  << "  Mode          : " << mode     << "\n\n";

        std::cout << "Language blocks:\n"
                  << "  0 = English\n"
                  << "  1 = French\n"
                  << "  2 = German\n"
                  << "  3 = Spanish\n"
                  << "  4 = Italian\n";
        return 0;
    }

    if (mode != "extract" && mode != "hash") {
        std::cerr << "Error: Mode must be 'extract' or 'hash'\n";
        return 1;
    }

    std::cout << "Settings:\n"
              << "  File List     : " << fileList << "\n"
              << "  REZ File      : " << rezFile  << "\n"
              << "  Block Number  : " << block    << "\n"
              << "  Mode          : " << mode     << "\n\n";

    Rez rez;

    // Open only the requested block → big RAM saving
    if (!rez.Open(rezFile, block)) {
        std::cerr << "ERROR: Failed to open \"" << rezFile << "\" (block " << block << ")\n";
        return 1;
    }

    if (mode == "extract") {
        extractRez(rez, fileList, block);
    }
    else {
        checkHash(rez, fileList);   // in hash mode the "file list" parameter is a single filename
    }

    rez.Close();
    return 0;
}
