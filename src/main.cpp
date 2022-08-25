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
#include <chrono>

#define SHOW_RECURSIVE          (0)                                                         /** Option that specifies if directories should be recursively scanned and displayed */

#define SHOW_PERMISSIONS        (1)                                                         /** Option that specified if the permissions of a filesystem entry should be printed */
#define SHOW_LASTTIME           (2)                                                         /** Option that specified if the last modification time of a file or directory should be printed */

#define SHOW_ABSNOINDENT        (3)                                                         /** Option that specifies if the absoulute paths of all entries should be printed without indentation */

#define SHOW_FILES              (5)                                                         /** Option that specifies if all files within a directory need to be individually displayed */
#define SHOW_SYMLINKS           (6)                                                         /** Option that specifies if all symlinks within a directory need to be individually displayed */
#define SHOW_SPECIAL            (7)                                                         /** Option that specifies if all special files (such as sockets, block devices etc.) within a directory need to be individually displayed */

#define SEARCH_EXACT            (8)                                                         /** Option that specifies if only those entries whose name matches a given pattern should be shown */
#define SEARCH_NOEXT            (9)                                                         /** Option that specifies if only those entries whose name (without the extension) matches a given pattern should be shown */
#define SEARCH_CONTAINS         (10)                                                        /** Option that specifies if only those entries whose name contains a given pattern should be shown */ //! UNUSED

#define SHOW_DIR_SIZE           (11)                                                        /** Option that specifies if directory sizes should be recursively calculated and shown */
#define SHOW_ERRORS             (12)

#define HELP                    (13)                                                        /** Option that specifies if usage instructions need to be printed */

#define MAX_ARG_LEN             (32)                                                        /** Maximum allowed length of an argument (other than the path) after which it is not checked further */
#define MAX_PATH_LEN            (256)                                                       /** Maximum allowed length of the provided path after which any further characters are ignored */
#define MAX_FMT_TIME_LEN        (64)                                                        /** Maximum allowed length of the string that stores the last modified time (formatted) of a file */

#define INDENT_COL_WIDTH        (4)                                                         /** Number of spaces by which to further indent each subsequent nested directory's entries */

// #define USE_KMP_SEARCH          true                                                        /** Defined if Knuth Morris Prat search needs to be used */

#define red                     "\033[0;31m"
#define Lred                    L"\033[0;31m"

#define RED                     "\033[1;31m"
#define LRED                    L"\033[1;31m"

#define blue                    "\033[0;34m"
#define Lblue                   L"\033[0;34m"

#define BLUE                    "\033[1;34m"
#define LBLUE                   L"\033[1;34m"

#define cyan                    "\033[0;36m"
#define Lcyan                   L"\033[0;36m"

#define CYAN                    "\033[1;36m"
#define LCYAN                   L"\033[1;36m"

#define green                   "\033[0;32m"
#define Lgreen                  L"\033[0;32m"

#define GREEN                   "\033[1;32m"
#define LGREEN                  L"\033[1;32m"

#define yellow                  "\033[0;33m"
#define Lyellow                 L"\033[0;33m"

#define YELLOW                  "\033[1;33m"
#define LYELLOW                 L"\033[1;33m"

#define NO_CLR                  "\033[0m"
#define LNO_CLR                 L"\033[0m"

// #define ERR_NEWL                true

#ifdef ERR_NEWL                                                                             /** Determines if each error entry should have empty lines before and after */
#define SHOW_ERR(pMsg, ...)     fwprintf (stderr,                               \
                                            L"\n"                               \
                                            pMsg L" (Code %d, %s)\n",           \
                                            L"\n",                              \
                                            __VA_ARGS__,                        \
                                            sErrorCode.value (),                \
                                            sErrorCode.message ().c_str ());                /** Macro to print errors in a well formatted manner */
#else
#define SHOW_ERR(pMsg, ...)     fwprintf (stderr,                               \
                                            pMsg L" (Code %d, %s)\n",           \
                                            __VA_ARGS__,                        \
                                            sErrorCode.value (),                \
                                            sErrorCode.message ().c_str ());                /** Macro to print errors in a well formatted manner */
#endif

namespace fs                    = std::filesystem;
namespace chrono                = std::chrono;

/** Usage instructions of the program */
static const wchar_t    *usage      = L"Usage: %s [PATH] [options]\n"
                                    L"Scan through the filesystem starting from PATH.\n"
                                    L"\n"
                                    L"Example: %s \"..\" --recursive --files\n"
                                    L"\n"
                                    L"Options:\n"
                                    L"-r, --recursive             Recursively go through directories\n"
                                    L"-p, --permissions           Show Permissions of each entry\n"
                                    L"-t, --modification-time     Show Time of Last Modification\n"
                                    L"\n"
                                    L"    --abs                   Show the complete absoulute path without indentation\n"
                                    L"\n"
                                    L"-f, --files                 Show Regular Files (normally hidden)\n"
                                    L"-l, --symlinks              Show Symlinks\n"
                                    L"-s, --special               Show Special Files such as sockets, pipes, etc. (normally hidden)\n"
                                    L"\n"
                                    L"-S, --search                Only log those entries whose name matches exactly with the given string. (normally hidden)\n"
                                    L"    --search-noext          Only log those entries whose name (excluding extension) matches exactly with the given string. (normally hidden)\n"
                                    L"    --contains              Only log those entries whose name contains the given string. (normally hidden)\n"
                                    L"\n"
                                    L"-h, --help                  Print Usage Instructions\n"
                                    L"    --recursive-dir-size    Use the size of the inode structure of a directory rather than recursively going inside to check it\n"
                                    L"-e, --show-err              Show errors\n"
                                    L"\n";

