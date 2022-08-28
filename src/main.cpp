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


/** Option that specifies if directories should be recursively scanned and displayed */
#define SHOW_RECURSIVE          (0)

#if defined (_WIN32) || (_WIN64)
#else

/** Option that specified if the permissions of a filesystem entry should be printed */
#define SHOW_PERMISSIONS        (1)
#endif

/** Option that specified if the last modification time of a file or directory should be printed */
#define SHOW_LASTTIME           (2)


/** Option that specifies if the absolute paths of all entries should be printed without indentation */
#define SHOW_ABSNOINDENT        (3)


/** Option that specifies if all files within a directory need to be individually displayed */
#define SHOW_FILES              (5)

/** Option that specifies if all symlinks within a directory need to be individually displayed */
#define SHOW_SYMLINKS           (6)

/** Option that specifies if all special files (such as sockets, block devices etc.) within a directory need to be individually displayed */
#define SHOW_SPECIAL            (7)


/** Option that specifies if only those entries whose name matches a given pattern should be shown */
#define SEARCH_EXACT            (8)

/** Option that specifies if only those entries whose name (without the extension) matches a given pattern should be shown */
#define SEARCH_NOEXT            (9)

/** Option that specifies if only those entries whose name contains a given pattern should be shown */
#define SEARCH_CONTAINS         (10)


/** Option that specifies if directory sizes should be recursively calculated and shown */
#define SHOW_DIR_SIZE           (11)
#define SHOW_ERRORS             (12)


/** Option that specifies if usage instructions need to be printed */
#define HELP                    (13)


/** Maximum allowed length of an argument (other than the path) after which it is not checked further */
#define MAX_ARG_LEN             (32)

/** Maximum allowed length of the provided path after which any further characters are ignored */
#define MAX_PATH_LEN            (256)

/** Maximum allowed length of the string that stores the last modified time (formatted) of a file */
#define MAX_FMT_TIME_LEN        (64)

/** Maximum allowed length of the string that stores a formatted integer */
#define MAX_FMT_INT_LEN         (32)


/** Number of spaces by which to further indent each subsequent nested directory's entries */
#define INDENT_COL_WIDTH        (4)


/** Defined if Knuth Morris Prat search needs to be used */
// #define USE_KMP_SEARCH          true

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

/** Determines if each error entry should have empty lines before and after */
#if defined (ERR_NEWL)
#define SHOW_ERR(pMsg, ...)     fwprintf (stderr,                               \
                                            L"\n"                               \
                                            pMsg L" (Code %d, %hs)\n",          \
                                            L"\n",                              \
                                            __VA_ARGS__,                        \
                                            sErrorCode.value (),                \
                                            sErrorCode.message ().c_str ());                /** Macro to print errors in a well formatted manner */
#else
#define SHOW_ERR(pMsg, ...)     fwprintf (stderr,                               \
                                            pMsg L" (Code %d, %hs)\n",          \
                                            __VA_ARGS__,                        \
                                            sErrorCode.value (),                \
                                            sErrorCode.message ().c_str ());                /** Macro to print errors in a well formatted manner */
#endif

// trick to see if a type is signed
// for signed types, ~int_t (0) < int_t (0)
// for unsigned types, ~int_t (0) > int_t (0)

// this is because the most significant bit is treated as a negative value
// in signed types, and the result becomes -1

/** Macro to check if a type is a signed type or not */
#define is_signed_type(int_t)   (~int_t(0) < int_t(0))

namespace fs                    = std::filesystem;
namespace chrono                = std::chrono;

/** Usage instructions of the program */
static const wchar_t    *usage      = L"\n"
                                    L"File System Scanner (dumblebots.com)\n"
                                    L"\n"
                                    L"Usage: %hs [PATH] [options]\n"
                                    L"Scan through the filesystem starting from PATH.\n"
                                    L"\n"
                                    L"Example: %hs \"..\" --recursive --files\n"
                                    L"\n"
                                    L"Options:\n"
                                    L"-r, --recursive             Recursively scan directories (can be followed by a positive integer to indicate the depth)\n"
#if defined (_WIN32) || defined (_WIN64)
#else
                                    L"-p, --permissions           Show Permissions of all entries\n"
