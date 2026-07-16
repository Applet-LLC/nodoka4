// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

/*++

Module Name:

    devid.c

Abstract:

    DeviceId 決定ロジック。common/ の Try 連鎖と Fnv1aHash16Upper を用いて、
    kbdaddid と互換の DeviceId (上位16bit) を生成する。刻印の有無に関わらず
    常時コンパイルされる基盤機能 (ENUM_DEVICES / イベントへの deviceId 付与)。

    互換性の生命線: ハッシュアルゴリズムと入力文字列の正規化ルールを
    kbdaddid から 1 ビットも変えない。プライマリ経路 (DeviceInstanceId ->
    Fnv1aHash16Upper) は kbdaddid AddDevice と同一。

    注: common/fallback.c の TryQueryIdAsync は WDM DeviceExtension の固定
    オフセットに書き込むため KMDF では使用しない。同期版 Try 群のみ使う。

Environment:

    Kernel mode only. PASSIVE_LEVEL (EvtDeviceAdd)。

--*/

#include "nodokad2.h"

#include "common\\devprop.h"
#include "common\\hash.h"
#include "common\\fallback.h"
#include "common\\regutil.h"

#ifndef DevicePropertyHardwareId
#define DevicePropertyHardwareId 1
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Nodoka2ComputeDeviceId)
#endif

_IRQL_requires_max_(PASSIVE_LEVEL)
ULONG
Nodoka2ComputeDeviceId(
    _In_ PDEVICE_OBJECT Pdo,
    _In_ ULONG UniqNum
    )
{
    ULONG    deviceId = 0;
    ULONG    mode = 0;
    NTSTATUS status;

    PAGED_CODE();

    if (Pdo == NULL) {
        // 最終保険: 固定値。
        UseFixedValue(NULL, &deviceId, UniqNum);
        return deviceId;
    }

    // DeviceIdMode (0=InstanceId 優先, 1=HardwareId) を新サービスキーから読む。
    RegReadDword(L"nodokad2", L"DeviceIdMode", &mode, 0);

    //
    // プライマリ経路: DeviceInstanceId をハッシュ (kbdaddid と同一)。
    //
    if (mode == 0) {
        WCHAR       instanceIdBuf[512] = {0};
        ULONG       instanceIdLen = sizeof(instanceIdBuf);
        DEVPROPTYPE propType = 0;

        status = IoGetDevicePropertyData(
            Pdo, &DEVPKEY_Device_InstanceId, 0, 0,
            instanceIdLen, instanceIdBuf, &instanceIdLen, &propType);

        if (NT_SUCCESS(status) &&
            instanceIdLen >= sizeof(WCHAR) &&
            propType == DEVPROP_TYPE_STRING &&
            instanceIdBuf[instanceIdLen / sizeof(WCHAR) - 1] == L'\0' &&
            instanceIdBuf[0] != L'\0') {

            deviceId = Fnv1aHash16Upper(instanceIdBuf);
            N2_TRACE("DeviceId=0x%08X (InstanceId=%ws)\n", deviceId, instanceIdBuf);
            return deviceId;
        }
    }

    //
    // HardwareId モード。
    //
    if (mode == 1) {
        WCHAR hwidBuf[512] = {0};
        ULONG hwidLen = sizeof(hwidBuf);
        status = IoGetDeviceProperty(Pdo, DevicePropertyHardwareId, hwidLen, hwidBuf, &hwidLen);
        if (NT_SUCCESS(status) && hwidBuf[0] != L'\0') {
            deviceId = Fnv1aHash16Upper(hwidBuf);
            N2_TRACE("DeviceId=0x%08X (HardwareId=%ws)\n", deviceId, hwidBuf);
            return deviceId;
        }
    }

    //
    // フォールバック連鎖 (common/fallback.c、同期版のみ)。
    //
    if (TryContainerId(Pdo, &deviceId, UniqNum))       return deviceId;
    if (TryQueryId(Pdo, &deviceId, UniqNum))           return deviceId;
    if (TryIoGetDeviceProperty(Pdo, &deviceId, UniqNum)) return deviceId;
    if (TryCompatibleIds(Pdo, &deviceId, UniqNum))     return deviceId;
    if (TryHardwareIdWithUniq(Pdo, &deviceId, UniqNum)) return deviceId;

    UseFixedValue(Pdo, &deviceId, UniqNum);
    N2_TRACE("DeviceId=0x%08X (fixed fallback)\n", deviceId);
    return deviceId;
}