/** Unformatted summary string for directory to Totalerse (not including subdirectories) */
static const wchar_t    *rootSum    = L"\n"
                                    L"Summary of \"%ls\"\n"
                                    L"<%lu files>\n"
                                    L"<%lu symlinks>\n"
                                    L"<%lu special files>\n"
                                    L"<%lu subdirectories>\n"
                                    L"<%lu total entries>\n"
                                    L"\n";

/** Unformatted summary string for the directory to Totalerse (including subdirectories) */
static const wchar_t    *recSum     = L"Including subdirectories\n"
                                    L"<%lu files>\n"
                                    L"<%lu symlinks>\n"
                                    L"<%lu special files>\n"
                                    L"<%lu subdirectories>\n"
                                    L"<%lu total entries>\n"
                                    L"\n";

/** Unformatted summary string for number of entries found matching search pattern (in search mode) */
static const wchar_t    *foundSum   = L"\n"
                                    L"Summary of matching entries\n"
                                    L"<%lu files>\n"
                                    L"<%lu symlinks>\n"
                                    L"<%lu special files>\n"
                                    L"<%lu subdirectories>\n"
                                    L"<%lu total entries>\n"
                                    L"\n";

/** Unformatted summary string for number of entries traversed while matching search pattern (in search mode) */
static const wchar_t    *TotalSum    = L"Summary of traversal of \"%ls\"\n"
                                    L"<%lu files>\n"
                                    L"<%lu symlinks>\n"
                                    L"<%lu special files>\n"
                                    L"<%lu subdirectories>\n"
                                    L"<%lu total entries>\n"
                                    L"\n";

static const wchar_t    *sInitPath          {nullptr};                                      /** Initial path to start the scan from */
static const wchar_t    *sSearchPattern     {nullptr};                                      /** Pattern to search for if any of the search options are set */

#ifdef USE_KMP_SEARCH
static uint64_t         sSearchPatternLen   {};
#endif

static uint64_t         sOptionMask         {};                                             /** Bitmask to represent command line options provided by the user */
static std::error_code  sErrorCode          {};                                             /** Container for error code to be passed around when dealing with std::filesystem artifacts */

static uint64_t         sErrorEntries       {};                                             /** Number of entries that were encountered but could not be processed/displayed for whatever reason */ //! UNUSED

static uint64_t         sRecursionLevel     {};                                             /** Number of levels of directories to go within if the recursive option is set */

static uint64_t         sNumFilesTotal      {};                                             /** Total number of files traversed */
static uint64_t         sNumSymlinksTotal   {};                                             /** Total number of Symlinks traversed */
static uint64_t         sNumSpecialTotal    {};                                             /** Total number of special files traversed */
static uint64_t         sNumDirsTotal       {};                                             /** Total number of directories traversed */

static uint64_t         sNumFilesRoot       {};                                             /** Number of files traversed in the root directory */
static uint64_t         sNumSymlinksRoot    {};                                             /** Number of symlinks traversed in the root directory */
static uint64_t         sNumSpecialRoot     {};                                             /** Number of special files traversed in the root directory */
static uint64_t         sNumDirsRoot        {};                                             /** Number of subdirectories in the root directory */

static uint64_t         sNumFilesMatched    {};                                             /** Number of files matching the search pattern */
static uint64_t         sNumSymlinksMatched {};                                             /** Number of symlinks matching the search pattern */
static uint64_t         sNumSpecialMatched  {};                                             /** Number of special files matching the search pattern */
static uint64_t         sNumDirsMatched     {};                                             /** Number of subdirectories matching the search pattern */

static bool             sPrintSummary       {};                                             /** Flag to determine whether the summary should be printed or not */

#ifdef USE_KMP_SEARCH
static uint64_t          sLpsArray[MAX_ARG_LEN]        {};                                      /** LPS array to use */
#endif

/**
 * @brief                   Returns whether a given command line option is set or not
 *
 * @param pBit              Command Line option to check
 *
 * @return true             If the given option is set
 * @return false            If the given option is cleared
 */
[[nodiscard]] inline bool
get_option (const uint8_t &pBit) noexcept
{
    return (sOptionMask & (1ULL << pBit)) != 0;
}

/**
 * @brief                   Sets a command line option
 *
 * @param pBit              Option to set
 */
inline void
set_option (const uint8_t &pBit) noexcept
{
    sOptionMask |= (1ULL << pBit);
}

/**
 * @brief                   Clears a command line option
 *
 * @param pBit              Option to clear
 */
inline void
clear_option (const uint8_t &pBit) noexcept
{
    sOptionMask &= ~(1ULL << pBit);
}

/**
 * @brief                   Create a null-terminated wide string (of wchar_t) from a null-terminated string (of char)
 *
 *                          The new string is generated and placed on the heap, and care must be taken to free it
 *
 * @param pPtr              Null-terminated String to generate a wide string from
 *
 * @return wchar_t*         Given String as a wide string
 */
[[nodiscard]] inline wchar_t
*widen_string (const char *pPtr) noexcept
{
    // setlocale (LC_CTYPE, "UTF-32");

    uint64_t    len;                                                                        /** Length of the string to convert to a wide string (including the null termination character) */
    wchar_t     *result {nullptr};                                                          /** Resultant wide string after the conversion */

    // calculate the length of the input string and allocate a wide string of the corresponding size
    len     = strnlen (pPtr, MAX_PATH_LEN);
    result  = (wchar_t *)malloc (sizeof (wchar_t) * (len + 1));
    memset (result, 0, sizeof (wchar_t) * (len + 1));

    if (result == nullptr) {
        fprintf (stderr,
        "Could not allocate Wide String when converting path\n");
        std::exit (-1);
    }

    for (uint64_t i = 0; i <= len; ++i) {
        ((char *)&result[i])[0] = pPtr[i];
        ((char *)&result[i])[1] = 0;
        ((char *)&result[i])[2] = 0;
        ((char *)&result[i])[3] = 0;
    }

    // if (std::mbstowcs (result, pPtr, len) != len) {
    //     fprintf (stderr,
    //     "Could not convert charace String when converting path\n");
    //     std::exit (-1);
    // }

    return result;
}