#endif
                                    L"-t, --modification-time     Show time of last modification of entries\n"
                                    L"\n"
                                    L"-f, --files                 Show Regular Files (normally hidden)\n"
                                    L"-l, --symlinks              Show Symlinks (normally hidden)\n"
                                    L"-s, --special               Show Special Files such as sockets, pipes, etc. (normally hidden)\n"
                                    L"\n"
                                    L"-d, --dir-size              Recursively calculate and display the size of each directory\n"
                                    L"\n"
                                    L"-a, --abs                   Show the absolute path of each entry without any indentation\n"
                                    L"\n"
                                    L"-S, --search                Only show entries whose name completely matches the following string completely\n"
                                    L"    --search-noext          Only show entries whose name(except for the extension) matches the following string completely\n"
                                    L"    --contains              Only show entries whose name contains the following string completely\n"
                                    L"\n"
                                    L"-e, --show-err              Show errors\n"
                                    L"-h, --help                  Print Usage Instructions\n"
                                    L"\n";

/** Unformatted summary string for directory to Totalerse (not including subdirectories) */
static const wchar_t    *rootSum    = L"\n"
                                    L"Summary of \"%ls\"\n"
                                    L"<%hs files>\n"
                                    L"<%hs symlinks>\n"
                                    L"<%hs special files>\n"
                                    L"<%hs subdirectories>\n"
                                    L"<%hs total entries>\n"
                                    L"\n";

/** Unformatted summary string for the directory to Totalerse (including subdirectories) */
static const wchar_t    *recSum     = L"Including subdirectories\n"
                                    L"<%hs files>\n"
                                    L"<%hs symlinks>\n"
                                    L"<%hs special files>\n"
                                    L"<%hs subdirectories>\n"
                                    L"<%hs total entries>\n"
                                    L"\n";

/** Unformatted summary string for number of entries found matching search pattern (in search mode) */
static const wchar_t    *foundSum   = L"\n"
                                    L"Summary of matching entries\n"
                                    L"<%hs files>\n"
                                    L"<%hs symlinks>\n"
                                    L"<%hs special files>\n"
                                    L"<%hs subdirectories>\n"
                                    L"<%hs total entries>\n"
                                    L"\n";

/** Unformatted summary string for number of entries traversed while matching search pattern (in search mode) */
static const wchar_t    *TotalSum    = L"Summary of traversal of \"%ls\"\n"
                                    L"<%hs files>\n"
                                    L"<%hs symlinks>\n"
                                    L"<%hs special files>\n"
                                    L"<%hs subdirectories>\n"
                                    L"<%hs total entries>\n"
                                    L"\n";

static const wchar_t    *sInitPath          {nullptr};                                      /** Initial path to start the scan from */
static const wchar_t    *sSearchPattern     {nullptr};                                      /** Pattern to search for if any of the search options are set */

#if defined (USE_KMP_SEARCH)
static uint64_t         sSearchPatternLen   {};
#endif

/** Bitmask to represent command line options provided by the user */
static uint64_t         sOptionMask         {};
/** Container for error code to be passed around when dealing with std::filesystem artifacts */
static std::error_code  sErrorCode          {};

/** Number of levels of directories to go within if the recursive option is set */
static uint64_t         sRecursionLevel     {};

/** Total number of files traversed */
static uint64_t         sNumFilesTotal      {};
/** Total number of Symlinks traversed */
static uint64_t         sNumSymlinksTotal   {};
/** Total number of special files traversed */
static uint64_t         sNumSpecialTotal    {};
/** Total number of directories traversed */
static uint64_t         sNumDirsTotal       {};

/** Number of files traversed in the root directory */
static uint64_t         sNumFilesRoot       {};
/** Number of symlinks traversed in the root directory */
static uint64_t         sNumSymlinksRoot    {};
/** Number of special files traversed in the root directory */
static uint64_t         sNumSpecialRoot     {};
/** Number of subdirectories in the root directory */
static uint64_t         sNumDirsRoot        {};

/** Number of files matching the search pattern */
static uint64_t         sNumFilesMatched    {};
/** Number of symlinks matching the search pattern */
static uint64_t         sNumSymlinksMatched {};
/** Number of special files matching the search pattern */
static uint64_t         sNumSpecialMatched  {};
/** Number of subdirectories matching the search pattern */
static uint64_t         sNumDirsMatched     {};

/** Flag to determine whether the summary should be printed or not */
static bool             sPrintSummary       {};

/** Buffer to use for storing formatted integers */
static char             sFmtIntBuff[MAX_FMT_INT_LEN];

