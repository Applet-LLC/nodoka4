// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

/*++

Module Name:

    public2.h

Abstract:

    nodokad2 (kbfiltr 型 KMDF フィルタ) とユーザーモードアプリで共有する
    IOCTL コードとデータ構造の定義。inverted call ベースの通信プロトコル。

    現行 nodokad の Public.h / ioctl.h とは別物 (プロトコル V2)。
    KEYBOARD_INPUT_DATA に依存しないよう、キーイベントは NODOKA2_EVENT で表現する
    (ユーザーモード側が ntddkbd.h を include しなくてよい)。

Environment:

    user and kernel

--*/

#ifndef _NODOKA2_PUBLIC2_H
#define _NODOKA2_PUBLIC2_H

//
// コントロールデバイス名 (現行 detour の NodokaWalk とは非衝突の別名)
//
#define NODOKA2_DEVICE_NAME_W   L"\\Device\\Nodoka2Ctl"
#define NODOKA2_SYMLINK_NAME_W  L"\\DosDevices\\Nodoka2Ctl"
#define NODOKA2_WIN32_NAME_W    L"\\\\.\\Nodoka2Ctl"

//
// プロトコルバージョン (上位16bit=major, 下位16bit=minor)
// アプリは IOCTL_NODOKA2_GET_VERSION で取得し feature flag 判定に使う。
//
#define NODOKA2_PROTOCOL_VERSION 0x00020000

//
// IOCTL 群 (FILE_DEVICE_KEYBOARD, function 0x900 台で現行 0x800 台と非衝突)
//
#define IOCTL_NODOKA2_GET_EVENTS \
    CTL_CODE(FILE_DEVICE_KEYBOARD, 0x901, METHOD_BUFFERED, FILE_READ_DATA)

#define IOCTL_NODOKA2_INJECT \
    CTL_CODE(FILE_DEVICE_KEYBOARD, 0x902, METHOD_BUFFERED, FILE_WRITE_DATA)

#define IOCTL_NODOKA2_SET_MODE \
    CTL_CODE(FILE_DEVICE_KEYBOARD, 0x903, METHOD_BUFFERED, FILE_WRITE_DATA)

#define IOCTL_NODOKA2_ENUM_DEVICES \
    CTL_CODE(FILE_DEVICE_KEYBOARD, 0x904, METHOD_BUFFERED, FILE_READ_DATA)

#define IOCTL_NODOKA2_GET_VERSION \
    CTL_CODE(FILE_DEVICE_KEYBOARD, 0x905, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_NODOKA2_REFRESH_LICENSE \
    CTL_CODE(FILE_DEVICE_KEYBOARD, 0x906, METHOD_BUFFERED, FILE_WRITE_DATA)

//
// SET_MODE の値
//
#define NODOKA2_MODE_PASSTHROUGH 0   // 素通し (既定)
#define NODOKA2_MODE_INTERCEPT   1   // intercept (全イベントを飲み込みアプリへ)

//
// 1 キーイベント。KEYBOARD_INPUT_DATA の内容 + 由来キーボードの DeviceId。
//
#pragma pack(push, 4)
typedef struct _NODOKA2_EVENT {
    unsigned long  DeviceId;         // 由来キーボードの nodokad2 DeviceId (上位16bit有効)
    unsigned short UnitId;           // KEYBOARD_INPUT_DATA.UnitId
    unsigned short MakeCode;         // KEYBOARD_INPUT_DATA.MakeCode
    unsigned short Flags;            // KEYBOARD_INPUT_DATA.Flags
    unsigned short Reserved;         // KEYBOARD_INPUT_DATA.Reserved
    unsigned long  ExtraInformation; // KEYBOARD_INPUT_DATA.ExtraInformation
} NODOKA2_EVENT, *PNODOKA2_EVENT;

//
// IOCTL_NODOKA2_GET_EVENTS 出力バッファ。
// アプリは十分大きい Events[] を確保して pending。Count 件が返る。
//
typedef struct _NODOKA2_EVENTS_OUTPUT {
    unsigned long Count;
    NODOKA2_EVENT Events[1];
} NODOKA2_EVENTS_OUTPUT, *PNODOKA2_EVENTS_OUTPUT;

//
// IOCTL_NODOKA2_INJECT 入力バッファ。
// TargetDeviceId のキーボードインスタンスの上位コールバック経由で注入する。
// TargetDeviceId==0 は「任意の生存インスタンス」を意味する。
//
typedef struct _NODOKA2_INJECT_INPUT {
    unsigned long TargetDeviceId;
    unsigned long Count;
    NODOKA2_EVENT Events[1];
} NODOKA2_INJECT_INPUT, *PNODOKA2_INJECT_INPUT;

//
// IOCTL_NODOKA2_ENUM_DEVICES 出力の 1 エントリ。
//
#define NODOKA2_HWID_MAX 128
typedef struct _NODOKA2_DEVICE_INFO {
    unsigned long DeviceId;
    wchar_t       HardwareId[NODOKA2_HWID_MAX];
} NODOKA2_DEVICE_INFO, *PNODOKA2_DEVICE_INFO;

typedef struct _NODOKA2_ENUM_OUTPUT {
    unsigned long       Count;
    NODOKA2_DEVICE_INFO Devices[1];
} NODOKA2_ENUM_OUTPUT, *PNODOKA2_ENUM_OUTPUT;
#pragma pack(pop)

//
// アプリ検出用インターフェース GUID (現行 nodokad とは別 GUID)
//
// {5b8f4c2a-1e6d-4a93-9c77-3f0b2d8e14aa}
//
#define NODOKA2_INTERFACE_GUID_STR "5b8f4c2a-1e6d-4a93-9c77-3f0b2d8e14aa"

#endif // _NODOKA2_PUBLIC2_H
