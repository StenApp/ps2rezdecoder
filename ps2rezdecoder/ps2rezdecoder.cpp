// ps2rezdecoder.cpp
// PS2 LithTech REZ decoder with language groups and extraction summary.

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <cctype>
#include <iomanip>
#include <exception>

#include "Rez.h"
#include "cxxopts.hpp"

unsigned int rezCalcHash(const char* szFileName, int magic);

void extractRez(
    Rez* aRez,
    const std::string& fileList,
    bool dumpUnknowns);

void checkHash(
    Rez* aRez,
    const std::string& fileName);

int main(int argc, char* argv[])
{
    std::string fileList = "file list.txt";
    std::string rezFile = "LITH_PS2.REZ";
    std::string mode = "extract";

    int language = 0;
    bool dumpUnknowns = false;

    cxxopts::Options options(
        "PS2 Rez Decoder and Extractor V1.1",
        "Decodes and extracts a PS2 LithTech REZ file.");

    options.add_options()
        ("h,help", "Show help")
        ("f,file",
            "File list (or filename for hash mode)",
            cxxopts::value<std::string>(fileList))
        ("r,rez",
            "Path to LITH_PS2.REZ",
            cxxopts::value<std::string>(rezFile))
        ("l,language",
            "Language group 0-4",
            cxxopts::value<int>(language))
        ("m,mode",
            "Mode: extract | hash",
            cxxopts::value<std::string>(mode))
        ("u,unknowns",
            "Also extract unknown hashes as .bin files",
            cxxopts::value<bool>(dumpUnknowns)
                ->default_value("false")
                ->implicit_value("true"));

    try {
        const auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help() << "\n\n"
                      << "Language groups (0-4):\n"
                      << "  Group 0: tables 1, 6, 11, ...\n"
                      << "  Group 1: tables 2, 7, 12, ...\n"
                      << "  Group 2: tables 3, 8, 13, ...\n"
                      << "  Group 3: tables 4, 9, 14, ...\n"
                      << "  Group 4: tables 5, 10, 15, ...\n";
            return 0;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Argument error: " << e.what() << '\n';
        return 1;
    }

    if (mode != "extract" && mode != "hash") {
        std::cerr
            << "ERROR: mode must be 'extract' or 'hash'\n";
        return 1;
    }

    if (language < 0 || language > 4) {
        std::cerr
            << "ERROR: language must be in the range 0..4\n";
        return 1;
    }

    std::cout
        << "Settings:\n"
        << "  File List    : " << fileList << '\n'
        << "  REZ File     : " << rezFile << '\n'
        << "  Language     : " << language << '\n'
        << "  Mode         : " << mode << '\n'
        << "  Dump unknowns: "
        << (dumpUnknowns ? "yes" : "no") << "\n\n";

    Rez aRez;

    if (!aRez.Open(rezFile, language)) {
        std::cerr << "ERROR: failed to open " << rezFile << '\n';
        return 1;
    }

    if (mode == "extract") {
        extractRez(&aRez, fileList, dumpUnknowns);
    }
    else {
        checkHash(&aRez, fileList);
    }

    aRez.Close();
    return 0;
}

