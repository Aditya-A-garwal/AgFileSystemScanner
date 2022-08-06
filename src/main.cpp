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
#define     SHOW_HIDDEN         (2)

#define     SHOW_FILES          (3)
#define     SHOW_SYMLINKS       (4)
#define     SHOW_SPECIAL        (5)

#define     SEARCH_EXACT        (6)
#define     SEARCH_NOEXT        (7)
#define     SEARCH_CONTAINS     (8)

#define     HELP                (9)

#define     MAX_ARG_LEN         (32)
#define     MAX_PATH_LEN        (256)

#define     INDENT_COL_WIDTH    (4)

namespace                   fs = std::filesystem;

static const char           *usage          = "Usage: %s [PATH] [options]\n"
                                              "Scan through the filesystem starting from PATH.\n"
                                              "\n"
                                              "Example: %s \"..\" --recursive --files\n"
                                              "\n"
                                              "Options:\n"
                                              "-r, --recursive         Recursively go through directories\n"
                                              "-p, --permissions       Show Permissions of each entry\n"
                                              "-h, --hidden            Show hidden entries\n"
                                              "\n"
                                              "-f, --files             Show Regular Files (normally hidden)\n"
                                              "-l, --symlinks          Show Symlinks\n"
                                              "-s, --special           Show Special Files such as sockets, pipes, etc. (normally hidden)\n"
                                              "\n"
                                              "-S, --search            Only log those entries whose name matches exactly with the given string. (normally hidden)\n"
                                              "    --search-noext      Only log those entries whose name (excluding extension) matches exactly with the given string. (normally hidden)\n"
                                              "    --contains          Only log those entries whose name contains the given string. (normally hidden)\n"
                                              "\n"
                                              "-h, --help              Print Usage Instructions\n"
                                              "\n";

/** Bitmask to represent command line options provided by the user */
static uint64_t             sOptionMask {};
/** Container for error code to be passed around when dealing with std::filesystem artifacts */
static std::error_code      sErrorCode {};

/** Number of levels of directories to go within if the recursive option is set */
static uint64_t             sRecursionLevel {};

/** Pattern to search for if any of the search options are set */
static wchar_t              *sSearchPattern {nullptr};

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
    // the path can not be a nullptr, it must be a valid string (FIND A WAY TO FREE THE STRINGS ALLOCATED IN MAIN)
    if (pPath == nullptr) {
        fwprintf (stderr, L"Path can not point to NULL\n");
        std::exit (-1);
    }

    // the path must exist (FIND A WAY TO FREE THE STINGS ALLOCATED IN MAIN)
    if (fs::exists (pPath) == false) {
        wprintf (L"The Given Path \"%ls\" does not exist\n", pPath);
        wprintf (L"Terminating...\n");
        std::exit (-1);
    }

    /** Iterator to the elements within the current directory */
    fs::directory_iterator      iter (pPath, sErrorCode);
    /** Iterator to the element after the last element in the current directory */
    fs::directory_iterator      fin;
    /** Reference to current entry (used while dereferencing iter) */
    fs::directory_entry         entry;

    /** Stores whether the current entry is a directory */
    bool                        isDir;
    /** Stores whether the current entry is a regular file */
    bool                        isFile;
    /** Stores whether the current entry is a symlink */
    bool                        isSymlink;
    /** Stores whether the current entry is a special file */
    bool                        isSpecial;

    /** Number of regular files within this directory */
    uint64_t                    regularFileCnt;
    /** Combines sizes of all files within this directory */
    uint64_t                    totalFileSize;
    /** Size of file that is being currently processed */
    uint64_t                    curFileSize;

    /** Number of symlinks in the current directory */
    uint64_t                    symlinkCnt;

    /** Number of special files in the current directory */
    uint64_t                    specialCnt;

    /** Name of the current entry */
    std::wstring                filename;
    const char                  *entryType;

    /** Number of spaces to enter before printing the entry for the current function call */
    const uint64_t              indentWidth         = INDENT_COL_WIDTH * pLevel;

    // if an error occoured while trying to get the directory iterator, then report it here
    if (sErrorCode.value () != 0) {
        fwprintf (stderr, L"Error Code: %4d\n", sErrorCode.value ());
        fwprintf (stderr, L"%s\n", sErrorCode.message ());
    }

    // iterate through all the files in the current path
    for (fin = end (iter), regularFileCnt = 0, symlinkCnt = 0, specialCnt = 0, totalFileSize = 0; iter != fin; ++iter) {

        // get the current entry and its name
        entry       = *iter;
        filename    = entry.path ().filename ().wstring ();

        isDir       = entry.is_directory ();
        isFile      = entry.is_regular_file ();
        isSymlink   = entry.is_symlink ();
        isSpecial   = entry.is_other ();

        if (isSymlink) {

            ++symlinkCnt;

            if (get_option (SHOW_SYMLINKS)) {
                wprintf (L"%20s    %-*c%ls -> %ls\n", "SYMLINK", indentWidth, ' ', filename.c_str (), fs::read_symlink (entry).wstring ().c_str ());
            }
        }
        else if (isFile) {

            ++regularFileCnt;

            curFileSize         = fs::file_size (entry);
            totalFileSize       += curFileSize;

            if (get_option (SHOW_FILES)) {
                wprintf (L"%20llu    %-*c%ls\n", curFileSize, indentWidth, ' ', filename.c_str ());
            }
        }
        else if (isSpecial) {

            ++specialCnt;

            if (get_option (SHOW_SPECIAL)) {

                entryType           = "SPECIAL";

                if (entry.is_socket ()) {
                    entryType       = "SOCKET";
                }
                else if (entry.is_block_file ()) {
                    entryType       = "BLOCK DEVICE";
                }
                else if (entry.is_fifo ()) {
                    entryType       = "FIFO PIPE";
                }

                wprintf (L"%20s    %-*c%ls\n", entryType, indentWidth, ' ', filename.c_str ());
            }
        }
        else if (isDir) {
            wprintf (L"%20llu    %-*c<%ls>\n", 0, indentWidth, ' ', filename.c_str ());

            if (get_option (SHOW_RECURSIVE)) {
                if ((sRecursionLevel == 0) || (pLevel < sRecursionLevel)) {
                    scan_path (entry.path ().wstring ().c_str (), 1 + pLevel);
                }
            }
        }
        else {
            printf ("File type of \"%ls\" can not be determined\n", entry.path ().wstring ().c_str ());
            printf ("Terminating...\n");
            exit(-1);
        }
    }

    if (regularFileCnt != 0 && !get_option (SHOW_FILES)) {
        wprintf (L"%20llu    %-*c<%llu files>\n", totalFileSize, indentWidth, ' ', regularFileCnt);
    }
    if (symlinkCnt != 0 && !get_option (SHOW_SYMLINKS)) {
        wprintf (L"%20c    %-*c<%llu symlinks>\n", '-', indentWidth, ' ', symlinkCnt);
    }
    if (specialCnt != 0 && !get_option (SHOW_SPECIAL)) {
        wprintf (L"%20c    %-*c<%llu special entries>\n", '-', indentWidth, ' ', specialCnt);
    }
}

