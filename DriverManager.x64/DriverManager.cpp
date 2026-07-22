// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <SetupAPI.h>
#include <newdev.h>
#include <filesystem>
#include <algorithm>
#include <cwctype>
#include <cstdio>  // For swscanf_s (DriverVer parsing)
#include <cwchar>  // For swscanf_s (wide-char declaration)
#include <shlwapi.h> // For registry functions
#include <objbase.h> // For CLSIDFromString (class GUID parsing)

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "newdev.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")

// Function prototypes
// ドライバーをインストールする関数
bool InstallDriver(const std::wstring& driverName, const std::wstring& classGuidString);
// ドライバーをアンインストールする関数（optionalInfPath が空でない場合、そのパスを INF として使用）
bool UninstallDriver(const std::wstring& driverName, const std::wstring& classGuidString, const std::wstring& optionalInfPath);
// UpperFiltersを修正する関数（insertBeforeClassDriver=true で class driver の前に挿入）
bool ModifyUpperFilters(const std::wstring& classGuidString, const std::wstring& driverName, bool add, bool insertBeforeClassDriver = false);
// 指定クラスの現在のデバイスを再起動して UpperFilters 変更を即時反映する関数
bool RestartDevicesOfClass(const std::wstring& classGuidString);
// 実行可能ファイルのパスを取得する関数
std::wstring GetExecutablePath();
// サービスを停止し、SERVICE_STOPPED になるまで待機する関数（install/uninstall共用）
bool stopAndWaitService(const std::wstring& serviceName, DWORD timeoutMs);
// FileRepository内の <driverName>* に一致する全 INF パスを列挙する関数
std::vector<std::wstring> EnumerateDriverStoreInfs(const std::wstring& driverName);
// DriverStoreから対象ドライバの全パッケージを削除する関数
bool RemoveDriverStorePackages(const std::wstring& driverName);
// System32\drivers\ に残る対象ドライバの .sys / .inf を無条件削除する関数
void RemoveLeftoverSystemFiles(const std::wstring& driverName);
// サービスが存在するかを確認する関数
bool ServiceExists(const std::wstring& serviceName);
// UpperFilters内で対象ドライバ名が何個見つかるかを数える関数
int CountUpperFiltersOccurrences(const std::wstring& classGuidString, const std::wstring& driverName);
// UpperFiltersに対象ドライバ名が含まれるかを確認する関数
bool UpperFiltersContains(const std::wstring& classGuidString, const std::wstring& driverName);
// uninstall後にUpperFilters・サービス・DriverStoreの3点が消えたことを検証する関数
bool VerifyUninstalled(const std::wstring& classGuidString, const std::wstring& driverName);
// install完了後にUpperFiltersへの登録がちょうど1個であることを検証する関数
bool VerifyInstalled(const std::wstring& classGuidString, const std::wstring& driverName);
// サービスの現在のImagePathを取得する関数
std::wstring GetServiceImagePath(const std::wstring& serviceName);
// 大文字小文字を無視した部分文字列検索
bool ContainsCaseInsensitive(const std::wstring& haystack, const std::wstring& needle);
// install失敗時、直前にステージングした新パッケージのみを削除するロールバック関数
void RollbackNewPackageOnly(const std::wstring& driverStoreInfPath);
// 今回の操作対象ではない、自分が管理する残りの名前について、UpperFiltersに名前は
// あるがサービスキーが存在しないという不整合を検査し、警告ログのみを出す関数（Phase4対応）
void WarnIfOtherManagedDriversInconsistent(const std::wstring& excludeDriverName);
// INFファイルの[Version]セクションからDriverVerの値を取得する関数
std::wstring GetInfDriverVer(const std::wstring& infPath);
// 2つのDriverVer文字列を比較し、verAがverB以上（新しいか同じ）かを判定する関数
bool IsDriverVerNewerOrEqual(const std::wstring& verA, const std::wstring& verB);
// 複数のDriverStoreパッケージ(INFパス)の中からDriverVerが最も新しいものを選ぶ関数
std::wstring SelectNewestDriverStorePackage(const std::vector<std::wstring>& infPaths);

// 使用法を表示する関数
void ShowUsage() {
    std::wcout << L"Usage: DriverManager.exe [command] [driver_type] [optional_inf_path]" << std::endl;
    std::wcout << L"Commands:" << std::endl;
    std::wcout << L"  install   - Installs the specified driver" << std::endl;
    std::wcout << L"  uninstall - Uninstalls the specified driver" << std::endl;
    std::wcout << L"Driver Types:" << std::endl;
    std::wcout << L"  keyboard" << std::endl;
    std::wcout << L"  mouse" << std::endl;
    std::wcout << L"  nodokad  - keyboard upper filter (requires nodokad.sys and nodokad.inf in exe_dir\\nodokad\\)" << std::endl;
    std::wcout << L"  nodokad2 - keyboard filter v2 (requires nodokad2.sys and nodokad2.inf in exe_dir\\nodokad2\\)" << std::endl;
    std::wcout << L"Optional (uninstall only):" << std::endl;
    std::wcout << L"  [optional_inf_path] - Path to the INF file or folder containing the INF." << std::endl;
    std::wcout << L"                        Use this to uninstall an older driver (e.g. DirID 12) by specifying" << std::endl;
    std::wcout << L"                        the INF used at install time (e.g. ...\\nodokad\\drivers\\x64)." << std::endl;
}

