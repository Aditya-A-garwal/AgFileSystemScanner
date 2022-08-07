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
#include <ctime>

#include <filesystem>
#include <chrono>

/** Option that specifies if directories should be recursively scanned and displayed */
#define SHOW_RECURSIVE (0)

/** Option that specified if the permissions of a filesystem entry should be printed */
#define SHOW_PERMISSIONS (1)
/** Option that specified if the last modification time of a file or directory should be printed */
#define SHOW_LASTTIME (2)
/** Option that specified if hidden filesystem entries should be shown and scanned */
#define SHOW_HIDDEN (3)

//! UNUSED
/** Option that specifies if all entries need to be printed linearly (without indentation) with their relative paths */
#define SHOW_LINEAR_REL (4)
//! UNUSED
/** Option that specifies if all entries need to be printed linearly (without indentation) with their absoulute paths */
#define SHOW_LINEAR_ABS (5)

/** Option that specifies if all files within a directory need to be individually displayed */
#define SHOW_FILES (6)
/** Option that specifies if all symlinks within a directory need to be individually displayed */
#define SHOW_SYMLINKS (7)
/** Option that specifies if all special files (such as sockets, block devices etc.) within a directory need to be individually displayed */
#define SHOW_SPECIAL (8)

//! UNUSED
/** Option that specifies if only those entries whose name matches a given pattern should be shown */
#define SEARCH_EXACT (9)
//! UNUSED
/** Option that specifies if only those entries whose name (without the extension) matches a given pattern should be shown */
#define SEARCH_NOEXT (10)
//! UNUSED
/** Option that specifies if only those entries whose name contains a given pattern should be shown */
#define SEARCH_CONTAINS (11)

/** Option that specified if usage instructions need to be printed */
#define HELP (12)

/** Maximum allowed length of an argument (other than the path) after which it is not checked further */
#define MAX_ARG_LEN (32)
/** Maximum allowed length of the provided path after which any further characters are ignored */
#define MAX_PATH_LEN (256)
/** Maximum allowed length of the string that stores the last modified time (formatted) of a file */
#define MAX_FMT_TIME_LEN (64)

/** Number of spaces by which to further indent each subsequent nested directory's entries */
#define INDENT_COL_WIDTH (4)

namespace fs = std::filesystem;
namespace chrono = std::chrono;

/** Usage instructions of the program */
static const char *usage = "Usage: %s [PATH] [options]\n"
                                              "Scan through the filesystem starting from PATH.\n"
                                              "\n"
                                              "Example: %s \"..\" --recursive --files\n"
                                              "\n"
                                              "Options:\n"
                           "-r, --recursive             Recursively go through directories\n"
                           "-p, --permissions           Show Permissions of each entry\n"
                           "-t, --modification-time     Show Time of Last Modification\n"
                           "-h, --hidden                Show hidden entries\n"
                                              "\n"
                           "-f, --files                 Show Regular Files (normally hidden)\n"
                           "-l, --symlinks              Show Symlinks\n"
                           "-s, --special               Show Special Files such as sockets, pipes, etc. (normally hidden)\n"
                                              "\n"
                           "-S, --search                Only log those entries whose name matches exactly with the given string. (normally hidden)\n"
                           "    --search-noext          Only log those entries whose name (excluding extension) matches exactly with the given string. (normally hidden)\n"
                           "    --contains              Only log those entries whose name contains the given string. (normally hidden)\n"
                                              "\n"
                           "-h, --help                  Print Usage Instructions\n"
                                              "\n";

static uint64_t sOptionMask{};       /** Bitmask to represent command line options provided by the user */
static std::error_code sErrorCode{}; /** Container for error code to be passed around when dealing with std::filesystem artifacts */

static uint64_t sRecursionLevel{}; /** Number of levels of directories to go within if the recursive option is set */

static wchar_t *sSearchPattern{nullptr}; /** Pattern to search for if any of the search options are set */

