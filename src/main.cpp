/**
 * @file            main.cpp
 * @author          Aditya Agarwal (aditya.agarwal@dumblebots.com)
 * @brief           Source File for AgFileSystemScanner
 *
 */

#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <cstring>

#include <filesystem>

#define     SHOW_RECURSIVE      (0)
#define     SHOW_PERMISSIONS    (1)
#define     SHOW_FILES          (2)
#define     SHOW_SPECIAL        (3)
#define     SEARCH_EXACT        (4)
#define     SEARCH_NOEXT        (5)
#define     SEARCH_CONTAINS     (6)
#define     HELP                (7)

#define     MAX_PATH_LEN    (256)

namespace                   fs = std::filesystem;

static const char           *usage          = "Usage: %s [PATH] [options]\n"
                                              "Scan through the filesystem starting from PATH.\n"
                                              "\n"
                                              "Example: %s \"..\" --recursive --files\n"
                                              "\n"
                                              "Options:\n"
                                              "-r, --recursive         Recursively go through directories\n"
                                              "-p, --permissions       Show Permissions of each entry\n"
                                              "\n"
                                              "-f, --files             Show Regular Files (normally hidden)\n"
                                              "-s, --special           Show Special Files such as sockets, pipes, etc. (normally hidden)\n"
                                              "\n"
                                              "-S, --search            Only log those entries whose name matches exactly with the given string. (normally hidden)\n"
                                              "    --search-noext      Only log those entries whose name (excluding extension) matches exactly with the given string. (normally hidden)\n"
                                              "    --contains          Only log those entries whose name contains the given string. (normally hidden)\n"
                                              "\n"
                                              "-h, --help              Print Usage Instructions\n"
                                              "\n";

static uint64_t             sOptionMask     = 0;
static std::error_code      sErrorCode;

[[nodiscard]]
bool
get_option (const uint8_t &pBit)
{
    return (sOptionMask & (1ULL << pBit)) != 0;
}

void
set_option (const uint8_t &pBit)
{
    sOptionMask     |= (1ULL << pBit);
}

void
unset_option (const uint8_t &pBit)
{
    sOptionMask     &= ~(1ULL << pBit);
}

[[nodiscard]]
wchar_t
*create_wchar (const char *pInitStr)
{
    uint64_t    len;
    wchar_t     *result {nullptr};

    len     = strnlen (pInitStr, MAX_PATH_LEN);
    result  = (wchar_t *)malloc (sizeof (wchar_t) * len);

    if (result == nullptr) {
        fprintf (stderr, "Could not allocate Wide String when converting path\n");
        std::exit (-1);
    }

    for (uint64_t i = 0; i < len; ++i) {
        ((char *)&result[i])[0]     = pInitStr[i];
        ((char *)&result[i])[1]     = 0;
        ((char *)&result[i])[2]     = 0;
        ((char *)&result[i])[3]     = 0;
    }

    return result;
}

bool
parse_str_to_uint64 (const char *pPtr, uint64_t &pRes)
{
    for (; *pPtr != 0; ++pPtr) {
        if (*pPtr < '0' || *pPtr > '9')
            return false;
        pRes = (pRes * 10) + (uint64_t)(*pPtr - '0');
    }

    return true;
}