int wmain(int argc, wchar_t* argv[]) {
    // install は 2 引数、uninstall は 2 引数または 3 引数（オプションで INF パス）
    if (argc < 3) {
        ShowUsage();
        return 1;
    }

    std::wstring command = argv[1];
    std::wstring driverType = argv[2];

    // コマンドが適切か確認
    if (command != L"install" && command != L"uninstall") {
        ShowUsage();
        return 1;
    }

    if (command == L"install" && argc != 3) {
        ShowUsage();
        return 1;
    }
    if (command == L"uninstall" && argc != 3 && argc != 4) {
        ShowUsage();
        return 1;
    }

    std::wstring driverName;
    std::wstring classGuidString;

    // ドライバータイプに応じてドライバ名とクラスGUID文字列を設定
    if (driverType == L"keyboard") {
        driverName = L"kbdaddid";
        classGuidString = L"{4D36E96B-E325-11CE-BFC1-08002BE10318}";
    } else if (driverType == L"mouse") {
        driverName = L"mouaddid";
        classGuidString = L"{4D36E96F-E325-11CE-BFC1-08002BE10318}";
    } else if (driverType == L"nodokad") {
        driverName = L"nodokad";
        classGuidString = L"{4D36E96B-E325-11CE-BFC1-08002BE10318}";  // Keyboard class
    } else if (driverType == L"nodokad2") {
        driverName = L"nodokad2";
        classGuidString = L"{4D36E96B-E325-11CE-BFC1-08002BE10318}";  // Keyboard class
    } else {
        ShowUsage();
        return 1;
    }

    std::wstring optionalInfPath;
    if (command == L"uninstall" && argc == 4) {
        optionalInfPath = argv[3];
    }

    // 今回の操作対象以外の、自分が管理する残りの名前について現状を確認し、
    // 不整合（UpperFiltersに名前はあるがサービスが無い）があれば警告ログのみ出す。
    // 対象自身の整合性は InstallDriver/UninstallDriver 内の検証ステップで保証する（Phase4対応）。
    WarnIfOtherManagedDriversInconsistent(driverName);

    bool result = false;
    if (command == L"install") {
        result = InstallDriver(driverName, classGuidString);
    } else {
        result = UninstallDriver(driverName, classGuidString, optionalInfPath);
    }

    if (result) {
        std::wcout << L"Operation completed successfully." << std::endl;
        std::wcout << L"A system restart is required for changes to take full effect." << std::endl;
    } else {
        std::wcout << L"Operation failed." << std::endl;
    }

    return result ? 0 : 1;
}

// 指定サービスを停止し、SERVICE_STOPPED になるまで待機する。
// install/uninstall 双方にあった個別の待機ループをここに一本化する（W5対応）。
// サービスが存在しない場合は「既に停止済み」として true を返す。
bool stopAndWaitService(const std::wstring& serviceName, DWORD timeoutMs) {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) {
        return false;
    }
    SC_HANDLE svc = OpenServiceW(scm, serviceName.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!svc) {
        bool notExist = (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST);
        CloseServiceHandle(scm);
        return notExist;
    }

    SERVICE_STATUS ss = {};
    bool stopped = QueryServiceStatus(svc, &ss) && ss.dwCurrentState == SERVICE_STOPPED;
    if (!stopped) {
        ControlService(svc, SERVICE_CONTROL_STOP, &ss); // 使用中等で失敗してもタイムアウトまで待機を試みる
        const DWORD intervalMs = 500;
        for (DWORD waited = 0; waited < timeoutMs; waited += intervalMs) {
            if (!QueryServiceStatus(svc, &ss)) break;
            if (ss.dwCurrentState == SERVICE_STOPPED) {
                stopped = true;
                break;
            }
            Sleep(intervalMs);
        }
    }

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return stopped;
}

// FileRepository\<driverName>* に一致するサブディレクトリ内の <driverName>.inf を全て列挙する。
// install（最新世代の選択）・uninstall（全世代の削除/検証）の両方から使われる。
std::vector<std::wstring> EnumerateDriverStoreInfs(const std::wstring& driverName) {
    std::vector<std::wstring> result;
    wchar_t windir[MAX_PATH] = {};
    GetWindowsDirectoryW(windir, MAX_PATH);
    std::wstring repoBase = std::wstring(windir) + L"\\System32\\DriverStore\\FileRepository\\";
    std::wstring searchPattern = repoBase + driverName + L"*";

    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return result;
    }
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring candidate = repoBase + fd.cFileName + L"\\" + driverName + L".inf";
        if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) {
            result.push_back(candidate);
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return result;
}

// DriverStore内の対象ドライバの全パッケージ（全世代）を削除する。
// ローカルの <driverName>\<driverName>.inf の有無に依存しないため、
// そのファイルが無い/別途開発中のドライバでも確実にDriverStoreから削除できる（W2対応）。
bool RemoveDriverStorePackages(const std::wstring& driverName) {
    bool allOk = true;
    for (const auto& infPath : EnumerateDriverStoreInfs(driverName)) {
        std::wcout << L"Uninstalling DriverStore package: " << infPath << std::endl;
        BOOL rebootReq = FALSE;
        if (!DiUninstallDriverW(NULL, infPath.c_str(), 0, &rebootReq)) {
            DWORD error = GetLastError();
            if (error != ERROR_NOT_FOUND) {
                std::wcerr << L"DiUninstallDriverW failed for " << infPath << L". Error: " << error << std::endl;
                allOk = false;
            }
        } else if (rebootReq) {
            std::wcout << L"A reboot is required to complete removal of: " << infPath << std::endl;
        }
    }
    return allOk;
}

// System32\drivers\ に残る <driverName>.sys / .inf を無条件削除する。
// uninstallは痕跡をゼロにすることが目的のため、現在の登録状態を問わず削除してよい。
void RemoveLeftoverSystemFiles(const std::wstring& driverName) {
    wchar_t sys32[MAX_PATH] = {};
    GetSystemDirectoryW(sys32, MAX_PATH);
    std::wstring driversDir = std::wstring(sys32) + L"\\drivers\\";

    for (const wchar_t* ext : { L".sys", L".inf" }) {
        std::wstring path = driversDir + driverName + ext;
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) continue;
        if (DeleteFileW(path.c_str())) {
            std::wcout << L"Removed leftover file: " << path << std::endl;
        } else {
            MoveFileExW(path.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
            std::wcout << L"Leftover file scheduled for deletion on next reboot: " << path << std::endl;
        }
    }
}

// サービスキーが存在するかどうかを確認する。
bool ServiceExists(const std::wstring& serviceName) {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return false;
    SC_HANDLE svc = OpenServiceW(scm, serviceName.c_str(), SERVICE_QUERY_STATUS);
    bool exists = (svc != NULL);
    if (svc) CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return exists;
}