#ifdef USE_KMP_SEARCH
/**
 * @brief
 *
 */
void
compute_lps ()
{
    // clear the LPS array
    memset (sLpsArray, 0, sizeof (sLpsArray[0]) * MAX_ARG_LEN);

    // sSearchPatternLen   = wstrnlen (sSearchPattern, MAX_ARG_LEN);

    // build the LPS array
    for (uint64_t i = 1, maxPrefLen = 0; i < sSearchPatternLen; ++i) {

        if (sSearchPattern[i] == sSearchPattern[maxPrefLen]) {
            sLpsArray[i]    = ++maxPrefLen;
        }
        else if (maxPrefLen != 0) {
            maxPrefLen      = sLpsArray[maxPrefLen - 1];
            --i;
        }
        // else {
        //     sLpsArray[i]    = 0;
        // }
    }
}
#endif

/**
 * @brief                   Checks if a given string contains the search pattern provided by the user
 *
 * @param pStr              String to check
 *
 * @return true             If the given string contains the search pattern
 * @return false            If the given string does not contains the search pattern
 */
bool
check_contains (const std::wstring &pStr)
{

#ifdef USE_KMP_SEARCH

    for (uint64_t i = 0, j = 0; (pStr.size () + j) >= (sSearchPatternLen + i); ) {
        if (sSearchPattern[j] == pStr[i]) {
            ++i;
            ++j;
        }

        if (j == sSearchPatternLen) {
            return true;
        }

        else if ((i < pStr.size ()) && (sSearchPattern[j] != pStr[i])) {
            if (j != 0) {
                j = sLpsArray[j - 1];
            }
            else {
                ++i;
            }
        }
    }

    return false;

#else
    return pStr.find (sSearchPattern) != std::wstring::npos;
#endif
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
bool
parse_str_to_uint64 (const char *pPtr, uint64_t &pRes) noexcept
{
    for (; *pPtr != 0; ++pPtr) {
        if (*pPtr < '0' || *pPtr > '9') {
            return false;
        }
        pRes = (pRes * 10) + (uint64_t)(*pPtr - '0');
    }

    return true;
}

/**
 * @brief                   Calculates and returns the size of a directory in bytes (-1 if the size can not be found out)
 *
 * @param pDirPath          Path to the directory whose size needs to be calculated
 *
 * @return int64_t          Size of the directory (-1 if size could not be calculated)
 */
[[nodiscard]] int64_t
calc_dir_size (const wchar_t *pPath) noexcept
{
    fs::directory_iterator  iter (pPath, sErrorCode);                                       /** Iterator to the elements within the current directory */
    fs::directory_iterator  fin;                                                            /** Iterator to the element after the last element in the current directory */
    fs::directory_entry     entry;                                                          /** Reference to current entry (used while dereferencing iter) */

    static bool             isDir;                                                          /** Stores whether the current entry is a directory */
    static bool             isFile;                                                         /** Stores whether the current entry is a regular file */
    static bool             isSymlink;                                                      /** Stores whether the current entry is a symlink */
    static bool             isSpecial;                                                      /** Stores whether the current entry is a special file */

    int64_t                 totalDirSize;                                                   /** Stores the total size of this entry */
    int64_t                 curFileSize;                                                    /** Size of file that is being currently processed */

    fs::file_status         entryStatus;                                                    /** Status of the current entry (permissions, type, etc.) */

    // if an error occoured while trying to get the directory iterator, then report it here
    if (sErrorCode.value () != 0) {
        if (get_option (SHOW_ERRORS)) {
            SHOW_ERR (L"Error while creating directory iterator for \"%ls\"",
                    pPath);
        }
        return -1;
    }

    // initialize the size variable
    totalDirSize        = 0;

    // iterate through all the entries within the current directory
    for (fin = end (iter); iter != fin; ++iter) {

        // get the current entry and its status
        entry           = *iter;
        entryStatus     = entry.status (sErrorCode);

        // skip this entry if the status is not available
        if (sErrorCode.value () != 0) {
            if (get_option (SHOW_ERRORS)) {
                SHOW_ERR (L"Error while getting status of \"%ls\"", entry.path ().wstring ().c_str ());
            }
            continue;
        }

        // find out the type of the entry
        isDir           = entry.is_directory ();
        isFile          = entry.is_regular_file ();
        isSymlink       = entry.is_symlink ();
        isSpecial       = entry.is_other ();

        if (isFile) {
            // read the size of the current file and add it to the total size of the directory
            // if the size can not be read, don't add it to the final size
            curFileSize     = fs::file_size (entry, sErrorCode);
            if (sErrorCode.value () != 0) {
                if (get_option (SHOW_ERRORS)) {
                    SHOW_ERR (L"Error while reading size of file \"%ls\"",
                                entry.path ().wstring ().c_str ());
                }
            }
            else {
                totalDirSize    += curFileSize;
            }
        }
        else if (isDir && !isSymlink) {
            curFileSize     = calc_dir_size (entry.path ().wstring ().c_str ());

            if (curFileSize != -1) {
                totalDirSize    += curFileSize;
            }
        }
        // if the file is not of a known type, skip it
        else if (!isSpecial && !isSymlink) {
            wprintf (L"File type of \"%ls\" can not be determined\n",
                    entry.path ().wstring ().c_str ());
            continue;
        }
    }

    return totalDirSize;
}

/**
 * @brief                   Prints the last modified time of a filesystem entry (formatted)
 *
 * @param pFsEntry          Filesystem entry whose last modification time should be printed
 */
void
print_last_modif_time (const fs::directory_entry &pFsEntry) noexcept
{
    static fs::file_time_type   lastModifTpFs;                                              /** Time point when the given entry was last modified */
    static time_t               lastModifTime;                                              /** Time point when the given entry was last modified as a time_t instance */
    static char                 formattedTime[MAX_FMT_TIME_LEN];                            /** Time point when the given entry was last modified formatted as a string */

    // get the timepoint on which it was last modified (the timepoint is on chrono::file_clock)
    lastModifTpFs   = pFsEntry.last_write_time (sErrorCode);

    // check if the time could be read
    if (sErrorCode.value () != 0) {
        if (get_option (SHOW_ERRORS)) {
            SHOW_ERR (L"Error while reading last modified time for \"%ls\"",
                        pFsEntry.path ().wstring ().c_str ());
        }
        // after reporting the error, put 20 blank spaces to indent the current entry
        wprintf (L"%20c", ' ');
    }
    else {
        // convert the time point on chrono::file_clock to a time_t object (time passed since epoch) and format it
        lastModifTime   = chrono::system_clock::to_time_t (
            chrono::file_clock::to_sys (lastModifTpFs));
        strftime (formattedTime,
                    MAX_FMT_TIME_LEN,
                    "%b %d %Y  %H:%M",
                    std::localtime (&lastModifTime));

        wprintf (L"%20s", formattedTime);
    }
}

/**
 * @brief                   Prints the permissions of a filesystem entry (formatted)
 *
 * @param pEntryStatus      Reference to the status of the entry whose permissions need to be printed
 */
void
print_permissions (const fs::file_status &pEntryStatus) noexcept
{
    static fs::perms            entryPerms;                                                 /** Permissions of the current entry */

    entryPerms      = pEntryStatus.permissions ();

    wprintf (L"%c%c%c%c%c%c%c%c%c   ",
            ((entryPerms & fs::perms::owner_read) == fs::perms::none) ? ('r') : ('-'),
            ((entryPerms & fs::perms::owner_write) == fs::perms::none) ? ('w') : ('-'),
            ((entryPerms & fs::perms::owner_exec) == fs::perms::none) ? ('x') : ('-'),
            ((entryPerms & fs::perms::group_read) == fs::perms::none) ? ('r') : ('-'),
            ((entryPerms & fs::perms::group_write) == fs::perms::none) ? ('w') : ('-'),
            ((entryPerms & fs::perms::group_exec) == fs::perms::none) ? ('x') : ('-'),
            ((entryPerms & fs::perms::others_read) == fs::perms::none) ? ('r') : ('-'),
            ((entryPerms & fs::perms::others_write) == fs::perms::none) ? ('w') : ('-'),
            ((entryPerms & fs::perms::others_exec) == fs::perms::none) ? ('x') : ('-')
        );
}

/**
 * @brief                   Scans through and prints the contents of a directory
 *
 * @param pPath             Path to the directory to scan
 * @param pLevel            The number of recursive calls of this function before the current one
 */
void
scan_path (const wchar_t *pPath, const uint64_t &pLevel) noexcept
{
    const uint64_t      indentWidth     = INDENT_COL_WIDTH * pLevel;                        /** Number of spaces to enter before printing the entry for the current function call */

    // the path can not be a nullptr, it must be a valid string (FIND A WAY TO FREE THE STRINGS ALLOCATED IN MAIN)
    if (pPath == nullptr) {
        fwprintf (stderr, L"Path can not be NULL");
        std::exit (-1);
    }

    fs::directory_iterator  iter (pPath, sErrorCode);                                       /** Iterator to the elements within the current directory */
    fs::directory_iterator  fin;                                                            /** Iterator to the element after the last element in the current directory */
    fs::directory_entry     entry;                                                          /** Reference to current entry (used while dereferencing iter) */

    fs::path                filepath;                                                       /** Name of the current entry */
    fs::file_status         entryStatus;                                                    /** Status of the current entry (permissions, type, etc.) */

    static bool             isDir;                                                          /** Stores whether the current entry is a directory */
    static bool             isFile;                                                         /** Stores whether the current entry is a regular file */
    static bool             isSymlink;                                                      /** Stores whether the current entry is a symlink */
    static bool             isSpecial;                                                      /** Stores whether the current entry is a special file */

    int64_t                 totalFileSize;                                                  /** Combines sizes of all files within this directory */
    int64_t                 curFileSize;                                                    /** Size of file that is being currently processed */

    uint64_t                symlinkCnt;                                                     /** Number of symlinks in the current directory */
    uint64_t                regularFileCnt;                                                 /** Number of regular files within this directory */
    uint64_t                specialCnt;                                                     /** Number of special files in the current directory */
    uint64_t                subdirCnt;                                                      /** Number of sub-directories in the current directory */

    fs::path                targetPath;                                                     /** Stores the path to the target of the symlink if the entry is a symlink */

    const char              *specialEntryType;                                              /** Stores the specific type of an entry if it is a special entry (if the specific type can not be determined, stores L"SPECIAL") */

    // if an error occoured while trying to get the directory iterator, then report it here
    if (sErrorCode.value () != 0) {
        if (get_option (SHOW_ERRORS)) {
            SHOW_ERR (L"Error iterating over \"%ls\"",
                        pPath);
        }
        if (pLevel == 0) {
            if (!get_option (SHOW_ERRORS)) {
                wprintf (L"Error iterating over \"%ls\"", pPath);
            }
            sPrintSummary   = false;
        }

        return;
    }

    // initialize all counters to 0
    regularFileCnt      = 0;
    symlinkCnt          = 0;
    specialCnt          = 0;
    subdirCnt           = 0;
    totalFileSize       = 0;

    // iterate through all the files in the current path
    for (fin = end (iter); iter != fin; ++iter) {

        // get the current entry, its name and its status
        entry           = *iter;
        filepath        = entry.path ();
        entryStatus     = entry.status (sErrorCode);

        if (sErrorCode.value () != 0) {
            if (get_option (SHOW_ERRORS)) {
                SHOW_ERR (L"Error while getting status of \"%ls\"", filepath.wstring ().c_str ());
            }
            continue;
        }

        // find out the type of the entry
        isDir           = entry.is_directory ();
        isFile          = entry.is_regular_file ();
        isSymlink       = entry.is_symlink ();
        isSpecial       = entry.is_other ();

        if (get_option (SHOW_ABSNOINDENT)) {

            // if the entry is a symlink, it is necessary to use the absolute path as the canonical path will evaluate and return the target
            filepath    = (isSymlink) ? (fs::absolute (filepath, sErrorCode)) : (fs::canonical (filepath, sErrorCode));

            if (sErrorCode.value () != 0) {
                if (get_option (SHOW_ERRORS)) {
                    SHOW_ERR (L"Error while converting filepath to canonical value for \"%ls\"",
                                filepath.wstring ().c_str ());
                }
            }
        }

        if (isSymlink) {
            ++symlinkCnt;

            if (get_option (SHOW_SYMLINKS)) {
                if (get_option (SHOW_PERMISSIONS)) {
                    print_permissions (entryStatus);
                }

                if (get_option (SHOW_LASTTIME)) {
                    wprintf (L"%20c", '-');
                }

                targetPath  = fs::read_symlink (entry, sErrorCode);

                if (sErrorCode.value () != 0) {
                    if (get_option (SHOW_ERRORS)) {
                        SHOW_ERR (L"Error while reading target of symlink \"%ls\"",
                                    filepath.wstring ().c_str ());
                    }
                }
                else {

                    if (get_option (SHOW_ABSNOINDENT)) {
                        wprintf ((isDir) ? (L"%16s    <%ls> -> <%ls>\n") : (L"%16s    %ls -> %ls\n"),
                                    "SYMLINK",
                                    filepath.wstring ().c_str (),
                                    targetPath.wstring ().c_str ());
                    }
                    else {
                        wprintf ((isDir) ? (L"%16s    %-*c<%ls> -> <%ls>\n") : (L"%16s    %-*c%ls -> %ls\n"),
                                    "SYMLINK",
                                    indentWidth,
                                    ' ',
                                    filepath.filename ().wstring ().c_str (),
                                    targetPath.wstring ().c_str ());
                    }
                }
            }
        }
        else if (isFile) {

            curFileSize     = fs::file_size (entry, sErrorCode);

            if (sErrorCode.value () != 0) {
                if (get_option (SHOW_ERRORS)) {
                    SHOW_ERR (L"Error while reading size of file \"%ls\"",
                                filepath.wstring ().c_str ());
                }

                // if the size can not be read, set the isFile flag to false to indicate a failed read
                curFileSize = -1;
                isFile      = false;
            }
            else {
                totalFileSize   += curFileSize;
            }

            ++regularFileCnt;
            if (get_option (SHOW_FILES)) {

                if (get_option (SHOW_PERMISSIONS)) {
                    print_permissions (entryStatus);
                }

                if (get_option (SHOW_LASTTIME)) {
                    print_last_modif_time (entry);
                }

                if (get_option (SHOW_ABSNOINDENT)) {
                    wprintf (L"%16lld    %ls\n",
                                curFileSize,
                                filepath.wstring ().c_str ());
                }
                else {
                    wprintf (L"%16lld    %-*c%ls\n",
                                curFileSize,
                                indentWidth,
                                ' ',
                                filepath.filename ().wstring ().c_str ());
                }
            }
        }
        else if (isSpecial) {
            ++specialCnt;

            if (get_option (SHOW_SPECIAL)) {
                specialEntryType    = "SPECIAL";

                if (entry.is_socket ()) {
                    specialEntryType    = "SOCKET";
                }
                else if (entry.is_block_file ()) {
                    specialEntryType    = "BLOCK DEVICE";
                }
                else if (entry.is_fifo ()) {
                    specialEntryType    = "FIFO PIPE";
                }

                if (get_option (SHOW_PERMISSIONS)) {
                    print_permissions (entryStatus);
                }

                if (get_option (SHOW_LASTTIME)) {
                    wprintf (L"%24c", ' ');
                }

                if (get_option (SHOW_ABSNOINDENT)) {
                    wprintf (L"%16s    %ls\n",
                                specialEntryType,
                                filepath.wstring ().c_str ());
                }
                else {
                    wprintf (L"%16s    %-*c%ls\n",
                                specialEntryType,
                                indentWidth,
                                ' ',
                                filepath.filename ().wstring ().c_str ());
                }
            }
        }
        else if (isDir) {

            ++subdirCnt;

            if (get_option (SHOW_DIR_SIZE)) {
                curFileSize     = calc_dir_size (filepath.wstring ().c_str ());
            }
            else {
                curFileSize     = -1;
            }

            if (get_option (SHOW_PERMISSIONS)) {
                print_permissions (entryStatus);
            }

            if (get_option (SHOW_LASTTIME)) {
                print_last_modif_time (entry);
            }

            if (get_option (SHOW_ABSNOINDENT)) {
                wprintf (L"%16lld    <%ls>\n",
                            curFileSize,
                            filepath.wstring ().c_str ());
            }
            else {
                wprintf (L"%16lld    %-*c<%ls>\n",
                            curFileSize,
                            indentWidth,
                            ' ',
                            filepath.filename ().wstring ().c_str ());
            }

            if (get_option (SHOW_RECURSIVE)) {
                if ((sRecursionLevel == 0) || (pLevel < sRecursionLevel)) {
                    scan_path (filepath.wstring ().c_str (), 1 + pLevel);
                }
            }
        }
        else {
            printf ("File type of \"%ls\" can not be determined\n",
                    filepath.wstring ().c_str ());
            printf ("Terminating...\n");
            exit (-1);
        }
    }

    sNumFilesTotal      += regularFileCnt;
    sNumSymlinksTotal   += symlinkCnt;
    sNumSpecialTotal    += specialCnt;
    sNumDirsTotal       += subdirCnt;

    if (pLevel == 0) {
        sNumFilesRoot       += regularFileCnt;
        sNumSymlinksRoot    += symlinkCnt;
        sNumSpecialRoot     += specialCnt;
        sNumDirsRoot        += subdirCnt;
    }

    // scanning is complete, now print the summary of the current directory if this function call was not for scanning

    // if the current dir has some files and the show files option was not set (they were not displayed), then print the number of files atleast
    if (regularFileCnt != 0 && !get_option (SHOW_FILES)) {

        // if the permissions and last modification options are set, print gaps before the directory's summary to format it better
        if (get_option (SHOW_PERMISSIONS)) {
            wprintf (L"            ");
        }
        if (get_option (SHOW_LASTTIME)) {
            wprintf (L"%20c", ' ');
        }

        // if either of the noindent options were set, then dont print the indentations for this directory
        if (get_option (SHOW_ABSNOINDENT)) {
            wprintf (L"%16lld    %-*c<%llu files>\n",
                        totalFileSize,
                        (pLevel == 0) ? (0) : (INDENT_COL_WIDTH),   // a single indent needs to be printed if this is not the root dir
                        ' ',
                        regularFileCnt);
        }
        else {
            wprintf (L"%16lld    %-*c<%llu files>\n",
                        totalFileSize,
                        indentWidth,
                        ' ',
                        regularFileCnt);
        }

    }
    // if the current dir has some symlinks and the show symlinks option was not set (they were not displayed), then print the number of symlinks atleast
    if (symlinkCnt != 0 && !get_option (SHOW_SYMLINKS)) {

        // if the permissions and last modification options are set, print gaps before the directory's summary to format it better
        if (get_option (SHOW_PERMISSIONS)) {
            wprintf (L"            ");
        }
        if (get_option (SHOW_LASTTIME)) {
            wprintf (L"%20c", ' ');
        }

        // if either of the noindent options were set, then dont print the indentations for this directory
        if (get_option (SHOW_ABSNOINDENT)) {
            wprintf (L"%16c    %-*c<%llu symlinks>\n",
                        '-',
                        (pLevel == 0) ? (0) : (INDENT_COL_WIDTH),
                        ' ',
                        symlinkCnt);
        }
        else {
            wprintf (L"%16c    %-*c<%llu symlinks>\n",
                        '-',
                        indentWidth,
                        ' ',
                        symlinkCnt);
        }

    }
    // if the current dir has some files and the show files option was not set (they were not displayed), then print the number of files atleast
    if (specialCnt != 0 && !get_option (SHOW_SPECIAL)) {

        // if the permissions and last modification options are set, print gaps before the directory's summary to format it better
        if (get_option (SHOW_PERMISSIONS)) {
            wprintf (L"            ");
        }
        if (get_option (SHOW_LASTTIME)) {
            wprintf (L"%20c", ' ');
        }

        // if either of the noindent options were set, then dont print the indentations for this directory
        if (get_option (SHOW_ABSNOINDENT)) {
            wprintf (L"%16c    %-*c<%llu special entries>\n",
                        '-',
                        (pLevel == 0) ? (0) : (INDENT_COL_WIDTH),
                        ' ',
                        specialCnt);
        }
        else {
            wprintf (L"%16c    %-*c<%llu special entries>\n",
                        '-',
                        indentWidth,
                        ' ',
                        specialCnt);
        }
    }

    return;
}

/**
 * @brief                   Scans throug a directory and prints entries that match the given pattern and search mode
 *
 * @param pPath             Path to the directory to scan
 * @param pLevel            The number of recursive calls of this function before the current one
 */
void
search_path (const wchar_t *pPath, const uint64_t &pLevel) noexcept
{
    if (pPath == nullptr) {
        fwprintf (stderr, L"Path can not be NULL");
        std::exit (-1);
    }


    fs::directory_iterator  iter (pPath, sErrorCode);                                       /** Iterator to the elements within the current directory */
    fs::directory_iterator  fin;                                                            /** Iterator to the element after the last element in the current directory */
    fs::directory_entry     entry;                                                          /** Reference to current entry (used while dereferencing iter) */

    static bool             isDir;                                                          /** Stores whether the current entry is a directory */
    static bool             isFile;                                                         /** Stores whether the current entry is a regular file */
    static bool             isSymlink;                                                      /** Stores whether the current entry is a symlink */
    static bool             isSpecial;                                                      /** Stores whether the current entry is a special file */

    static bool             isMatch;                                                        /** Stores whether the current entry matches the search pattern */

    uint64_t                curFileSize;                                                    /** Size of file that is being currently processed */

    fs::file_status         entryStatus;                                                    /** Status of the current entry (permissions, type, etc.) */

    fs::path                filepath;                                                       /** Name of the current entry */

    const char              *specialEntryType;                                              /** Stores the specific type of an entry if it is a special entry (if the specific type can not be determined, stores L"SPECIAL") */

    fs::path                targetPath;                                                     /** Stores the path to the target of the symlink if the entry is a symlink */


    // if an error occoured while trying to get the directory iterator, then report it here
    if (sErrorCode.value () != 0) {
        if (get_option (SHOW_ERRORS)) {
            SHOW_ERR (L"Error while creating directory iterator for \"%ls\"",
                        pPath);
        }
        if (pLevel == 0) {
            sPrintSummary   = false;
        }

        return;
    }


    for (fin = end (iter); iter != fin; ++iter) {

        // get the current entry, its name and its status
        entry           = *iter;
        filepath        = entry.path ();
        entryStatus     = entry.status (sErrorCode);

        if (sErrorCode.value () != 0) {
            if (get_option (SHOW_ERRORS)) {
                SHOW_ERR (L"Error while getting status of \"%ls\"", filepath.wstring ().c_str ());
            }
            continue;
        }


        // find out the type of the entry
        isDir           = entry.is_directory ();
        isFile          = entry.is_regular_file ();
        isSymlink       = entry.is_symlink ();
        isSpecial       = entry.is_other ();

        // reset the match status
        isMatch         = false;

        if (isSymlink) {
            ++sNumSymlinksTotal;
        }
        else if (isFile) {
            ++sNumFilesTotal;
        }
        else if (isSpecial) {
            ++sNumSpecialTotal;
        }
        else if (isDir) {
            ++sNumDirsTotal;
        }
        else {
            ++sErrorEntries;
        }

        if (get_option (SEARCH_EXACT)) {
            isMatch = filepath.filename ().wstring () == sSearchPattern;
        }
        else if (get_option (SEARCH_NOEXT)) {
            isMatch = filepath.stem ().wstring () == sSearchPattern;
        }
        else { //!
            isMatch = check_contains (filepath.filename ().wstring ());
        }

        if (isMatch) {
            if (isSymlink && get_option (SHOW_SYMLINKS)) {
                ++sNumSymlinksMatched;
            }
            else if (isFile && get_option (SHOW_FILES)) {
                ++sNumFilesMatched;
            }
            else if (isSpecial && get_option (SHOW_SPECIAL)) {
                ++sNumSpecialMatched;
            }
            else if (isDir) {
                ++sNumDirsMatched;
            }
            else {
                isMatch = false;
            }
        }

        if (isMatch) {

            filepath    = fs::canonical (filepath, sErrorCode);
            if (sErrorCode.value () != 0) {
                if (get_option (SHOW_ERRORS)) {
                    SHOW_ERR (L"Error while converting filepath to canonical value for \"%ls\"",
                                filepath.wstring ().c_str ());
                }
            }

            else {
                if (get_option (SHOW_PERMISSIONS)) {
                    print_permissions (entryStatus);
                }

                if (get_option (SHOW_LASTTIME)) {
                    print_last_modif_time (entry);
                }

                if (isSymlink) {
                    wprintf ((isDir) ? (L"%16s    <%ls> -> <%ls>\n") : (L"%16s    %ls -> %ls\n"),
                                "SYMLINK",
                                filepath.wstring ().c_str (),
                                targetPath.wstring ().c_str ());
                }

                else if (isFile) {
                    curFileSize     = fs::file_size (entry, sErrorCode);

                    if (sErrorCode.value () != 0) {
                        if (get_option (SHOW_ERRORS)) {
                            SHOW_ERR (L"Error while reading size of file \"%ls\"",
                                        filepath.wstring ().c_str ());
                        }

                        // if the size can not be read, set the isFile flag to false to indicate a failed read
                        curFileSize = -1;
                    }

                    wprintf (L"%16lld    %ls\n",
                                curFileSize,
                                filepath.wstring ().c_str ());
                }

                else if (isSpecial) {
                    specialEntryType    = "SPECIAL";

                    if (entry.is_socket ()) {
                        specialEntryType    = "SOCKET";
                    }
                    else if (entry.is_block_file ()) {
                        specialEntryType    = "BLOCK DEVICE";
                    }
                    else if (entry.is_fifo ()) {
                        specialEntryType    = "FIFO PIPE";
                    }

                    if (get_option (SHOW_PERMISSIONS)) {
                        print_permissions (entryStatus);
                    }

                    if (get_option (SHOW_LASTTIME)) {
                        wprintf (L"%24c", ' ');
                    }

                    wprintf (L"%16s    %ls\n",
                                specialEntryType,
                                filepath.wstring ().c_str ());
                }

                else if (isDir) {

                    if (get_option (SHOW_DIR_SIZE)) {
                        // curFileSize = scan_path<true> (filepath.wstring ().c_str (), 0);
                        curFileSize = calc_dir_size (filepath.wstring ().c_str ());
                    }
                    else {
                        curFileSize = -1;
                    }

                    wprintf (L"%16lld    <%ls>\n",
                            curFileSize,
                            filepath.wstring ().c_str ());
                }
            }

        }

        if (isDir && !isSymlink && get_option (SHOW_RECURSIVE)) {
            if ((sRecursionLevel == 0) || (pLevel < sRecursionLevel)) {
                search_path (filepath.wstring ().c_str (), 1 + pLevel);
            }
        }
    }
}

/**
 * @brief
 *
 * @param pPath
 */
void
scan_path_init (const wchar_t *pPath) noexcept
{
    sPrintSummary   = true;

    scan_path (pPath, 0);

    if (!sPrintSummary) {
        return;
    }

    wprintf (rootSum,
                pPath,
                sNumFilesRoot,
                sNumSymlinksRoot,
                sNumSpecialRoot,
                sNumDirsRoot,
                sNumFilesRoot +
                sNumSymlinksRoot +
                sNumSpecialRoot +
                sNumDirsRoot);

    if (get_option (SHOW_RECURSIVE)) {
        wprintf (recSum,
                    sNumFilesTotal,
                    sNumSymlinksTotal,
                    sNumSpecialTotal,
                    sNumDirsTotal,
                    sNumFilesTotal +
                    sNumSymlinksTotal +
                    sNumSpecialTotal +
                    sNumDirsTotal);
    }
}

/**
 * @brief
 *
 * @param pPath
 */
void
search_path_init (const wchar_t *pPath) noexcept
{
    sPrintSummary   = true;

    wprintf (L"Searching for %ls\n\n", sSearchPattern);

    search_path (pPath, 0);

    if (!sPrintSummary) {
        return;
    }

    wprintf (foundSum,
                sNumFilesMatched,
                sNumSymlinksMatched,
                sNumSpecialMatched,
                sNumDirsMatched,
                sNumFilesMatched +
                sNumSymlinksMatched +
                sNumSpecialMatched +
                sNumDirsMatched);
    wprintf (TotalSum,
                pPath,
                sNumFilesTotal,
                sNumSymlinksTotal,
                sNumSpecialTotal,
                sNumDirsTotal,
                sNumFilesTotal +
                sNumSymlinksTotal +
                sNumSpecialTotal +
                sNumDirsTotal);
}


int
main (int argc, char *argv[]) noexcept
{
    const char          *initPathStr;                                                       /** Path to start the scan process from, as a narrow string */
    const char          *searchPattern;                                                     /** Pattern to search for, as a narrow string */

    // initialize all paths to null (represents a value that has not been provided)
    initPathStr         = nullptr;
    searchPattern       = nullptr;

    // iterate through all the command line arguments (except the first one, which is the name of the executable)
    for (uint64_t i = 1, argLen; i < (uint64_t)argc; ++i) {

        // get the length of the current argument and ignore it if it is not greater than 0
        argLen  = strnlen (argv[i], MAX_ARG_LEN);
        if (argLen <= 0) {
            wprintf (L"Ignoring Unknown Option of length 0\n");
            continue;
        }

        // if this is the first argument after the executable name, check if it is a path (the first character must not be a '-')
        if (i == 1 && argv[i][0] != '-') {
            initPathStr = argv[i];
            continue;
        }

        // depending on the length of the argument, try to match it with any of the options
        switch (argLen) {

        case 2:
            if (strncmp (argv[i], "-h", 2) == 0) {
                set_option (HELP);
            }
            else if (strncmp (argv[i], "-r", 2) == 0) {
                set_option (SHOW_RECURSIVE);

                // if the user has provided the number of levels, then parse it into a string
                if ((i + 1) < (uint64_t)argc
                    && strnlen (argv[i + 1], MAX_ARG_LEN) != 0
                    && argv[i + 1][0] != '-') {
                    if (!parse_str_to_uint64 (argv[i + 1], sRecursionLevel)) {
                        printf ("Invalid value for recursion depth \"%s\"\nPlease provide a positive whole number\n", argv[i + 1]);
                        return -1;
                    }
                    ++i;
                }
            }
            else if (strncmp (argv[i], "-t", 2) == 0) {
                set_option (SHOW_LASTTIME);
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
                    wprintf (L"No Search pattern provided after \"%s\" flag\n", argv[i]);
                    wprintf (L"Terminating...\n");
                    std::exit (-1);
                }

                // set the option and skip the next argument (that is the search pattern)
                set_option (SEARCH_EXACT);
                searchPattern = argv[++i];
            }
            else if (strncmp (argv[i], "-e", 2) == 0) {
                set_option (SHOW_ERRORS);
            }
            else {
                printf ("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

        case 5:

            if (strncmp (argv[i], "--abs", 5) == 0) {
                set_option (SHOW_ABSNOINDENT);
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
            if (strncmp (argv[i], "--search", 8) == 0) {

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
            else if (strncmp (argv[i], "--hide-err", 10) == 0) {
                set_option (SHOW_ERRORS);
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
                    wprintf (L"No Search pattern provided after \"%s\" flag\n", argv[i]);
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

        case 19:
            if (strncmp (argv[i], "--modification-time", 19) == 0) {
                set_option (SHOW_LASTTIME);
            }
            else {
                printf ("Ignoring Unknown Option \"%s\"\n", argv[i]);
            }
            break;

        case 20:
            if (strncmp (argv[i], "--recursive-dir-size", 20) == 0) {
                set_option (SHOW_DIR_SIZE);
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
        wprintf (usage, argv[0]);
        return 0;
    }

    // convert the provided path to a wide string (if no path was provided, use the relative path to the working directory '.')
    // it is very important to not use L"." directory, since sInitPath is freed at the end of the program
    // using a string literal would cause wrong behaviour (possibly crash the program)
    sInitPath           = widen_string ((initPathStr == nullptr) ? (".") : (initPathStr));

    // if a search pattern was provided, convert it to a wide string and use the search function
    if (searchPattern != nullptr) {
        sSearchPattern  = widen_string (searchPattern);

#ifdef USE_KMP_SEARCH
        if (get_option (SEARCH_CONTAINS)) {
            sSearchPatternLen   = strnlen (searchPattern, MAX_ARG_LEN);
            compute_lps ();

            // for (auto &e : sLpsArray) {
            //     fprintf (stderr, "%lu\t", e);
            //     fprintf (stderr, "\n");
            // }
        }
#endif
        search_path_init (sInitPath);
    }
    // if no search pattern was provided, use the regular scan function
    else {
        scan_path_init (sInitPath);
    }

    free ((void *)sInitPath);
    free ((void *)sSearchPattern);

    return 0;
}