/**
 * @brief                   Returns whether a given command line option is set or not
 *
 * @param pBit              Command Line option to check
 *
 * @return true             If the given option is set
 * @return false            If the given option is cleared
 */
[[nodiscard]] inline bool
get_option(const uint8_t &pBit)
{
    return (sOptionMask & (1ULL << pBit)) != 0;
}

/**
 * @brief                   Sets a command line option
 *
 * @param pBit              Option to clear
 */
inline void
set_option(const uint8_t &pBit)
{
    sOptionMask |= (1ULL << pBit);
}

/**
 * @brief                   Clears a command line option
 *
 * @param pBit              Option to clear
 */
inline void
clear_option(const uint8_t &pBit)
{
    sOptionMask &= ~(1ULL << pBit);
}

/**
 * @brief                   Create a null-terminated wide string from a regular null-terminated string
 *
 * @param pPtr              Null-terminated String to generate a wide string from
 *
 * @return wchar_t*         Given String as a wide string
 */
[[nodiscard]] inline wchar_t
    *
    create_wchar(const char *pPtr)
{
    uint64_t len;
    wchar_t *result{nullptr};

    len = strnlen(pPtr, MAX_PATH_LEN);
    result = (wchar_t *)malloc(sizeof(wchar_t) * len);

    if (result == nullptr)
    {
        fprintf(stderr, "Could not allocate Wide String when converting path\n");
        std::exit(-1);
    }

    for (uint64_t i = 0; i < len; ++i)
    {
        ((char *)&result[i])[0] = pPtr[i];
        ((char *)&result[i])[1] = 0;
        ((char *)&result[i])[2] = 0;
        ((char *)&result[i])[3] = 0;
    }

    return result;
}

/**
 * @brief                   Parses a null-terminated string into an unsigned 64 bit integer
 *
 * @param pPtr              Pointer to string
 * @param pRes              Reference to resultant variable
 *
 * @return true             If the conversion was successful
 * @return false            If the conversion failed
 */
bool parse_str_to_uint64(const char *pPtr, uint64_t &pRes)
{
    for (; *pPtr != 0; ++pPtr)
    {
        if (*pPtr < '0' || *pPtr > '9')
            return false;
        pRes = (pRes * 10) + (uint64_t)(*pPtr - '0');
    }

    return true;
}

/**
 * @brief                   Prints the last modified time of a filesystem entry in a well formatted manner
 *
 * @param pFsEntry          Reference to entry whose last modification time is to be printed
 */
void print_last_modif_time(const fs::directory_entry &pFsEntry)
{
    static fs::file_time_type lastModifTpFs;     /** Time point when the given entry was last modified */
    static time_t lastModifTime;                 /** Time point when the given entry was last modified as a time_t instance */
    static char formattedTime[MAX_FMT_TIME_LEN]; /** Time point when the given entry was last modified formatted as a string */

    // get the timepoint on which it was last modified (the timepoint is on chrono::file_clock)
    lastModifTpFs = pFsEntry.last_write_time(sErrorCode);

    // if an error was generated, then report it
    if (sErrorCode.value() != 0)
    {
        fwprintf(stderr, L"Error while reading last modified time for \"%ls\"\n", pFsEntry.path().wstring().c_str());
        fwprintf(stderr, L"Error Code: %4d\n", sErrorCode.value());
        fwprintf(stderr, L"%s\n", sErrorCode.message());
        wprintf(L"%20c", ' ');
    }
    // format the time and print it
    else
    {

        // convert the time point on chrono::file_clock to a time_t object (time passed since epoch) and format it
        lastModifTime = chrono::system_clock::to_time_t(chrono::file_clock::to_sys(lastModifTpFs));
        strftime(formattedTime, MAX_FMT_TIME_LEN, "%b %d %Y  %H:%M", std::localtime(&lastModifTime));

        wprintf(L"%20s", formattedTime);
    }
}