// UpperFilters の現在値の中に対象ドライバ名が何個含まれているかを数える（検証用）。
// 正常な状態では0個（未登録）または1個（登録済み）のはずで、2個以上は重複登録の異常を示す。
int CountUpperFiltersOccurrences(const std::wstring& classGuidString, const std::wstring& driverName) {
    std::wstring regPath = L"SYSTEM\\CurrentControlSet\\Control\\Class\\" + classGuidString;
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return 0; // キー自体が無ければ含まれていない
    }

    std::vector<wchar_t> buffer(4096);
    DWORD bufferSize = static_cast<DWORD>(buffer.size() * sizeof(wchar_t));
    DWORD type;
    int count = 0;
    if (RegQueryValueExW(hKey, L"UpperFilters", 0, &type, (LPBYTE)buffer.data(), &bufferSize) == ERROR_SUCCESS) {
        for (const wchar_t* p = buffer.data(); *p; p += wcslen(p) + 1) {
            if (driverName == p) {
                ++count;
            }
        }
    }
    RegCloseKey(hKey);
    return count;
}

// UpperFilters の現在値に対象ドライバ名が含まれているかどうかを確認する（検証用）。
bool UpperFiltersContains(const std::wstring& classGuidString, const std::wstring& driverName) {
    return CountUpperFiltersOccurrences(classGuidString, driverName) > 0;
}