void extractRez(
    Rez* aRez,
    const std::string& fileList,
    bool dumpUnknowns)
{
    const uint magic = aRez->GetMagicNumber();

    size_t matches = 0;
    size_t hashNotFound = 0;
    size_t languageEntryNotFound = 0;
    size_t knownWritten = 0;
    size_t knownZeroSize = 0;
    size_t knownFailed = 0;

    std::ifstream infile(fileList);

    if (!infile) {
        std::cerr
            << "ERROR: cannot open file list: "
            << fileList << '\n';
        return;
    }

    std::ofstream diagnostic(
        "not_found.txt",
        std::ios::out | std::ios::trunc);

    if (!diagnostic) {
        std::cerr
            << "ERROR: cannot create not_found.txt\n";
        return;
    }

    diagnostic
        << "Not-found diagnostics for language group "
        << aRez->GetLoadedLanguage() << "\n\n";

    std::string line;

    while (std::getline(infile, line)) {
        // Trailing CR, spaces and tabs entfernen.
        while (!line.empty() &&
               (line.back() == '\r' ||
                line.back() == ' ' ||
                line.back() == '\t')) {
            line.pop_back();
        }

        if (line.empty())
            continue;

        const size_t first =
            line.find_first_not_of(" \t");

        if (first == std::string::npos)
            continue;

        if (line[first] == '#')
            continue;

        std::istringstream iss(line);
        std::string filename;

        if (!(iss >> filename))
            continue;

        const uint hash = rezCalcHash(
            filename.c_str(),
            static_cast<int>(magic));

        const ExtractResult result =
            aRez->Extract(filename, hash);

        switch (result) {
        case ExtractResult::Written:
            ++matches;
            ++knownWritten;
            break;

        case ExtractResult::ZeroSize:
            ++matches;
            ++knownZeroSize;
            break;

        case ExtractResult::WriteFailed:
            ++matches;
            ++knownFailed;
            break;

        case ExtractResult::HashNotFound:
            ++hashNotFound;
            diagnostic
                << "HASH_NOT_PRESENT"
                << "\t0x"
                << std::uppercase << std::hex
                << std::setw(8) << std::setfill('0')
                << hash
                << std::dec << std::setfill(' ')
                << "\t" << filename << '\n';
            break;

        case ExtractResult::LanguageEntryNotFound:
            ++languageEntryNotFound;
            diagnostic
                << "NO_LANGUAGE_ENTRY"
                << "\t0x"
                << std::uppercase << std::hex
                << std::setw(8) << std::setfill('0')
                << hash
                << std::dec << std::setfill(' ')
                << "\t" << filename << '\n';
            break;
        }
    }

    infile.close();
    diagnostic.close();

    const size_t missing =
        hashNotFound + languageEntryNotFound;

    UnknownStats unknown{};

    if (dumpUnknowns)
        aRez->ExtractUnknowns("out/unknown", unknown);

    std::cout
        << "\nExtraction summary (language group "
        << aRez->GetLoadedLanguage() << "):\n"
        << "  File list matches:                  "
        << matches << '\n'
        << "  File list entries not found:        "
        << missing << '\n'
        << "    Hash not present in REZ:          "
        << hashNotFound << '\n'
        << "    No entry for language group:      "
        << languageEntryNotFound << '\n'
        << "  Known files successfully written:   "
        << knownWritten << '\n'
        << "  Known files with size 0:             "
        << knownZeroSize << '\n'
        << "  Known files with write errors:       "
        << knownFailed << '\n';

    size_t totalWritten = knownWritten;

    if (dumpUnknowns) {
        totalWritten += unknown.written;

        std::cout
            << "  Unknown files successfully written: "
            << unknown.written << '\n'
            << "  Unknown files with size 0:          "
            << unknown.zeroSize << '\n'
            << "  Unknown files with write errors:    "
            << unknown.failed << '\n';
    }

    std::cout
        << "  Total files successfully written:   "
        << totalWritten << '\n'
        << "  Diagnostic file:                    not_found.txt\n";
}

void checkHash(
    Rez* aRez,
    const std::string& fileName)
{
    const uint hash = rezCalcHash(
        fileName.c_str(),
        static_cast<int>(aRez->GetMagicNumber()));

    std::cout
        << fileName
        << " -> 0x"
        << std::hex << std::uppercase << hash
        << " (" << std::dec << hash << ")\n";
}

// Extracted from LTIOREZ.IRX.
unsigned int rezCalcHash(
    const char* szFileName,
    int magic)
{
    unsigned int result = 0;

    for (int i = 0; szFileName[i] != '\0'; ++i) {
        unsigned int c = static_cast<unsigned char>(
            std::toupper(
                static_cast<unsigned char>(szFileName[i])));

        if (c == '/')
            c = '\\';

        result = result + c +
            result * static_cast<unsigned int>(magic);
    }

    return result;
}