/**
 * @brief                   Scans through and prints the contents of a directory
 *
 * @param pPath             Path to the directory to scan
 * @param pLevel            The number of recursive calls of this function before the current one
 */
void scan_path(const wchar_t *pPath, const uint64_t &pLevel)
{
    const uint64_t indentWidth = INDENT_COL_WIDTH * pLevel; /** Number of spaces to enter before printing the entry for the current function call */

    // the path can not be a nullptr, it must be a valid string (FIND A WAY TO FREE THE STRINGS ALLOCATED IN MAIN)
    if (pPath == nullptr)
    {
        fwprintf(stderr, L"Path can not point to NULL\n");
        std::exit(-1);
    }

    // the path must exist (FIND A WAY TO FREE THE STINGS ALLOCATED IN MAIN)
    if (fs::exists(pPath) == false)
    {
        wprintf(L"The Given Path \"%ls\" does not exist\n", pPath);
        wprintf(L"Terminating...\n");
        std::exit(-1);
    }

    fs::directory_iterator iter(pPath, sErrorCode); /** Iterator to the elements within the current directory */
    fs::directory_iterator fin;                     /** Iterator to the element after the last element in the current directory */
    /** Reference to current entry (used while dereferencing iter) */
    fs::directory_entry entry;

    bool isDir;     /** Stores whether the current entry is a directory */
    bool isFile;    /** Stores whether the current entry is a regular file */
    bool isSymlink; /** Stores whether the current entry is a symlink */
    bool isSpecial; /** Stores whether the current entry is a special file */

    uint64_t regularFileCnt; /** Number of regular files within this directory */
    uint64_t totalFileSize;  /** Combines sizes of all files within this directory */
    uint64_t curFileSize;    /** Size of file that is being currently processed */

    uint64_t symlinkCnt; /** Number of symlinks in the current directory */

    uint64_t specialCnt; /** Number of special files in the current directory */

    std::wstring filename;
    const char *entryType;

    // if an error occoured while trying to get the directory iterator, then report it here
    if (sErrorCode.value() != 0)
    {
        fwprintf(stderr, L"Error while creating directory iterator for path \"%ls\"\n", pPath);
        fwprintf(stderr, L"Error Code: %4d\n", sErrorCode.value());
        fwprintf(stderr, L"%s\n", sErrorCode.message());
    }

    // iterate through all the files in the current path
    for (fin = end(iter), regularFileCnt = 0, symlinkCnt = 0, specialCnt = 0, totalFileSize = 0; iter != fin; ++iter)
    {

        // get the current entry and its name
        entry = *iter;
        filename = entry.path().filename().wstring();

        isDir = entry.is_directory();
        isFile = entry.is_regular_file();
        isSymlink = entry.is_symlink();
        isSpecial = entry.is_other();

        if (isSymlink)
        {

            ++symlinkCnt;

            if (get_option(SHOW_SYMLINKS))
            {
                wprintf(L"%16s    %-*c%ls -> %ls\n", (isDir) ? ("SYMLINK DIR") : ("SYMLINK"), indentWidth, ' ', filename.c_str(), fs::read_symlink(entry).wstring().c_str());
            }
        }
        else if (isFile)
        {

            ++regularFileCnt;

            curFileSize = fs::file_size(entry);
            totalFileSize += curFileSize;

            if (get_option(SHOW_FILES))
            {

                if (get_option(SHOW_LASTTIME))
                {
                    print_last_modif_time(entry);
                }

                wprintf(L"%16llu    %-*c%ls\n", curFileSize, indentWidth, ' ', filename.c_str());
            }
        }
        else if (isSpecial)
        {

            ++specialCnt;

            if (get_option(SHOW_SPECIAL))
            {

                entryType = "SPECIAL";

                if (entry.is_socket())
                {
                    entryType = "SOCKET";
                }
                else if (entry.is_block_file())
                {
                    entryType = "BLOCK DEVICE";
                }
                else if (entry.is_fifo())
                {
                    entryType = "FIFO PIPE";
                }

                if (get_option(SHOW_LASTTIME))
                {
                    wprintf(L"%24c", ' ');
            }

                wprintf(L"%16s    %-*c%ls\n", entryType, indentWidth, ' ', filename.c_str());
        }
        }
        else if (isDir)
        {

            if (get_option(SHOW_LASTTIME))
            {
                print_last_modif_time(entry);
            }

            wprintf(L"%16llu    %-*c<%ls>\n", 0, indentWidth, ' ', filename.c_str());

            if (get_option(SHOW_RECURSIVE))
            {
                if ((sRecursionLevel == 0) || (pLevel < sRecursionLevel))
                {
                    scan_path(entry.path().wstring().c_str(), 1 + pLevel);
                }
            }
        }
        else
        {
            printf("File type of \"%ls\" can not be determined\n", entry.path().wstring().c_str());
            printf("Terminating...\n");
            exit(-1);
        }
    }

    if (regularFileCnt != 0 && !get_option(SHOW_FILES))
    {

        if (get_option(SHOW_LASTTIME))
        {
            wprintf(L"%20c", ' ');
    }

        wprintf(L"%16llu    %-*c<%llu files>\n", totalFileSize, indentWidth, ' ', regularFileCnt);
    }
    if (symlinkCnt != 0 && !get_option(SHOW_SYMLINKS))
    {

        if (get_option(SHOW_LASTTIME))
        {
            wprintf(L"%20c", ' ');
    }

        wprintf(L"%16c    %-*c<%llu symlinks>\n", '-', indentWidth, ' ', symlinkCnt);
}
    if (specialCnt != 0 && !get_option(SHOW_SPECIAL))
    {

        if (get_option(SHOW_LASTTIME))
{
            wprintf(L"%20c", ' ');
        }

        wprintf(L"%16c    %-*c<%llu special entries>\n", '-', indentWidth, ' ', specialCnt);
    }
}