#if defined (USE_KMP_SEARCH)
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
    // a given bit can be set by generating a bitmask where only the desired bit is set
    // and performing a logical OR operation between the initial value and the mask
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
    // a given bit can be cleared by generating a bitmask where only the desired bit is not
    // set and performing a logical AND operation between the initial value and the mask
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

    /** Length of the string to convert to a wide string (including the null termination character) */
    uint64_t    len;
    /** Resultant wide string after the conversion */
    wchar_t     *result {nullptr};

    // calculate the length of the input string and allocate a wide string of the corresponding size
    // this string needs to be freed by a call to free
    len     = strnlen (pPtr, MAX_PATH_LEN) + 1;
    result  = (wchar_t *)malloc (sizeof (wchar_t) * len);

    // memset (result, 0, sizeof (wchar_t) * len);

    if (result == nullptr) {
        fprintf (stderr,
        "Could not allocate Wide String when converting path\n");
        std::exit (-1);
    }

    // On Windows and Linux/OSX, sizeof (char) is 1 and the encoding is UTF-8
    // On Windows, sizeof (wchar_t) is 2 and the encoding is UTF-16
    // On Linux/OSX, sizeof (wchar_t) is 4 and the encoding is UTF-32

    // Simply copying the char (byte) to the least significant byte of the wchar_t
    // should be enough to create the new wide string
    // the remaining bytes can be cleared

    for (uint64_t i = 0; i < len; ++i) {
        ((char *)&result[i])[0] = pPtr[i];
        ((char *)&result[i])[1] = 0;

        if constexpr (sizeof (wchar_t) >= 4) {
            ((char *)&result[i])[2] = 0;
            ((char *)&result[i])[3] = 0;
        }
    }

    // if (std::mbstowcs (result, pPtr, len) != len) {
    //     fprintf (stderr,
    //     "Could not convert charace String when converting path\n");
    //     std::exit (-1);
    // }

    return result;
}

template <typename int_t>
[[nodiscard]] char
*format_int (int_t pValue)
{
    /** */
    uint8_t         buffLen;
    /** */
    uint8_t         digit;

    /** */
    bool            isSigned;

    if (pValue == 0) {
        sFmtIntBuff[0] = '0';
        sFmtIntBuff[1] = 0;

        return sFmtIntBuff;
    }

    if constexpr (is_signed_type (int_t)) {
        isSigned    = false;
        if (pValue < 0) {
            pValue      = -pValue;
            isSigned    = true;
        }
    }

    for (buffLen = 0; pValue != 0; ) {
        digit                   = pValue % 10;
        pValue                  = pValue / 10;

        sFmtIntBuff[buffLen++]  = '0' + digit;

        if (((buffLen % 4) == 3) && (pValue != 0)) {
            sFmtIntBuff[buffLen++]  = ',';
        }
    }

    if constexpr (is_signed_type (int_t)) {
        if (isSigned) {
            sFmtIntBuff[buffLen++]  = '-';
        }
    }

    for (int i = 0; i < buffLen / 2; ++i) {
        std::swap (sFmtIntBuff[i], sFmtIntBuff[buffLen - i - 1]);
    }

    sFmtIntBuff[buffLen]    = 0;

    return sFmtIntBuff;
}

#if defined (USE_KMP_SEARCH)/**
 * @brief                   Computes the LPS array for KMP search
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

#if defined (USE_KMP_SEARCH)
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
    // go through all characters and discard the string if any of the characters is not a decimal digit
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
    /** Iterator to the elements within the current directory */
    fs::directory_iterator  iter (pPath, sErrorCode);
    /** Iterator to the element after the last element in the current directory */
    fs::directory_iterator  fin;
    /** Reference to current entry (used while dereferencing iter) */
    fs::directory_entry     entry;

    /** Stores whether the current entry is a directory */
    static bool             isDir;
    /** Stores whether the current entry is a regular file */
    static bool             isFile;
    /** Stores whether the current entry is a symlink */
    static bool             isSymlink;
    /** Stores whether the current entry is a special file */
    static bool             isSpecial;

    /** Stores the total size of this entry */
    int64_t                 totalDirSize;
    /** Size of file that is being currently processed */
    int64_t                 curFileSize;

    /** Status of the current entry (permissions, type, etc.) */
    fs::file_status         entryStatus;

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

        // check if the entry is a regular file
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
        // check if the entry is a directory and make sure it is not a symlink (prevents circular scanning)
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
    /** Time point when the given entry was last modified */
    static fs::file_time_type   lastModifTpFs;
    /** Time point when the given entry was last modified as a time_t instance */
    static time_t               lastModifTime;
    /** Time point when the given entry was last modified formatted as a string */
    static char                 formattedTime[MAX_FMT_TIME_LEN];

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