void
scan_path (const wchar_t *pPath, const uint64_t &pLevel)
{
    if (pPath == nullptr) {
        fwprintf (stderr, L"Path can not point to NULL\n");
        std::exit (-1);
    }

    if (fs::exists (pPath) == false) {
        fwprintf (stderr, L"The Given Path \"%ls\" does not exist\n", pPath);
        std::exit (-1);
    }

    fs::directory_iterator      iter (pPath, sErrorCode);
    fs::directory_iterator      fin;
    fs::directory_entry         entry;

    bool                        isDir;
    bool                        isFile;
    bool                        isOther;

    if (sErrorCode.value () != 0) {
        fwprintf (stderr, L"Error Code: %4d", sErrorCode.value ());
        fwprintf (stderr, L"%s\n", sErrorCode.message ());
    }

    // iterate through all the files in the current path
    for (fin = end (iter); iter != fin; ++iter) {

        entry       = *iter;

        isDir       = entry.is_directory ();
        isFile      = entry.is_regular_file ();
        isOther     = entry.is_other ();

        // folder case
        if (isDir) {
            wprintf (L"%-16s%ls\n", "DIRECTORY", entry.path().wstring().c_str());

            // if the recursive option is set, then go inside
            if (get_option (SHOW_RECURSIVE)) {
                if (pLevel != 1) {
                    wprintf (L"\n");
                    scan_path (entry.path().wstring().c_str(), (pLevel != 0) ? (pLevel - 1) : (pLevel));
                    wprintf (L"\n");
                }
            }
        }

        // regular file case
        else if (isFile) {

            // only log the entry the show files option is set
            if (get_option (SHOW_FILES)) {
                wprintf (L"%-16s%ls\n", "FILE", entry.path().wstring().c_str());
            }
        }

        // special file case
        else if (isOther) {

            // only log the entry if the show special files option is set
            if (get_option (SHOW_SPECIAL)) {
                wprintf (L"%-16s%ls\n", "OTHER", entry.path().wstring().c_str());
            }
        }

        // unrecognized file type case
        else {
            printf ("File type can not be determined\n");
            printf ("Dying...\n");
            exit(-1);
        }
    }
}

int
main (int argc, char *argv[])
{
    const char  *initPathStr {nullptr};
    wchar_t     *initPathWstr {nullptr};

    uint64_t    recursionLevel {0};

    for (uint64_t i = 1, len; i < (uint64_t)argc; ++i) {

        len = strnlen (argv[i], 32);

        if (len <= 0) {
            wprintf (L"Ignoring Unknown Option of length 0\n");
            continue;
        }

        if (i == 1 && argv[i][0] != '-') {
            initPathStr = argv[i];
            continue;
        }

        switch (len) {

            case 2:
            if (strncmp (argv[i], "-h", 2) == 0) {
                set_option (HELP);
            }
            else if (strncmp (argv[i], "-r", 2) == 0) {
                set_option (SHOW_RECURSIVE);
            }
            else if (strncmp (argv[i], "-f", 2) == 0) {
                set_option (SHOW_FILES);
            }
            else if (strncmp (argv[i], "-s", 2) == 0) {
                set_option (SHOW_SPECIAL);
            }
            else {
                printf ("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

            case 6:
            if (strncmp (argv[i], "--help", 6) == 0) {
                set_option (HELP);
            }
            else {
                printf ("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

            case 7:
            if (strncmp (argv[i], "--files", 7) == 0) {
                set_option (SHOW_FILES);
            }
            else {
                printf ("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

            case 9:
            if (strncmp (argv[i], "--special", 9) == 0) {
                set_option (SHOW_SPECIAL);
            }
            break;

            case 11:
            if (strncmp (argv[i], "--recursive", 11) == 0) {
                set_option (SHOW_RECURSIVE);

                // if the user has provided the number of levels, then parse it into a string
                if ((i + 1) < (uint64_t)argc) {
                    if (!parse_str_to_uint64 (argv[i + 1], recursionLevel)) {
                        printf ("Invalid value for recursion depth \"%s\"\nPlease provide a positive whole number\n", argv[i + 1]);
                        return -1;
                    }
                    ++i;
                }
            }
            else {
                printf ("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

            default:
            printf ("Ignoring Unknown Option \"%s\"\n", argv[i]);
        }
    }

    // print the usage instructions and terminate
    if (get_option (HELP)) {
        printf (usage, argv[0]);
        return 0;
    }

    if (initPathStr == nullptr) {
        initPathStr = ".";
    }

    initPathWstr        = create_wchar (initPathStr);

    if (fs::exists (initPathWstr)) {
        if (fs::is_directory (initPathWstr)) {
            scan_path(initPathWstr, recursionLevel);
        }
        else {
            wprintf (L"The given path must be to a directory\n");
        }
    }
    else {
        wprintf (L"The given Path \"%ls\" does not exist\n", initPathWstr);
    }
    free (initPathWstr);

    return 0;
}