// サービスの現在のImagePathを取得する。取得できない場合は空文字列を返す。
std::wstring GetServiceImagePath(const std::wstring& serviceName) {
    std::wstring imagePath;
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return imagePath;
    SC_HANDLE svc = OpenServiceW(scm, serviceName.c_str(), SERVICE_QUERY_CONFIG);
    if (svc) {
        DWORD bytesNeeded = 0;
        QueryServiceConfigW(svc, NULL, 0, &bytesNeeded);
        if (bytesNeeded > 0) {
            std::vector<BYTE> buffer(bytesNeeded);
            LPQUERY_SERVICE_CONFIGW cfg = reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(buffer.data());
            if (QueryServiceConfigW(svc, cfg, bytesNeeded, &bytesNeeded) && cfg->lpBinaryPathName) {
                imagePath = cfg->lpBinaryPathName;
            }
        }
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
    return imagePath;
}

// 大文字小文字を無視した部分文字列検索（レジストリ/ファイルシステムでパスの大小文字表記が
// 食い違う場合があるため、ImagePathの一致判定はこれを使う）。
bool ContainsCaseInsensitive(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) return true;
    std::wstring h = haystack;
    std::wstring n = needle;
    std::transform(h.begin(), h.end(), h.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    std::transform(n.begin(), n.end(), n.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return h.find(n) != std::wstring::npos;
}

// install失敗時、直前にステージングした新パッケージのみを削除するロールバック。
// 旧世代・既存サービスには触れない（W4対応）。
void RollbackNewPackageOnly(const std::wstring& driverStoreInfPath) {
    std::wcout << L"Rolling back newly staged package: " << driverStoreInfPath << std::endl;
    BOOL rebootReq = FALSE;
    if (!DiUninstallDriverW(NULL, driverStoreInfPath.c_str(), 0, &rebootReq)) {
        DWORD error = GetLastError();
        if (error != ERROR_NOT_FOUND) {
            std::wcerr << L"Rollback DiUninstallDriverW failed. Error: " << error << std::endl;
        }
    }
}

// uninstall完了後にUpperFilters・サービス・DriverStoreの3点を再読込して、
// 実際に痕跡が消えたことを確認する（W8対応）。API戻り値のみで成功と判断しない。
bool VerifyUninstalled(const std::wstring& classGuidString, const std::wstring& driverName) {
    bool ok = true;
    if (UpperFiltersContains(classGuidString, driverName)) {
        std::wcerr << L"Verification failed: " << driverName << L" still present in UpperFilters." << std::endl;
        ok = false;
    }
    if (ServiceExists(driverName)) {
        std::wcerr << L"Verification failed: service key " << driverName << L" still exists." << std::endl;
        ok = false;
    }
    auto remaining = EnumerateDriverStoreInfs(driverName);
    if (!remaining.empty()) {
        std::wcerr << L"Verification failed: " << remaining.size()
                    << L" DriverStore package(s) still present for " << driverName << L"." << std::endl;
        ok = false;
    }
    return ok;
}

// install完了後にUpperFilters・サービス・DriverStoreの3点が揃っていることを確認する
// （W8対応、Phase4で強化）。VerifyUninstalledの正反対（消えたこと vs 揃っていること）に相当する。
// UpperFiltersは「ちょうど1個」であることを確認する。0個（追加漏れ）・2個以上（重複登録）の
// いずれも異常とみなす。
bool VerifyInstalled(const std::wstring& classGuidString, const std::wstring& driverName) {
    bool ok = true;
    int count = CountUpperFiltersOccurrences(classGuidString, driverName);
    if (count != 1) {
        std::wcerr << L"Verification failed: " << driverName << L" appears " << count
                    << L" time(s) in UpperFilters (expected exactly 1)." << std::endl;
        ok = false;
    }
    if (!ServiceExists(driverName)) {
        std::wcerr << L"Verification failed: service key " << driverName << L" does not exist." << std::endl;
        ok = false;
    }
    if (EnumerateDriverStoreInfs(driverName).empty()) {
        std::wcerr << L"Verification failed: no DriverStore package found for " << driverName << L"." << std::endl;
        ok = false;
    }
    return ok;
}

// DriverManagerが管理する3つの名前（nodokad / kbdaddid / mouaddid）のうち、今回の
// 操作対象ではない残りについて、「UpperFiltersに名前はあるがサービスキーが存在しない」
// という不整合を検査し、見つかったら警告ログのみを出す。自動修復はしない（既定は警告のみ）。
// nodokad/kbdaddid はキーボードクラス、mouaddid はマウスクラスに属するため、
// 名前ごとに対応するクラスGUIDを使って確認する。他社ドライバ（ETD等）には一切触れない。
void WarnIfOtherManagedDriversInconsistent(const std::wstring& excludeDriverName) {
    struct ManagedDriver { const wchar_t* name; const wchar_t* classGuid; };
    static const ManagedDriver kManagedDrivers[] = {
        {L"nodokad",  L"{4D36E96B-E325-11CE-BFC1-08002BE10318}"}, // keyboard class
        {L"nodokad2", L"{4D36E96B-E325-11CE-BFC1-08002BE10318}"}, // keyboard class
        {L"kbdaddid", L"{4D36E96B-E325-11CE-BFC1-08002BE10318}"}, // keyboard class
        {L"mouaddid", L"{4D36E96F-E325-11CE-BFC1-08002BE10318}"}, // mouse class
    };

    for (const auto& d : kManagedDrivers) {
        if (excludeDriverName == d.name) continue;
        if (UpperFiltersContains(d.classGuid, d.name) && !ServiceExists(d.name)) {
            std::wcerr << L"Warning: " << d.name << L" is present in UpperFilters but its service "
                          L"does not exist. It may be incompletely installed or uninstalled. "
                          L"No automatic action was taken." << std::endl;
        }
    }
}

// INFファイルの[Version]セクションからDriverVerの値（例: "06/22/2026,20.31.5.308"）を取得する。
// 取得できない場合は空文字列を返す。
std::wstring GetInfDriverVer(const std::wstring& infPath) {
    HINF hInf = SetupOpenInfFileW(infPath.c_str(), NULL, INF_STYLE_WIN4, NULL);
    if (hInf == INVALID_HANDLE_VALUE) {
        return L"";
    }
    wchar_t buffer[256] = {};
    std::wstring result;
    if (SetupGetLineTextW(NULL, hInf, L"Version", L"DriverVer", buffer, ARRAYSIZE(buffer), NULL)) {
        result = buffer;
    }
    SetupCloseInfFile(hInf);
    return result;
}

// DriverVer文字列 "MM/DD/YYYY,a.b.c.d" を比較し、verAがverB以上（同じか新しい）かを判定する。
// DriverStore自体の新旧判定（日付優先、同日ならバージョン番号）を模倣する。
// 解析できない方は最も古いものとして扱う（安全側に倒す）。
bool IsDriverVerNewerOrEqual(const std::wstring& verA, const std::wstring& verB) {
    auto parse = [](const std::wstring& v, unsigned __int64& dateKey, unsigned __int64& versionKey) -> bool {
        size_t comma = v.find(L',');
        std::wstring datePart = (comma == std::wstring::npos) ? v : v.substr(0, comma);
        std::wstring verPart = (comma == std::wstring::npos) ? L"" : v.substr(comma + 1);
        unsigned mm = 0, dd = 0, yyyy = 0;
        if (swscanf_s(datePart.c_str(), L"%u/%u/%u", &mm, &dd, &yyyy) != 3) {
            return false;
        }
        dateKey = (unsigned __int64)yyyy * 10000 + mm * 100 + dd;
        unsigned a = 0, b = 0, c = 0, d = 0;
        swscanf_s(verPart.c_str(), L"%u.%u.%u.%u", &a, &b, &c, &d);
        versionKey = (unsigned __int64)a * 1000000000000ULL + (unsigned __int64)b * 100000000ULL
                   + (unsigned __int64)c * 10000ULL + d;
        return true;
    };

    unsigned __int64 dateA = 0, verNumA = 0, dateB = 0, verNumB = 0;
    bool okA = parse(verA, dateA, verNumA);
    bool okB = parse(verB, dateB, verNumB);
    if (!okA) return false; // 解析できなければ「新しい」とは判定しない
    if (!okB) return true;
    if (dateA != dateB) return dateA > dateB;
    return verNumA >= verNumB;
}

// 複数のDriverStoreパッケージ(INFパス)の中から、DriverVerが最も新しいものを選んで返す。
// リストが空の場合は空文字列を返す。
std::wstring SelectNewestDriverStorePackage(const std::vector<std::wstring>& infPaths) {
    std::wstring best;
    std::wstring bestVer;
    for (const auto& candidate : infPaths) {
        std::wstring ver = GetInfDriverVer(candidate);
        if (best.empty() || IsDriverVerNewerOrEqual(ver, bestVer)) {
            best = candidate;
            bestVer = ver;
        }
    }
    return best;
}

bool InstallDriver(const std::wstring& driverName, const std::wstring& classGuidString) {
    namespace fs = std::filesystem;
    fs::path exePath = GetExecutablePath();
    fs::path infPath = exePath / driverName / (driverName + L".inf");

    if (!fs::exists(infPath)) {
        std::wcerr << L"INF file not found at: " << infPath << std::endl;
        return false;
    }

    std::wcout << L"Installing driver package from: " << infPath << std::endl;

    // アップグレード（既存サービスあり）か新規インストールかを記録する。
    // ModifyUpperFilters(add) 失敗時のロールバック判断に使う（W4対応）。
    // アップグレード中に automatic rollback でサービスを削除すると、
    // 書き込みに失敗してそのまま残った旧UpperFiltersエントリが参照先を失い、
    // 今回の事故と同種の宙ぶらりん状態を新たに作ってしまうため区別が必要。
    bool serviceExistedBefore = ServiceExists(driverName);

    // 既存サービスを停止して SetupInstallServicesFromInfSectionW が安全に動作できる状態にする。
    // Case 2（旧 %12% サービスあり）でも Case 3（既存 %13% サービスあり）でも必要。
    // サービス停止+待機の実装はここに一本化されている（setup.cpp側の同等処理は撤去済み、W5対応）。
    if (!stopAndWaitService(driverName, 5000)) {
        std::wcerr << L"Service did not reach STOPPED state within timeout. Continuing anyway." << std::endl;
    }

    // install開始時点のDriverStore世代を「旧世代」として保持する。
    // 成功時にこれらを削除し、世代蓄積（W3）を防ぐ。
    std::vector<std::wstring> oldGenerations = EnumerateDriverStoreInfs(driverName);

    // 1. DiInstallDriverW でドライバを DriverStore にステージングする
    //    これにより %13% (DIID_DRIVER_STORE) が解決され、nodokad.sys が FileRepository に配置される。
    //    注意: ステージング対象が既存のActiveなパッケージよりDriverVerで古い場合、
    //    DiInstallDriverWはエラーを返さず「成功」するが、DriverStore内部では
    //    supersededとして新フォルダだけ作り、既存のActiveパッケージは変更されない。
    // DIIRFLAG_FORCE_INF: ユーザーが手動でINFを選んだ場合と同様、ドライバランキング比較
    // （署名レベル・DriverVer）を無視してこのINFを強制的にステージングする。
    // Flags=0だとDriverVerが既存Active版と同一（例: EV署名版とWHQL提出版が同じdate/versionを
    // 持つ場合）の際に「既存より優れていない」と判定され、新パッケージが有効化されない
    // （pnputilでの明示アンインストール→再インストールが必要になっていた問題の原因）。
    BOOL rebootRequired = FALSE;
    if (!DiInstallDriverW(NULL, infPath.c_str(), DIIRFLAG_FORCE_INF, &rebootRequired)) {
        std::wcerr << L"DiInstallDriverW failed. Error: " << GetLastError() << std::endl;
        return false;
    }

    // 2. ステージング後のDriverStoreを再列挙し、旧世代に無かった
    //    今回新規に作られたパッケージを特定する。
    //    新規フォルダが見つかっても、既存世代（oldGenerations）の最新よりDriverVerで
    //    新しいことを確認できない限り「今回有効になった新パッケージ」とは見なさない。
    //    確認できない場合はDriverStoreにsupersededされた孤立パッケージなので、
    //    世代蓄積（W3）の原因にしないよう即座にロールバックする。
    //    （このチェックが無いと、supersededされて実際には使われていない新パッケージを
    //    「今回のパッケージ」と誤認し、本来Activeなままの既存パッケージの方を
    //    6.の旧世代削除で消してしまう。これが実機事故の直接原因だった）
    std::vector<std::wstring> postGenerations = EnumerateDriverStoreInfs(driverName);
    std::vector<std::wstring> newCandidates;
    for (const auto& candidate : postGenerations) {
        if (std::find(oldGenerations.begin(), oldGenerations.end(), candidate) == oldGenerations.end()) {
            newCandidates.push_back(candidate);
        }
    }

    std::wstring driverStoreInfPath;
    bool isNewPackage = false;
    if (!newCandidates.empty()) {
        std::wstring bestOldVer = oldGenerations.empty()
            ? L""
            : GetInfDriverVer(SelectNewestDriverStorePackage(oldGenerations));
        for (const auto& candidate : newCandidates) {
            std::wstring candidateVer = GetInfDriverVer(candidate);
            if (oldGenerations.empty() || IsDriverVerNewerOrEqual(candidateVer, bestOldVer)) {
                driverStoreInfPath = candidate;
                isNewPackage = true;
            } else {
                std::wcout << L"Staged package is older than the existing active package; "
                               L"rolling back superseded package: " << candidate << std::endl;
                RollbackNewPackageOnly(candidate);
            }
        }
    }

    if (driverStoreInfPath.empty()) {
        // 新規ステージングが無かった、またはsupersededで上記ロールバック済みの場合は、
        // 現存する世代の中からDriverVerが最も新しいものを「現在使用すべきパッケージ」として選ぶ
        // （同一バージョンの再インストール等で新規フォルダが作られなかった場合も含む。
        // 新規パッケージではないため、以降の失敗ロールバックでは削除しない
        // ＝旧世代・旧サービスは触らない、W4対応）。
        driverStoreInfPath = SelectNewestDriverStorePackage(EnumerateDriverStoreInfs(driverName));
        isNewPackage = false;
    }

    if (driverStoreInfPath.empty()) {
        std::wcerr << L"Staged INF not found in FileRepository for: " << driverName << std::endl;
        return false;
    }
    std::wcout << L"DriverStore INF: " << driverStoreInfPath
                << (isNewPackage ? L" (newly staged)" : L" (existing)") << std::endl;

    // 3. DriverStore INF の [Services] セクションを処理してサービスを作成/更新する。
    //    既存サービスがある場合（Case 2: %12%, Case 3: %13% 更新）は ImagePath 等を上書き更新する。
    //    新規の場合（Case 1）はサービスを新規作成する。
    HINF hInf = SetupOpenInfFileW(driverStoreInfPath.c_str(), NULL, INF_STYLE_WIN4, NULL);
    if (hInf == INVALID_HANDLE_VALUE) {
        std::wcerr << L"SetupOpenInfFileW failed. Error: " << GetLastError() << std::endl;
        if (isNewPackage) RollbackNewPackageOnly(driverStoreInfPath);
        return false;
    }
    std::wstring serviceSection = driverName + L"_Install.";
#ifdef _WIN64
    serviceSection += L"NTamd64.Services";
#else
    serviceSection += L"NTx86.Services";
#endif
    std::wcout << L"Creating service from section: " << serviceSection << std::endl;
    BOOL svcResult = SetupInstallServicesFromInfSectionW(hInf, serviceSection.c_str(), 0);
    DWORD svcErr = GetLastError();
    SetupCloseInfFile(hInf);
    if (!svcResult) {
        std::wcerr << L"SetupInstallServicesFromInfSectionW failed. Error: " << svcErr << std::endl;
        // 直前にステージングした新パッケージのみをロールバック削除する。
        // 旧世代・既存サービスには触れない（W4対応）。この時点ではサービス作成/更新は
        // 行われていないため、既存サービスは影響を受けていない。
        if (isNewPackage) {
            RollbackNewPackageOnly(driverStoreInfPath);
        }
        return false;
    }

    // 4. 旧 System32\drivers\<name>.sys の削除は、サービスのImagePathが
    //    実際に新しいDriverStoreパスへ更新されたことを確認した後にのみ行う（W3対応）。
    //    無条件削除はuninstall専用とし、installでは確認できない限り削除しない。
    //    注意: サービスのImagePathは "\SystemRoot\System32\DriverStore\..." のように
    //    SystemRootトークン形式で記録されるのに対し、newPackageDirはGetWindowsDirectoryW由来の
    //    ドライブレター形式（例: "C:\Windows\System32\DriverStore\..."）になるため、
    //    フルパスのまま比較すると前置部分が一致せず常に不一致になる。
    //    そのためパッケージフォルダ名（ハッシュ名）部分だけを目印として比較する。
    fs::path newPackageDir = fs::path(driverStoreInfPath).parent_path();
    std::wstring packageDirMarker = L"\\DriverStore\\FileRepository\\" + newPackageDir.filename().wstring();
    std::wstring imagePath = GetServiceImagePath(driverName);
    bool imagePathUpdated = !imagePath.empty() && ContainsCaseInsensitive(imagePath, packageDirMarker);
    if (imagePathUpdated) {
        wchar_t sys32[MAX_PATH];
        GetSystemDirectoryW(sys32, MAX_PATH);
        std::wstring oldSys = std::wstring(sys32) + L"\\drivers\\" + driverName + L".sys";
        if (GetFileAttributesW(oldSys.c_str()) != INVALID_FILE_ATTRIBUTES) {
            if (!DeleteFileW(oldSys.c_str())) {
                MoveFileExW(oldSys.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
                std::wcout << L"Old sys scheduled for deletion on next reboot: " << oldSys << std::endl;
            } else {
                std::wcout << L"Old sys deleted: " << oldSys << std::endl;
            }
        }
    } else {
        std::wcout << L"Service ImagePath does not point to the new DriverStore package yet; "
                       L"skipping old System32\\drivers cleanup." << std::endl;
    }

    // 5. UpperFilters に追加する。
    //    CONNECT を横取りする kbfiltr/moufiltr 型 (nodokad2 / kbdaddid / mouaddid) は
    //    class driver の前（下）に、READ 完了を横取りするレガシー nodokad は直後（上）に置く。
    const bool insertBeforeClass =
        (driverName == L"nodokad2" || driverName == L"kbdaddid" || driverName == L"mouaddid");
    std::wcout << L"Adding " << driverName << L" to UpperFilters"
               << (insertBeforeClass ? L" (before class driver)..." : L" (after class driver)...") << std::endl;
    if (!ModifyUpperFilters(classGuidString, driverName, true, insertBeforeClass)) {
        std::wcerr << L"Failed to modify UpperFilters registry." << std::endl;
        if (!serviceExistedBefore) {
            // 新規インストールが最終段で失敗：サービス・新規パッケージとも今回作られた
            // ものだけなので、安全に完全ロールバックできる（W4対応）。
            std::wcerr << L"Fresh install: rolling back service and new package." << std::endl;
            stopAndWaitService(driverName, 5000);
            SC_HANDLE hScm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
            if (hScm) {
                SC_HANDLE hSvc = OpenServiceW(hScm, driverName.c_str(), DELETE);
                if (hSvc) {
                    DeleteService(hSvc);
                    CloseServiceHandle(hSvc);
                }
                CloseServiceHandle(hScm);
            }
            if (isNewPackage) {
                RollbackNewPackageOnly(driverStoreInfPath);
            }
        } else {
            // アップグレード中の失敗：既存のUpperFiltersエントリは同じドライバ名を
            // 参照し続けているため、ここでサービスや新規パッケージを削除すると
            // そのエントリの参照先が消え、今回の事故と同種の宙ぶらりん状態になる。
            // そのため自動削除はせず、警告のみとして手動確認を促す。
            std::wcerr << L"Upgrade in progress: leaving the existing service/package intact to avoid "
                           L"breaking the still-registered UpperFilters entry. Manual review recommended."
                        << std::endl;
        }
        return false;
    }

    // 6. 成功時、保持していた「旧世代」のDriverStoreパッケージを削除する（W3関連の蓄積対応）。
    //    現在使用中のパッケージ（driverStoreInfPath）と同じものは誤って消さないようにする。
    for (const auto& oldInfPath : oldGenerations) {
        if (oldInfPath == driverStoreInfPath) continue;
        std::wcout << L"Removing superseded DriverStore package: " << oldInfPath << std::endl;
        BOOL rebootReq = FALSE;
        if (!DiUninstallDriverW(NULL, oldInfPath.c_str(), 0, &rebootReq)) {
            DWORD error = GetLastError();
            if (error != ERROR_NOT_FOUND) {
                std::wcerr << L"Failed to remove superseded package " << oldInfPath << L". Error: " << error << std::endl;
            }
        }
    }

    // 7. 検証: UpperFilters に対象ドライバ名がちょうど1個だけ存在することを確認する（W8対応）。
    if (!VerifyInstalled(classGuidString, driverName)) {
        return false;
    }

    // 8. デバイスを再起動して UpperFilters 変更を即時反映する（手動再起動を不要にする）。
    //    kbfiltr 型 (nodokad2 等) はこれで IOCTL_INTERNAL_KEYBOARD_CONNECT が流れ直し、
    //    ServiceCallback が新しいスタック順で差し替わる。boot キーボード等は再起動が必要な
    //    場合があるため、失敗しても致命的とはせず案内のみ行う。
    std::wcout << L"Restarting devices to apply filter changes..." << std::endl;
    if (RestartDevicesOfClass(classGuidString)) {
        std::wcout << L"Devices restarted. Filter changes are now active." << std::endl;
    } else {
        std::wcout << L"Some devices could not be restarted automatically; "
                      L"a system reboot may be required for changes to take full effect." << std::endl;
    }

    return true;
}

bool UninstallDriver(const std::wstring& driverName, const std::wstring& classGuidString, const std::wstring& optionalInfPath) {
    namespace fs = std::filesystem;

    // 1. UpperFilters からドライバを削除する（短間隔リトライ）。
    //    これが消えないまま放置されると、UpperFiltersに死んだ参照が残り
    //    キーボード/マウスがCode 19で全滅する事故につながる（今回の事故の直接原因）。
    //    そのためリトライしても失敗した場合は即座にエラーを返し、サービス削除・
    //    DriverStore削除といった後続処理を実行しない（W1対応）。
    std::wcout << L"Removing " << driverName << L" from UpperFilters..." << std::endl;
    bool filtersRemoved = false;
    const int maxRetries = 3;
    for (int attempt = 1; attempt <= maxRetries; ++attempt) {
        if (ModifyUpperFilters(classGuidString, driverName, false)) {
            filtersRemoved = true;
            break;
        }
        std::wcerr << L"ModifyUpperFilters(remove) failed (attempt " << attempt << L"/" << maxRetries << L")." << std::endl;
        if (attempt < maxRetries) {
            Sleep(200);
        }
    }
    if (!filtersRemoved) {
        std::wcerr << L"Failed to remove " << driverName << L" from UpperFilters after " << maxRetries
                    << L" attempts. Aborting uninstall without touching the service or DriverStore." << std::endl;
        return false;
    }

    // 2. サービスを停止・削除する（W5対応: 待機をstopAndWaitServiceに一本化）
    //    UpperFiltersから除去済みのため、停止/削除に失敗しても再起動後は読み込まれない。
    std::wcout << L"Stopping service: " << driverName << std::endl;
    if (!stopAndWaitService(driverName, 5000)) {
        std::wcerr << L"Service did not reach STOPPED state within timeout (will be removed after reboot if still in use)." << std::endl;
    }
    SC_HANDLE hScm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hScm) {
        SC_HANDLE hSvc = OpenServiceW(hScm, driverName.c_str(), DELETE);
        if (hSvc) {
            if (!DeleteService(hSvc)) {
                std::wcerr << L"DeleteService failed. Error: " << GetLastError()
                           << L" (will be removed after reboot)" << std::endl;
            }
            CloseServiceHandle(hSvc);
        }
        CloseServiceHandle(hScm);
    }

    // 3. 旧 DirID 12 形式等、DriverStoreに存在しない古いINFを明示指定された場合は
    //    そちらも個別にアンインストールを試みる（CLIの互換オプションを維持）。
    if (!optionalInfPath.empty()) {
        fs::path optionalPath(optionalInfPath);
        fs::path legacyInfPath = fs::is_directory(optionalPath)
            ? optionalPath / (driverName + L".inf")
            : optionalPath;
        if (fs::exists(legacyInfPath)) {
            std::wcout << L"Uninstalling legacy driver package: " << legacyInfPath.wstring() << std::endl;
            BOOL rebootReq = FALSE;
            if (!DiUninstallDriverW(NULL, legacyInfPath.c_str(), 0, &rebootReq)) {
                DWORD error = GetLastError();
                if (error != ERROR_NOT_FOUND) {
                    std::wcerr << L"DiUninstallDriverW failed for legacy INF. Error: " << error << std::endl;
                }
            }
        } else {
            std::wcout << L"Legacy INF not found at " << legacyInfPath.wstring() << L". Skipping." << std::endl;
        }
    }

    // 4. ドライバパッケージを DriverStore から削除する。
    //    ローカルの <driverName>\<driverName>.inf の存在チェックには依存せず、
    //    FileRepositoryを名前パターンで直接列挙し、見つかった世代を全て削除する（W2対応）。
    RemoveDriverStorePackages(driverName);

    // 5. System32\drivers\ に残る .sys/.inf を無条件削除する
    //    （uninstallは全消去が目的のため、現在使用中かどうかを問わず削除してよい）
    RemoveLeftoverSystemFiles(driverName);

    // 6. 検証: UpperFilters・サービスキー・DriverStoreの3点が全て消えたことを
    //    再読込して確認する。API戻り値のみで成功と判断しない（W8対応）。
    if (!VerifyUninstalled(classGuidString, driverName)) {
        std::wcerr << L"Post-uninstall verification failed: residue still present for " << driverName << L"." << std::endl;
        return false;
    }

    return true;
}

// クラスキー配下の "0000"/"0001"… の形式のインスタンスサブキー名かどうか。
static bool IsClassInstanceSubkeyName(const wchar_t* name, DWORD cchName) {
    if (cchName != 4) {
        return false;
    }
    for (DWORD i = 0; i < 4; ++i) {
        const wchar_t ch = name[i];
        if (ch < L'0' || ch > L'9') {
            return false;
        }
    }
    return name[4] == L'\0';
}

// 挿入位置がドライバの介入方式によって class driver (kbdclass/mouclass) の前後で異なる:
//   - insertBeforeClassDriver=true : class driver の「前」= 下。
//     IOCTL_INTERNAL_*_CONNECT を横取りする kbfiltr/moufiltr 型 (ServiceCallback フック)。
//     class driver が下向きに送る CONNECT を受け取るには class driver より下にいる必要がある。
//     該当: nodokad2 (kbfiltr 型), kbdaddid, mouaddid。
//   - insertBeforeClassDriver=false: class driver の「直後」= 上。
//     READ 完了 IRP を上向きに横取りするレガシー nodokad (IRP 上位フィルタ)。
// UpperFilters (REG_MULTI_SZ) は先頭=スタック下、末尾=上。
static bool ModifyUpperFiltersAtKey(HKEY key, const std::wstring& driverName, bool add, bool insertBeforeClassDriver = false) {
    std::vector<wchar_t> buffer(4096);
    DWORD bufferSize = static_cast<DWORD>(buffer.size() * sizeof(wchar_t));
    DWORD type = 0;
    const LSTATUS queryResult = RegQueryValueExW(
        key, L"UpperFilters", nullptr, &type,
        reinterpret_cast<LPBYTE>(buffer.data()), &bufferSize);

    std::vector<std::wstring> filters;
    if (queryResult == ERROR_SUCCESS && type == REG_MULTI_SZ && bufferSize >= sizeof(wchar_t)) {
        for (const wchar_t* cursor = buffer.data(); *cursor; cursor += wcslen(cursor) + 1) {
            filters.emplace_back(cursor);
        }
    }

    const auto existing = std::find(filters.begin(), filters.end(), driverName);
    if (add) {
        if (existing != filters.end()) {
            filters.erase(existing);
        }
        static const wchar_t* const kClassDrivers[] = { L"kbdclass", L"mouclass" };
        // kbdclass/mouclass の手前 (= スタックの下側) に並ぶドライバ同士の固定順序。
        // 各配列は kClassDrivers の対応するインデックスに紐づく。先頭ほど kbdclass から遠い。
        // nodoka (nodokad2) と nodoka_subscribe (kbdaddid) のどちらを先にインストールしても
        // 最終的にこの順序に揃うよう、挿入位置を kbdclass の直前ではなくこの順序表に従って決める。
        static const std::wstring kKeyboardPreClassOrder[] = { L"kbdaddid", L"nodokad2" };
        static const std::wstring kMousePreClassOrder[] = { L"mouaddid" };
        struct PreClassOrder { const std::wstring* list; size_t count; };
        static const PreClassOrder kPreClassOrders[] = {
            { kKeyboardPreClassOrder, sizeof(kKeyboardPreClassOrder) / sizeof(kKeyboardPreClassOrder[0]) },
            { kMousePreClassOrder, sizeof(kMousePreClassOrder) / sizeof(kMousePreClassOrder[0]) },
        };

        bool classDriverFound = false;
        auto insertPos = filters.end();
        size_t busIndex = 0;
        for (size_t i = 0; i < sizeof(kClassDrivers) / sizeof(kClassDrivers[0]); ++i) {
            auto pos = std::find(filters.begin(), filters.end(), kClassDrivers[i]);
            if (pos != filters.end()) {
                insertPos = insertBeforeClassDriver ? pos : (pos + 1);
                classDriverFound = true;
                busIndex = i;
                break;
            }
        }
        // class driver の前に置きたいのに、このキーに class driver が無い場合はスキップ。
        // インスタンスサブキー (0000, 0001, …) は通常 kbdclass を持たないため、
        // クラスキー側に任せて重複登録を避ける。
        if (insertBeforeClassDriver && !classDriverFound) {
            return true;
        }
        if (insertBeforeClassDriver) {
            // 自分より後ろに来るべきドライバが既に filters 内にあれば、その手前に挿入し直す。
            // これにより「先に入っていた方の直前」ではなく常に kKeyboardPreClassOrder の並びになる。
            const auto& order = kPreClassOrders[busIndex];
            const std::wstring* selfIt = std::find(order.list, order.list + order.count, driverName);
            if (selfIt != order.list + order.count) {
                for (const std::wstring* peerIt = selfIt + 1; peerIt != order.list + order.count; ++peerIt) {
                    auto peerPos = std::find(filters.begin(), insertPos, *peerIt);
                    if (peerPos != insertPos) {
                        insertPos = peerPos;
                        break;
                    }
                }
            }
        }
        filters.insert(insertPos, driverName);
    } else if (existing != filters.end()) {
        filters.erase(existing);
    }

    if (!add && filters.empty()) {
        const LSTATUS del = RegDeleteValueW(key, L"UpperFilters");
        return del == ERROR_SUCCESS || del == ERROR_FILE_NOT_FOUND;
    }

    std::vector<wchar_t> newBuffer;
    for (const auto& filter : filters) {
        newBuffer.insert(newBuffer.end(), filter.begin(), filter.end());
        newBuffer.push_back(L'\0');
    }
    newBuffer.push_back(L'\0');

    const LSTATUS writeStatus = RegSetValueExW(
        key, L"UpperFilters", 0, REG_MULTI_SZ,
        reinterpret_cast<const BYTE*>(newBuffer.data()),
        static_cast<DWORD>(newBuffer.size() * sizeof(wchar_t)));
    return writeStatus == ERROR_SUCCESS;
}

// クラスキー本体と各インスタンスサブキー (0000, 0001, …) の両方の UpperFilters を更新する。
// インスタンスキーの UpperFilters はクラスキーを上書きするため両方を揃える必要がある。
bool ModifyUpperFilters(const std::wstring& classGuidString, const std::wstring& driverName, bool add, bool insertBeforeClassDriver) {
    const std::wstring regPath = L"SYSTEM\\CurrentControlSet\\Control\\Class\\" + classGuidString;
    HKEY classKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath.c_str(), 0,
                      KEY_READ | KEY_WRITE | KEY_ENUMERATE_SUB_KEYS, &classKey) != ERROR_SUCCESS) {
        return false;
    }

    bool anyInstanceSuccess = false;

    for (DWORD index = 0;; ++index) {
        wchar_t subName[256] = {};
        DWORD cch = 255;
        const LONG enumResult =
            RegEnumKeyExW(classKey, index, subName, &cch, nullptr, nullptr, nullptr, nullptr);
        if (enumResult == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (enumResult != ERROR_SUCCESS || !IsClassInstanceSubkeyName(subName, cch)) {
            continue;
        }

        HKEY instanceKey = nullptr;
        if (RegOpenKeyExW(classKey, subName, 0, KEY_READ | KEY_WRITE, &instanceKey) != ERROR_SUCCESS) {
            continue;
        }
        if (ModifyUpperFiltersAtKey(instanceKey, driverName, add, insertBeforeClassDriver)) {
            anyInstanceSuccess = true;
        }
        RegCloseKey(instanceKey);
    }

    // クラスキー本体も必ず更新する（新規デバイスはこれを継承する）。
    const bool parentSuccess = ModifyUpperFiltersAtKey(classKey, driverName, add, insertBeforeClassDriver);

    RegCloseKey(classKey);
    return anyInstanceSuccess || parentSuccess;
}

// 指定クラスの現在存在するデバイスを再起動して UpperFilters の変更を即時反映する。
// kbfiltr 型 (nodokad2 等) は再起動で IOCTL_INTERNAL_KEYBOARD_CONNECT が流れ直し、
// ServiceCallback が新しいスタック順で差し替わる。管理者権限が必要（本 exe は昇格実行）。
// 戻り値: 1 台以上再起動できたら true。boot キーボード等で失敗した場合は要再起動。
bool RestartDevicesOfClass(const std::wstring& classGuidString) {
    GUID classGuid;
    if (CLSIDFromString(classGuidString.c_str(), &classGuid) != NOERROR) {
        return false;
    }

    HDEVINFO devInfo = SetupDiGetClassDevsW(&classGuid, nullptr, nullptr, DIGCF_PRESENT);
    if (devInfo == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool anyRestarted = false;
    SP_DEVINFO_DATA did = {};
    did.cbSize = sizeof(did);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &did); ++i) {
        SP_PROPCHANGE_PARAMS pcp = {};
        pcp.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
        pcp.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
        pcp.StateChange = DICS_PROPCHANGE; // stop + start = スタック再構築
        pcp.Scope = DICS_FLAG_CONFIGSPECIFIC;
        pcp.HwProfile = 0;

        if (SetupDiSetClassInstallParamsW(devInfo, &did, &pcp.ClassInstallHeader, sizeof(pcp)) &&
            SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, devInfo, &did)) {
            anyRestarted = true;
        } else {
            std::wcerr << L"RestartDevicesOfClass: restart failed for a device, Error: "
                       << GetLastError() << std::endl;
        }
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return anyRestarted;
}

std::wstring GetExecutablePath() {
    wchar_t path[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}