#if defined (_WIN32) || defined (_WIN64)
        lastModifTime   = chrono::system_clock::to_time_t (
                        chrono::utc_clock::to_sys (
                                chrono::file_clock::to_utc (lastModifTpFs)
                                )
                        );
#else
        lastModifTime   = chrono::system_clock::to_time_t (
            chrono::file_clock::to_sys (lastModifTpFs));
#endif

        strftime (formattedTime,
                    MAX_FMT_TIME_LEN,
                    "%b %d %Y  %H:%M",
                    std::localtime (&lastModifTime));

        wprintf (L"%20hs", formattedTime);
    }
}

#if defined (_WIN32) || defined (_WIN64)
#else

/**
 * @brief                   Prints the permissions of a filesystem entry (formatted)
 *
 * @param pEntryStatus      Reference to the status of the entry whose permissions need to be printed
 */
void
print_permissions (const fs::file_status &pEntryStatus) noexcept
{
    /** Permissions of the current entry */
    static fs::perms            entryPerms;

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
#endif

/**
 * @brief                   Scans through and prints the contents of a directory
 *
 * @param pPath             Path to the directory to scan
 * @param pLevel            The number of recursive calls of this function before the current one
 */
void
scan_path (const wchar_t *pPath, const uint64_t &pLevel) noexcept
{
    /** Number of spaces to enter before printing the entry for the current function call */
    const uint64_t      indentWidth     = INDENT_COL_WIDTH * pLevel;

    // the path can not be a nullptr, it must be a valid string (FIND A WAY TO FREE THE STRINGS ALLOCATED IN MAIN)
    if (pPath == nullptr) {
        fwprintf (stderr, L"Path can not be NULL");
        std::exit (-1);
    }

    /** Iterator to the elements within the current directory */
    fs::directory_iterator  iter (pPath, sErrorCode);
    /** Iterator to the element after the last element in the current directory */
    fs::directory_iterator  fin;
    /** Reference to current entry (used while dereferencing iter) */
    fs::directory_entry     entry;

    /** Name of the current entry */
    fs::path                filepath;
    /** Status of the current entry (permissions, type, etc.) */
    fs::file_status         entryStatus;

    /** Stores whether the current entry is a directory */
    static bool             isDir;
    /** Stores whether the current entry is a regular file */
    static bool             isFile;
    /** Stores whether the current entry is a symlink */
    static bool             isSymlink;
    /** Stores whether the current entry is a special file */
    static bool             isSpecial;

    /** Combines sizes of all files within this directory */
    int64_t                 totalFileSize;
    /** Size of file that is being currently processed */
    int64_t                 curFileSize;

    /** Number of symlinks in the current directory */
    uint64_t                symlinkCnt;
    /** Number of regular files within this directory */
    uint64_t                regularFileCnt;
    /** Number of special files in the current directory */
    uint64_t                specialCnt;
    /** Number of sub-directories in the current directory */
    uint64_t                subdirCnt;

    /** Stores the path to the target of the symlink if the entry is a symlink */
    fs::path                targetPath;

    /** Stores the specific type of an entry if it is a special entry (if the specific type can not be determined, stores L"SPECIAL") */
    const char              *specialEntryType;

    /** Buffer to store the total size of files formatted with periods */
    static char             fmtIntBuff[MAX_FMT_INT_LEN];

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

#if defined (_WIN32) || defined (_WIN64)
#else
                if (get_option (SHOW_PERMISSIONS)) {
                    print_permissions (entryStatus);
                }
#endif

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
                        wprintf ((isDir) ? (L"%16hs    <%ls> -> <%ls>\n") : (L"%16hs    %ls -> %ls\n"),
                                    "SYMLINK",
                                    filepath.wstring ().c_str (),
                                    targetPath.wstring ().c_str ());
                    }
                    else {
                        wprintf ((isDir) ? (L"%16hs    %-*c<%ls> -> <%ls>\n") : (L"%16hs    %-*c%ls -> %ls\n"),
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

#if defined (_WIN32) || defined (_WIN64)
#else
                if (get_option (SHOW_PERMISSIONS)) {
                    print_permissions (entryStatus);
                }
#endif

                if (get_option (SHOW_LASTTIME)) {
                    print_last_modif_time (entry);
                }

                if (get_option (SHOW_ABSNOINDENT)) {
                    wprintf (L"%16hs    %ls\n",
                                format_int (curFileSize),
                                filepath.wstring ().c_str ());
                }
                else {
                    wprintf (L"%16hs    %-*c%ls\n",
                                format_int (curFileSize),
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

#if defined (_WIN32) || defined (_WIN64)
#else
                if (get_option (SHOW_PERMISSIONS)) {
                    print_permissions (entryStatus);
                }
#endif

                if (get_option (SHOW_LASTTIME)) {
                    wprintf (L"%20c", ' ');
                }

                if (get_option (SHOW_ABSNOINDENT)) {
                    wprintf (L"%16hs    %ls\n",
                                specialEntryType,
                                filepath.wstring ().c_str ());
                }
                else {
                    wprintf (L"%16hs    %-*c%ls\n",
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

#if defined (_WIN32) || defined (_WIN64)
#else
            if (get_option (SHOW_PERMISSIONS)) {
                print_permissions (entryStatus);
            }
#endif

            if (get_option (SHOW_LASTTIME)) {
                print_last_modif_time (entry);
            }

            if (get_option (SHOW_ABSNOINDENT)) {
                wprintf (L"%16hs    <%ls>\n",
                            (curFileSize == -1) ? (" ") : format_int (curFileSize),
                            filepath.wstring ().c_str ());
            }
            else {
                wprintf (L"%16hs    %-*c<%ls>\n",
                            (curFileSize == -1) ? (" ") : format_int (curFileSize),
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
            wprintf (L"File type of \"%ls\" can not be determined\n",
                    filepath.wstring ().c_str ());
            wprintf (L"Terminating...\n");
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

#if defined (_WIN32) || defined (_WIN64)
#else
        // if the permissions and last modification options are set, print gaps before the directory's summary to format it better
        if (get_option (SHOW_PERMISSIONS)) {
            wprintf (L"            ");
        }
#endif

        if (get_option (SHOW_LASTTIME)) {
            wprintf (L"%20c", ' ');
        }

        strcpy (fmtIntBuff, format_int (totalFileSize));

        // if either of the noindent options were set, then dont print the indentations for this directory
        if (get_option (SHOW_ABSNOINDENT)) {
            wprintf (L"%16hs    %-*c<%hs files>\n",
                        fmtIntBuff,
                        (pLevel == 0) ? (0) : (INDENT_COL_WIDTH),   // a single indent needs to be printed if this is not the root dir
                        ' ',
                        format_int (regularFileCnt));
        }
        else {
            wprintf (L"%16hs    %-*c<%hs files>\n",
                        fmtIntBuff,
                        indentWidth,
                        ' ',
                        format_int (regularFileCnt));
        }

    }
    // if the current dir has some symlinks and the show symlinks option was not set (they were not displayed), then print the number of symlinks atleast
    if (symlinkCnt != 0 && !get_option (SHOW_SYMLINKS)) {

#if defined (_WIN32) || defined (_WIN64)
#else
        // if the permissions and last modification options are set, print gaps before the directory's summary to format it better
        if (get_option (SHOW_PERMISSIONS)) {
            wprintf (L"            ");
        }
#endif

        if (get_option (SHOW_LASTTIME)) {
            wprintf (L"%20c", ' ');
        }

        // if either of the noindent options were set, then dont print the indentations for this directory
        if (get_option (SHOW_ABSNOINDENT)) {
            wprintf (L"%16c    %-*c<%hs symlinks>\n",
                        '-',
                        (pLevel == 0) ? (0) : (INDENT_COL_WIDTH),
                        ' ',
                        format_int (symlinkCnt));
        }
        else {
            wprintf (L"%16c    %-*c<%hs symlinks>\n",
                        '-',
                        indentWidth,
                        ' ',
                        format_int (symlinkCnt));
        }

    }
    // if the current dir has some files and the show files option was not set (they were not displayed), then print the number of files atleast
    if (specialCnt != 0 && !get_option (SHOW_SPECIAL)) {

#if defined (_WIN32) || defined (_WIN64)
#else
        // if the permissions and last modification options are set, print gaps before the directory's summary to format it better
        if (get_option (SHOW_PERMISSIONS)) {
            wprintf (L"            ");
        }
#endif

        if (get_option (SHOW_LASTTIME)) {
            wprintf (L"%20c", ' ');
        }

        // if either of the noindent options were set, then dont print the indentations for this directory
        if (get_option (SHOW_ABSNOINDENT)) {
            wprintf (L"%16c    %-*c<%hs special entries>\n",
                        '-',
                        (pLevel == 0) ? (0) : (INDENT_COL_WIDTH),
                        ' ',
                        format_int (specialCnt));
        }
        else {
            wprintf (L"%16c    %-*c<%llu special entries>\n",
                        '-',
                        indentWidth,
                        ' ',
                        format_int (specialCnt));
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


    /** Iterator to the elements within the current directory */
    fs::directory_iterator  iter (pPath, sErrorCode);
    /** Iterator to the element after the last element in the current directory */
    fs::directory_iterator  fin;
    /** Reference to current entry (used while dereferencing iter) */
    fs::directory_entry     entry;

    /** Stores whether the current entry is a directory */
    static bool             isDir;
    /** Stores whether the current entry is a regular file */
    static bool             isFile;
    /** Stores whether the current entry is a symlink */
    static bool             isSymlink;
    /** Stores whether the current entry is a special file */
    static bool             isSpecial;

    /** Stores whether the current entry matches the search pattern */
    static bool             isMatch;

    /** Size of file that is being currently processed */
    int64_t                 curFileSize;

    /** Status of the current entry (permissions, type, etc.) */
    fs::file_status         entryStatus;

    /** Name of the current entry */
    fs::path                filepath;

    /** Stores the specific type of an entry if it is a special entry (if the specific type can not be determined, stores L"SPECIAL") */
    const char              *specialEntryType;

    /** Stores the path to the target of the symlink if the entry is a symlink */
    fs::path                targetPath;


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

        if (get_option (SEARCH_EXACT)) {
            isMatch = filepath.filename ().wstring () == sSearchPattern;
        }
        else if (get_option (SEARCH_NOEXT)) {
            isMatch = filepath.stem ().wstring () == sSearchPattern;
        }
        else {
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

#if defined (_WIN32) || defined (_WIN64)
#else
                if (get_option (SHOW_PERMISSIONS)) {
                    print_permissions (entryStatus);
                }
#endif

                if (get_option (SHOW_LASTTIME)) {
                    print_last_modif_time (entry);
                }

                if (isSymlink) {
                    wprintf ((isDir) ? (L"%16hs    <%ls> -> <%ls>\n") : (L"%16hs    %ls -> %ls\n"),
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

                    wprintf (L"%16hs    %ls\n",
                                format_int (curFileSize),
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

#if defined (_WIN32) || defined (_WIN64)
#else
                    if (get_option (SHOW_PERMISSIONS)) {
                        print_permissions (entryStatus);
                    }
#endif

                    if (get_option (SHOW_LASTTIME)) {
                        wprintf (L"%20c", ' ');
                    }

                    wprintf (L"%16hs    %ls\n",
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

                    wprintf (L"%16hs    <%ls>\n",
                            (curFileSize == -1) ? (" ") : format_int (curFileSize),
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
    /** Buffer to store the number of files formatted with periods */
    char    numFilesFmt[MAX_FMT_INT_LEN];
    /** Buffer to store the number of symlinks formatted with periods */
    char    numSymlinksFmt[MAX_FMT_INT_LEN];
    /** Buffer to store the number of special files formatted with periods */
    char    numSpecialFmt[MAX_FMT_INT_LEN];
    /** Buffer to store the number of directories formatted with periods */
    char    numDirsFmt[MAX_FMT_INT_LEN];
    /** Buffer to store the total number of filesystem entries formatted with periods */
    char    numTotalFmt[MAX_FMT_INT_LEN];

    sPrintSummary   = true;

    scan_path (pPath, 0);

    if (!sPrintSummary) {
        return;
    }

    strcpy (numFilesFmt, format_int (sNumFilesRoot));
    strcpy (numSymlinksFmt, format_int (sNumSymlinksRoot));
    strcpy (numSpecialFmt, format_int (sNumSpecialRoot));
    strcpy (numDirsFmt, format_int (sNumDirsRoot));
    strcpy (numTotalFmt, format_int (sNumFilesRoot +
                                sNumSymlinksRoot +
                                sNumSpecialRoot +
                                sNumDirsRoot));

    wprintf (rootSum,
                pPath,
                numFilesFmt,
                numSymlinksFmt,
                numSpecialFmt,
                numDirsFmt,
                numTotalFmt);

    if (get_option (SHOW_RECURSIVE)) {
        strcpy (numFilesFmt, format_int (sNumFilesTotal));
        strcpy (numSymlinksFmt, format_int (sNumSymlinksTotal));
        strcpy (numSpecialFmt, format_int (sNumSpecialTotal));
        strcpy (numDirsFmt, format_int (sNumDirsTotal));
        strcpy (numTotalFmt, format_int (sNumFilesTotal +
                                    sNumSymlinksTotal +
                                    sNumSpecialTotal +
                                    sNumDirsTotal));

        wprintf (recSum,
                    numFilesFmt,
                    numSymlinksFmt,
                    numSpecialFmt,
                    numDirsFmt,
                    numTotalFmt);
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
    /** Buffer to store the number of files formatted with periods */
    char    numFilesFmt[MAX_FMT_INT_LEN];
    /** Buffer to store the number of symlinks formatted with periods */
    char    numSymlinksFmt[MAX_FMT_INT_LEN];
    /** Buffer to store the number of special files formatted with periods */
    char    numSpecialFmt[MAX_FMT_INT_LEN];
    /** Buffer to store the number of directories formatted with periods */
    char    numDirsFmt[MAX_FMT_INT_LEN];
    /** Buffer to store the total number of filesystem entries formatted with periods */
    char    numTotalFmt[MAX_FMT_INT_LEN];

    sPrintSummary   = true;

    wprintf (L"Searching for %ls\n\n", sSearchPattern);

    search_path (pPath, 0);

    if (!sPrintSummary) {
        return;
    }

    strcpy (numFilesFmt, format_int (sNumFilesMatched));
    strcpy (numSymlinksFmt, format_int (sNumSymlinksMatched));
    strcpy (numSpecialFmt, format_int (sNumSpecialMatched));
    strcpy (numDirsFmt, format_int (sNumDirsMatched));
    strcpy (numTotalFmt, format_int (sNumFilesMatched +
                                sNumSymlinksMatched +
                                sNumSpecialMatched +
                                sNumDirsMatched));

    wprintf (foundSum,
                numFilesFmt,
                numSymlinksFmt,
                numSpecialFmt,
                numDirsFmt,
                numTotalFmt);

    strcpy (numFilesFmt, format_int (sNumFilesTotal));
    strcpy (numSymlinksFmt, format_int (sNumSymlinksTotal));
    strcpy (numSpecialFmt, format_int (sNumSpecialTotal));
    strcpy (numDirsFmt, format_int (sNumDirsTotal));
    strcpy (numTotalFmt, format_int (sNumFilesTotal +
                                sNumSymlinksTotal +
                                sNumSpecialTotal +
                                sNumDirsTotal));

    wprintf (TotalSum,
                pPath,
                numFilesFmt,
                numSymlinksFmt,
                numSpecialFmt,
                numDirsFmt,
                numTotalFmt);
}


int
main (int argc, char *argv[]) noexcept
{
    /** Path to start the scan process from, as a narrow string */
    const char          *initPathStr;
    /** Pattern to search for, as a narrow string */
    const char          *searchPattern;

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
            if (strncmp (argv[i], "-r", 2) == 0) {
                set_option (SHOW_RECURSIVE);

                // if the user has provided the number of levels, then parse it into a string
                if ((i + 1) < (uint64_t)argc
                    && strnlen (argv[i + 1], MAX_ARG_LEN) != 0
                    && argv[i + 1][0] != '-') {
                    if (!parse_str_to_uint64 (argv[i + 1], sRecursionLevel)) {
                        wprintf (L"Invalid value for recursion depth \"%hs\"\nPlease provide a positive whole number\n", argv[i + 1]);
                        return -1;
                    }
                    ++i;
                }
            }
#if defined (_WIN32) || defined (_WIN64)
#else
            else if (strncmp (argv[i], "-p", 2) == 0) {
                set_option (SHOW_PERMISSIONS);
            }
#endif
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

            else if (strncmp (argv[i], "-d", 2) == 0) {
                set_option (SHOW_DIR_SIZE);
            }
            else if (strncmp (argv[i], "-a", 2) == 0) {
                set_option (SHOW_ABSNOINDENT);
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
                    wprintf (L"No Search pattern provided after \"%hs\" flag\n", argv[i]);
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
            else if (strncmp (argv[i], "-h", 2) == 0) {
                set_option (HELP);
            }
            else {
                wprintf (L"Ignoring Unknown Option \"%hs\"\n", argv[i]);
            }
            break;

        case 5:

            if (strncmp (argv[i], "--abs", 5) == 0) {
                set_option (SHOW_ABSNOINDENT);
            }
            else {
                wprintf (L"Ignoring Unknown Option \"%hs\"\n", argv[i]);
            }
            break;

        case 6:
            if (strncmp (argv[i], "--help", 6) == 0) {
                set_option (HELP);
            }
            else {
                wprintf (L"Ignoring Unknown Option \"%hs\"\n", argv[i]);
            }
            break;

        case 7:
            if (strncmp (argv[i], "--files", 7) == 0) {
                set_option (SHOW_FILES);
            }
            else {
                wprintf (L"Ignoring Unknown Option \"%hs\"\n", argv[i]);
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
                wprintf (L"Ignoring Unknown Option \"%hs\"\n", argv[i]);
            }
            break;

        case 9:
            if (strncmp (argv[i], "--special", 9) == 0) {
                set_option (SHOW_SPECIAL);
            }
            else {
                wprintf (L"Ignoring Unknown Option \"%hs\"\n", argv[i]);
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
            else if (strncmp (argv[i], "--dir-size", 10) == 0) {
                set_option (SHOW_DIR_SIZE);
            }
            else if (strncmp (argv[i], "--show-err", 10) == 0) {
                set_option (SHOW_ERRORS);
            }
            else {
                wprintf (L"Ignoring Unknown Option \"%hs\"\n", argv[i]);
            }
            break;

        case 11:
            if (strncmp (argv[i], "--recursive", 11) == 0) {
                set_option (SHOW_RECURSIVE);

                // if the user has provided the number of levels, then parse it into a string
                if ((i + 1) < (uint64_t)argc && strnlen (argv[i + 1], MAX_ARG_LEN) != 0 && argv[i + 1][0] != '-') {
                    if (!parse_str_to_uint64 (argv[i + 1], sRecursionLevel)) {
                        wprintf (L"Invalid value for recursion depth \"%hs\"\nPlease provide a positive whole number\n", argv[i + 1]);
                        return -1;
                    }
                    ++i;
                }
            }
            else {
                wprintf (L"Ignoring Unknown Option \"%hs\"\n", argv[i]);
            }
            break;
#if defined (_WIN32) || defined (_WIN64)
#else
        case 13:
            if (strncmp (argv[i], "--permissions", 13) == 0) {
                set_option (SHOW_PERMISSIONS);
            }
            else {
                wprintf (L"Ignoring Unknown Option \"%hs\"\n", argv[i]);
            }
            break;
#endif
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
                    wprintf (L"No Search pattern provided after \"%hs\" flag\n", argv[i]);
                    wprintf (L"Terminating...\n");
                    std::exit (-1);
                }

                // set the option and skip the next argument (that is the search pattern)
                set_option (SEARCH_NOEXT);
                searchPattern = argv[++i];
            }
            else {
                wprintf (L"Ignoring Unknown Option \"%hs\"\n", argv[i]);
            }
            break;
        case 19:
            if (strncmp (argv[i], "--modification-time", 19) == 0) {
                set_option (SHOW_LASTTIME);
            }
            else {
                wprintf (L"Ignoring Unknown Option \"%hs\"\n", argv[i]);
            }
            break;

        default:
            wprintf (L"Ignoring Unknown Option \"%hs\"\n", argv[i]);
        }
    }

    // print the usage instructions and terminate
    if (get_option (HELP)) {
        wprintf (usage, argv[0], argv[0]);
        return 0;
    }

    // convert the provided path to a wide string (if no path was provided, use the relative path to the working directory '.')
    // it is very important to not use L"." directory, since sInitPath is freed at the end of the program
    // using a string literal would cause wrong behaviour (possibly crash the program)
    // sInitPath           = widen_string ((initPathStr == nullptr) ? (".") : (initPathStr));
    sInitPath           = (initPathStr == nullptr) ? widen_string (".") : widen_string (initPathStr);

    // if a search pattern was provided, convert it to a wide string and use the search function
    if (searchPattern != nullptr) {
        sSearchPattern  = widen_string (searchPattern);

#if defined (USE_KMP_SEARCH)
        if (get_option (SEARCH_CONTAINS)) {
            sSearchPatternLen   = strnlen (searchPattern, MAX_ARG_LEN);
            compute_lps ();
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