int main(int argc, char *argv[])
{
    const char *initPathStr;   /** Path to start the scan process from, represneted as a string of chars */
    wchar_t *initPathWstr;     /** Path to start the scan process from, represented as a string of wide characters */
    const char *searchPattern; /** Pattern to search for if any of the search option are set (equals nullptr when no search option is set) */

    initPathStr = nullptr;
    initPathWstr = nullptr;
    searchPattern = nullptr;

    // iterate through all the command line arguments
    for (uint64_t i = 1, argLen; i < (uint64_t)argc; ++i)
    {

        // get the length of the current argument
        argLen = strnlen(argv[i], MAX_ARG_LEN);

        // if the length is not greater than 0, ignore it
        if (argLen <= 0)
        {
            wprintf(L"Ignoring Unknown Option of length 0\n");
            continue;
        }

        // if this is the first argument (apart from the executable name), check if it is a path
        if (i == 1 && argv[i][0] != '-')
        {
            initPathStr = argv[i];
            continue;
        }

        // depending on the length of the argument, try to match it with any of the options
        switch (argLen)
        {

            case 2:
            if (strncmp(argv[i], "-h", 2) == 0)
            {
                set_option(HELP);
            }
            else if (strncmp(argv[i], "-r", 2) == 0)
            {
                set_option(SHOW_RECURSIVE);

                // if the user has provided the number of levels, then parse it into a string
                if ((i + 1) < (uint64_t)argc && strnlen(argv[i + 1], MAX_ARG_LEN) != 0 && argv[i + 1][0] != '-')
                {
                    if (!parse_str_to_uint64(argv[i + 1], sRecursionLevel))
                    {
                        printf("Invalid value for recursion depth \"%s\"\nPlease provide a positive whole number\n", argv[i + 1]);
                        return -1;
                    }
                    ++i;
                }
            }
            else if (strncmp(argv[i], "-t", 2) == 0)
            {
                set_option(SHOW_LASTTIME);
            }
            else if (strncmp(argv[i], "-f", 2) == 0)
            {
                set_option(SHOW_FILES);
            }
            else if (strncmp(argv[i], "-l", 2) == 0)
            {
                set_option(SHOW_SYMLINKS);
            }
            else if (strncmp(argv[i], "-s", 2) == 0)
            {
                set_option(SHOW_SPECIAL);
            }
            else if (strncmp(argv[i], "-p", 2) == 0)
            {
                set_option(SHOW_PERMISSIONS);
            }
            else if (strncmp(argv[i], "-h", 2) == 0)
            {
                set_option(SHOW_HIDDEN);
            }
            else if (strncmp(argv[i], "-S", 2) == 0)
            {

                // make sure that multiple search modes are not being used simultaneously
                if (get_option(SEARCH_NOEXT) || get_option(SEARCH_CONTAINS))
                {
                    wprintf(L"Can only set one search mode at a time\n");
                    wprintf(L"Terminating...\n");
                    std::exit(-1);
                }

                // make sure that a search pattern was provided
                // if (i == (argc - 1) || strnlen (argv[i + 1], MAX_ARG_LEN) == 0 || argv[i + 1] == '-') {
                if (i == (uint64_t)(argc - 1) || strnlen(argv[i + 1], MAX_ARG_LEN) == 0)
                {
                    wprintf(L"No Search pattern provided after \"%ls\" flag\n", argv[i]);
                    wprintf(L"Terminating...\n");
                    std::exit(-1);
                }

                // set the option and skip the next argument (that is the search pattern)
                set_option(SEARCH_EXACT);
                searchPattern = argv[++i];
            }
            else
            {
                printf("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

            case 6:
            if (strncmp(argv[i], "--help", 6) == 0)
            {
                set_option(HELP);
            }
            else
            {
                printf("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

            case 7:
            if (strncmp(argv[i], "--files", 7) == 0)
            {
                set_option(SHOW_FILES);
            }
            else
            {
                printf("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

            case 8:
            if (strncmp(argv[i], "--hidden", 8) == 0)
            {
                set_option(SHOW_HIDDEN);
            }
            else if (strncmp(argv[i], "--search", 8) == 0)
            {

                // make sure that multiple search modes are not being used simultaneously
                if (get_option(SEARCH_NOEXT) || get_option(SEARCH_CONTAINS))
                {
                    wprintf(L"Can only set one search mode at a time\n");
                    wprintf(L"Terminating...\n");
                    std::exit(-1);
                }

                // make sure that a search pattern was provided
                // if (i == (uint64_t)(argc - 1) || strnlen (argv[i + 1], MAX_ARG_LEN) == 0 || argv[i + 1] == '-') {
                if (i == (uint64_t)(argc - 1) || strnlen(argv[i + 1], MAX_ARG_LEN) == 0)
                {
                    wprintf(L"No Search pattern provided after \"%ls\" flag\n", argv[i]);
                    wprintf(L"Terminating...\n");
                    std::exit(-1);
                }

                // set the option and skip the next argument (that is the search pattern)
                set_option(SEARCH_EXACT);
                searchPattern = argv[++i];
            }
            else
            {
                printf("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

            case 9:
            if (strncmp(argv[i], "--special", 9) == 0)
            {
                set_option(SHOW_SPECIAL);
            }
            else
            {
                printf("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

            case 10:
            if (strncmp(argv[i], "--symlinks", 10) == 0)
            {
                set_option(SHOW_SYMLINKS);
            }
            else if (strncmp(argv[i], "--contains", 10) == 0)
            {

                // make sure that multiple search modes are not being used simultaneously
                if (get_option(SEARCH_NOEXT) || get_option(SEARCH_EXACT))
                {
                    wprintf(L"Can only set one search mode at a time\n");
                    wprintf(L"Terminating...\n");
                    std::exit(-1);
                }
                // make sure that a search pattern was provided
                // if (i == (uint64_t)(argc - 1) || strnlen (argv[i + 1], MAX_ARG_LEN) == 0 || argv[i + 1] == '-') {
                if (i == (uint64_t)(argc - 1) || strnlen(argv[i + 1], MAX_ARG_LEN) == 0)
                {
                    wprintf(L"No Search pattern provided after \"%ls\" flag\n", argv[i]);
                    wprintf(L"Terminating...\n");
                    std::exit(-1);
                }

                // set the option and skip the next argument (that is the search pattern)
                set_option(SEARCH_CONTAINS);
                searchPattern = argv[++i];
            }
            else
            {
                printf("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

            case 11:
            if (strncmp(argv[i], "--recursive", 11) == 0)
            {
                set_option(SHOW_RECURSIVE);

                // if the user has provided the number of levels, then parse it into a string
                if ((i + 1) < (uint64_t)argc && strnlen(argv[i + 1], MAX_ARG_LEN) != 0 && argv[i + 1][0] != '-')
                {
                    if (!parse_str_to_uint64(argv[i + 1], sRecursionLevel))
                    {
                        printf("Invalid value for recursion depth \"%s\"\nPlease provide a positive whole number\n", argv[i + 1]);
                        return -1;
                    }
                    ++i;
                }
            }
            else
            {
                printf("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

            case 13:
            if (strncmp(argv[i], "--permissions", 13) == 0)
            {
                set_option(SHOW_PERMISSIONS);
            }
            else
            {
                printf("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

            case 14:
            if (strncmp(argv[i], "--search-noext", 14) == 0)
            {

                // make sure that multiple search modes are not being used simultaneously
                if (get_option(SEARCH_CONTAINS) || get_option(SEARCH_EXACT))
                {
                    wprintf(L"Can only set one search mode at a time\n");
                    wprintf(L"Terminating...\n");
                    std::exit(-1);
                }
                // make sure that a search pattern was provided
                // if (i == (uint64_t)(argc - 1) || strnlen (argv[i + 1], MAX_ARG_LEN) == 0 || argv[i + 1] == '-') {
                if (i == (uint64_t)(argc - 1) || strnlen(argv[i + 1], MAX_ARG_LEN) == 0)
                {
                    wprintf(L"No Search pattern provided after \"%ls\" flag\n", argv[i]);
                    wprintf(L"Terminating...\n");
                    std::exit(-1);
                }

                // set the option and skip the next argument (that is the search pattern)
                set_option(SEARCH_NOEXT);
                searchPattern = argv[++i];
            }
            else
            {
                printf("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

        case 19:
            if (strncmp(argv[i], "--modification-time", 19) == 0)
            {
                set_option(SHOW_LASTTIME);
            }
            else
            {
                printf("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

            default:
            printf("Ignoring Unknown Option \"%s\"\n", argv[i]);
        }
    }

    // print the usage instructions and terminate
    if (get_option(HELP))
    {
        printf(usage, argv[0]);
        return 0;
    }

    initPathWstr = create_wchar((initPathStr == nullptr) ? (".") : (initPathStr));

    sSearchPattern = (searchPattern == nullptr) ? (nullptr) : (create_wchar(searchPattern));

    if (fs::exists(initPathWstr))
    {
        if (fs::is_directory(initPathWstr))
        {
            scan_path(initPathWstr, 0);
        }
        else
        {
            wprintf(L"The given path is not to a directory\n");
            wprintf(L"Terminating...\n");
        }
    }
    else
    {
        wprintf(L"The given Path \"%ls\" does not exist\n", initPathWstr);
    }

    free(initPathWstr);
    free(sSearchPattern);

    return 0;
}
