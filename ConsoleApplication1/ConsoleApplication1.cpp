#include <windows.h>
#include <iostream>
#include <string>
#include <filesystem>
#include <thread>
#include <chrono>

void MovePDFFile(const std::wstring& sourcePath, const std::wstring& destinationFolder) {
    std::wstring fileName = sourcePath.substr(sourcePath.find_last_of(L"\\") + 1);
    std::wstring destinationPath = destinationFolder + L"\\" + fileName;
    const int maxRetries = 5;
    const int retryDelay = 2000; // milliseconds

    int attempt = 0;
    BOOL moveResult;

    do {
        moveResult = MoveFileW(sourcePath.c_str(), destinationPath.c_str());
        if (!moveResult) {
            DWORD error = GetLastError();
            if (error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION) {
                std::wcerr << L"File is in use. Retrying in " << retryDelay << L" milliseconds..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(retryDelay));
                attempt++;
            }
            else {
                std::wcerr << L"Failed to move " << fileName << L". Error: " << error << std::endl;
                break;
            }
        }
    } while (!moveResult && attempt < maxRetries);

    if (moveResult) {
        std::wcout << L"Moved " << fileName << L" to " << destinationFolder << std::endl;
    }
    else {
        std::wcerr << L"Failed to move " << fileName << L" after " << maxRetries << L" attempts." << std::endl;
    }
}

void WatchDirectory(const std::wstring& sourceFolder, const std::wstring& destinationFolder) {
    HANDLE hDir = CreateFileW(
        sourceFolder.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );

    if (hDir == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to open directory. Error: " << GetLastError() << std::endl;
        return;
    }

    char buffer[1024];
    DWORD bytesReturned;
    FILE_NOTIFY_INFORMATION* pNotify;
    std::wstring fileName;

    while (true) {
        if (ReadDirectoryChangesW(
            hDir,
            buffer,
            sizeof(buffer),
            FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME,
            &bytesReturned,
            NULL,
            NULL
        )) {
            pNotify = (FILE_NOTIFY_INFORMATION*)buffer;

            do {
                fileName = std::wstring(pNotify->FileName, pNotify->FileNameLength / sizeof(WCHAR));
                if (fileName.substr(fileName.find_last_of(L".") + 1) == L"pdf") {
                    MovePDFFile(sourceFolder + L"\\" + fileName, destinationFolder);
                }

                pNotify = pNotify->NextEntryOffset
                    ? (FILE_NOTIFY_INFORMATION*)((LPBYTE)pNotify + pNotify->NextEntryOffset)
                    : NULL;
            } while (pNotify);
        }
        else {
            std::wcerr << L"ReadDirectoryChangesW failed. Error: " << GetLastError() << std::endl;
            break;
        }
    }

    CloseHandle(hDir);
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc != 3) {
        std::wcerr << L"Usage: " << argv[0] << L" <source_folder> <destination_folder>" << std::endl;
        return 1;
    }

    std::wstring sourceFolder = argv[1];
    std::wstring destinationFolder = argv[2];

    if (!std::filesystem::exists(sourceFolder)) {
        std::wcerr << L"Source folder does not exist: " << sourceFolder << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(destinationFolder)) {
        std::wcerr << L"Destination folder does not exist: " << destinationFolder << std::endl;
        return 1;
    }

    WatchDirectory(sourceFolder, destinationFolder);

    return 0;
}