int
main (int argc, char *argv[])
{
    /** Path to start the scan process from, represneted as a string of chars */
    const char  *initPathStr {nullptr};
    /** Path to start the scan process from, represented as a string of wide characters */
    wchar_t     *initPathWstr {nullptr};
    /** Pattern to search for if any of the search option are set (equals nullptr when no search option is set) */
    const char  *searchPattern {nullptr};

    // iterate through all the command line arguments
    for (uint64_t i = 1, len; i < (uint64_t)argc; ++i) {

        // get the length of the current argument
        len = strnlen (argv[i], MAX_ARG_LEN);

        // if hte length is not greater than 0, ignore it
        if (len <= 0) {
            wprintf (L"Ignoring Unknown Option of length 0\n");
            continue;
        }

        // if this is the first argument (apart from the executable name), check if it is a path
        if (i == 1 && argv[i][0] != '-') {
            initPathStr = argv[i];
            continue;
        }

        // depending on the length of the argument, try to match it with any of the options
        switch (len) {

            case 2:
            if (strncmp (argv[i], "-h", 2) == 0) {
                set_option (HELP);
            }
            else if (strncmp (argv[i], "-r", 2) == 0) {
                set_option (SHOW_RECURSIVE);

                // if the user has provided the number of levels, then parse it into a string
                if ((i + 1) < (uint64_t)argc && strnlen (argv[i + 1], MAX_ARG_LEN) != 0 && argv[i + 1][0] != '-') {
                    if (!parse_str_to_uint64 (argv[i + 1], sRecursionLevel)) {
                        printf ("Invalid value for recursion depth \"%s\"\nPlease provide a positive whole number\n", argv[i + 1]);
                        return -1;
                    }
                    ++i;
                }
            }
            else if (strncmp (argv[i], "-f", 2) == 0) {
                set_option (SHOW_FILES);
            }
            else if (strncmp (argv[i], "-l", 2) == 0) {
                set_option (SHOW_SYMLINKS);
            }
            else if (strncmp (argv[i], "-s", 2) == 0) {
                set_option (SHOW_SPECIAL);
            }
            else if (strncmp (argv[i], "-p", 2) == 0) {
                set_option (SHOW_PERMISSIONS);
            }
            else if (strncmp (argv[i], "-h", 2) == 0) {
                set_option (SHOW_HIDDEN);
            }
            else if (strncmp (argv[i], "-S", 2) == 0) {

                // make sure that multiple search modes are not being used simultaneously
                if (get_option (SEARCH_NOEXT) || get_option (SEARCH_CONTAINS)) {
                    wprintf (L"Can only set one search mode at a time\n");
                    wprintf (L"Terminating...\n");
                    std::exit (-1);
                }

                // make sure that a search pattern was provided
                // if (i == (argc - 1) || strnlen (argv[i + 1], MAX_ARG_LEN) == 0 || argv[i + 1] == '-') {
                if (i == (uint64_t)(argc - 1) || strnlen (argv[i + 1], MAX_ARG_LEN) == 0) {
                    wprintf (L"No Search pattern provided after \"%ls\" flag\n", argv[i]);
                    wprintf (L"Terminating...\n");
                    std::exit (-1);
                }

                // set the option and skip the next argument (that is the search pattern)
                set_option (SEARCH_EXACT);
                searchPattern = argv[++i];
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

            case 8:
            if (strncmp (argv[i], "--hidden", 8) == 0) {
                set_option (SHOW_HIDDEN);
            }
            else if (strncmp (argv[i], "--search", 8) == 0) {

                // make sure that multiple search modes are not being used simultaneously
                if (get_option (SEARCH_NOEXT) || get_option (SEARCH_CONTAINS)) {
                    wprintf (L"Can only set one search mode at a time\n");
                    wprintf (L"Terminating...\n");
                    std::exit (-1);
                }

                // make sure that a search pattern was provided
                // if (i == (uint64_t)(argc - 1) || strnlen (argv[i + 1], MAX_ARG_LEN) == 0 || argv[i + 1] == '-') {
                if (i == (uint64_t)(argc - 1) || strnlen (argv[i + 1], MAX_ARG_LEN) == 0) {
                    wprintf (L"No Search pattern provided after \"%ls\" flag\n", argv[i]);
                    wprintf (L"Terminating...\n");
                    std::exit (-1);
                }

                // set the option and skip the next argument (that is the search pattern)
                set_option (SEARCH_EXACT);
                searchPattern = argv[++i];
            }
            else {
                printf ("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

            case 9:
            if (strncmp (argv[i], "--special", 9) == 0) {
                set_option (SHOW_SPECIAL);
            }
            else {
                printf ("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

            case 10:
            if (strncmp (argv[i], "--symlinks", 10) == 0) {
                set_option (SHOW_SYMLINKS);
            }
            else if (strncmp (argv[i], "--contains", 10) == 0) {

                // make sure that multiple search modes are not being used simultaneously
                if (get_option (SEARCH_NOEXT) || get_option (SEARCH_EXACT)) {
                    wprintf (L"Can only set one search mode at a time\n");
                    wprintf (L"Terminating...\n");
                    std::exit (-1);
                }
                // make sure that a search pattern was provided
                // if (i == (uint64_t)(argc - 1) || strnlen (argv[i + 1], MAX_ARG_LEN) == 0 || argv[i + 1] == '-') {
                if (i == (uint64_t)(argc - 1) || strnlen (argv[i + 1], MAX_ARG_LEN) == 0) {
                    wprintf (L"No Search pattern provided after \"%ls\" flag\n", argv[i]);
                    wprintf (L"Terminating...\n");
                    std::exit (-1);
                }

                // set the option and skip the next argument (that is the search pattern)
                set_option (SEARCH_CONTAINS);
                searchPattern = argv[++i];
            }
            else {
                printf ("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

            case 11:
            if (strncmp (argv[i], "--recursive", 11) == 0) {
                set_option (SHOW_RECURSIVE);

                // if the user has provided the number of levels, then parse it into a string
                if ((i + 1) < (uint64_t)argc && strnlen (argv[i + 1], MAX_ARG_LEN) != 0 && argv[i + 1][0] != '-') {
                    if (!parse_str_to_uint64 (argv[i + 1], sRecursionLevel)) {
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

            case 13:
            if (strncmp (argv[i], "--permissions", 13) == 0) {
                set_option (SHOW_PERMISSIONS);
            }
            else {
                printf ("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

            case 14:
            if (strncmp (argv[i], "--search-noext", 14) == 0) {

                // make sure that multiple search modes are not being used simultaneously
                if (get_option (SEARCH_CONTAINS) || get_option (SEARCH_EXACT)) {
                    wprintf (L"Can only set one search mode at a time\n");
                    wprintf (L"Terminating...\n");
                    std::exit (-1);
                }
                // make sure that a search pattern was provided
                // if (i == (uint64_t)(argc - 1) || strnlen (argv[i + 1], MAX_ARG_LEN) == 0 || argv[i + 1] == '-') {
                if (i == (uint64_t)(argc - 1) || strnlen (argv[i + 1], MAX_ARG_LEN) == 0) {
                    wprintf (L"No Search pattern provided after \"%ls\" flag\n", argv[i]);
                    wprintf (L"Terminating...\n");
                    std::exit (-1);
                }

                // set the option and skip the next argument (that is the search pattern)
                set_option (SEARCH_NOEXT);
                searchPattern = argv[++i];
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

    if (searchPattern != nullptr) {
        sSearchPattern  = create_wchar (searchPattern);
    }

    if (fs::exists (initPathWstr)) {
        if (fs::is_directory (initPathWstr)) {
            scan_path(initPathWstr, 0);
        }
        else {
            wprintf (L"The given path is not to a directory\n");
            wprintf (L"Terminating...\n");
        }
    }
    else {
        wprintf (L"The given Path \"%ls\" does not exist\n", initPathWstr);
    }
    free (initPathWstr);

    return 0;
}
