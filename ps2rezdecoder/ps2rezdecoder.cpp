// ps2rezdecoder.cpp
// Updated for correct table layout + language groups + unknown dump

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <cctype>
#include "Rez.h"
#include "cxxopts.hpp"

unsigned int rezCalcHash(const char* szFileName, int magic);
void extractRez(Rez* aRez, const std::string& fileList, bool dumpUnknowns);
void checkHash(Rez* aRez, const std::string& fileName);

int main(int argc, char* argv[])
{
    std::string fileList = "file list.txt";
    std::string rezFile  = "LITH_PS2.REZ";
    std::string mode     = "extract";
    int  language        = 0;
    bool showHelp        = false;
    bool dumpUnknowns    = false;

    cxxopts::Options options("PS2 Rez Decoder and Extractor",
        "Decodes and extracts a PS2 Lithtech REZ file (language-aware).");

    options.add_options()
        ("h,help",     "Show help")
        ("f,file",     "File list (or single filename for hash mode)", cxxopts::value(fileList))
        ("r,rez",      "Path to LITH_PS2.REZ",                        cxxopts::value(rezFile))
        ("l,language", "Language group 0-4",                           cxxopts::value(language))
        ("m,mode",     "Mode: extract | hash",                         cxxopts::value(mode))
        ("u,unknowns", "Also extract unknown hashes as .bin blobs",    cxxopts::value(dumpUnknowns))
        ;

    auto result = options.parse(argc, argv);

    if (result.count("help") || showHelp) {
        std::cout << options.help() << "\n"
                  << "Language groups (0-4):\n"
                  << "  Each group consists of every 5th table\n"
                  << "  (0,5,10,... / 1,6,11,... etc.)\n"
                  << "  Exact language mapping still being verified via credits/voices.\n\n";
        return 0;
    }

    if (mode != "extract" && mode != "hash") {
        std::cerr << "Error: mode must be 'extract' or 'hash'\n";
        return 1;
    }

    std::cout << "Settings:\n"
              << "  File List   : " << fileList << "\n"
              << "  Rez File    : " << rezFile  << "\n"
              << "  Language    : " << language << "\n"
              << "  Mode        : " << mode     << "\n"
              << "  Dump unknowns: " << (dumpUnknowns ? "yes" : "no") << "\n\n";

    Rez* aRez = new Rez();

    if (!aRez->Open(rezFile, language)) {
        std::cerr << "ERROR: Failed to open " << rezFile << "\n";
        delete aRez;
        return 1;
    }

    if (mode == "extract") {
        extractRez(aRez, fileList, dumpUnknowns);
    }
    else if (mode == "hash") {
        checkHash(aRez, fileList);
    }

    aRez->Close();
    delete aRez;
    return 0;
}

void extractRez(Rez* aRez, const std::string& fileList, bool dumpUnknowns)
{
    const uint magic = aRez->GetMagicNumber();
    size_t ok = 0, missing = 0;

    std::ifstream infile(fileList);
    if (!infile) {
        std::cerr << "ERROR: cannot open file list: " << fileList << "\n";
        return;
    }

    std::string line;
    while (std::getline(infile, line)) {
        // trim
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;

        // take first token only
        std::istringstream iss(line);
        std::string file;
        if (!(iss >> file))
            continue;

        uint hash = rezCalcHash(file.c_str(), magic);
        if (aRez->Extract(file, hash)) {
            ++ok;
        } else {
            ++missing;
            // optional: std::cout << "missing: " << file << "\n";
        }
    }
    infile.close();

    std::cout << "Extracted " << ok << " known files, "
              << missing << " from list not found in this language group.\n";

    if (dumpUnknowns) {
        aRez->ExtractUnknowns("out/unknown");
    }
}

void checkHash(Rez* aRez, const std::string& fileName)
{
    uint hash = rezCalcHash(fileName.c_str(), aRez->GetMagicNumber());
    std::cout << fileName << " -> 0x" << std::hex << std::uppercase << hash
              << " (" << std::dec << hash << ")\n";
}

// Extracted from LTIOREZ.IRX
unsigned int rezCalcHash(const char* szFileName, int magic)
{
    unsigned int result = 0;
    for (int i = 0; szFileName[i] != '\0'; ++i) {
        unsigned int c = static_cast<unsigned char>(toupper(static_cast<unsigned char>(szFileName[i])));
        if (c == '/') c = '\\';
        result = result + c + result * static_cast<unsigned int>(magic);
    }
    return result;
}
