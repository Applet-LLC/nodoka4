/*
  addid/common/devprop.h
  非破壊的な DEVPKEY 宣言ヘッダ
  - 通常はプラットフォーム SDK の型を利用する。
  - devprop.c など（実体を定義する翻訳単位）では
      #define ADDID_DEFINE_DEVPROPKEY
    を行ってからこのヘッダを include することで、完全な型定義を受け取り実体を置けます。
  - 他の翻訳単位は型の前方宣言のみを受け取り、SDK との衝突を回避します。
*/

#ifndef ADDID_COMMON_DEVPROP_H
#define ADDID_COMMON_DEVPROP_H

/* DeviceProperty macro の不足を補う（既存ソース互換） */
#ifndef DevicePropertyHardwareId
#define DevicePropertyHardwareId 1
#endif
#ifndef DevicePropertyCompatibleIds
#define DevicePropertyCompatibleIds 2
#endif
#ifndef DevicePropertyDeviceInstanceId
#define DevicePropertyDeviceInstanceId 12
#endif
#ifndef DevicePropertyLocationInformation
#define DevicePropertyLocationInformation 10
#endif
#ifndef DevicePropertyBusNumber
#define DevicePropertyBusNumber 21
#endif
#ifndef DevicePropertyAddress
#define DevicePropertyAddress 28
#endif
#ifndef DevicePropertyContainerId
#define DevicePropertyContainerId 29
#endif
#ifndef DevicePropertyPhysicalDeviceLocation
#define DevicePropertyPhysicalDeviceLocation 33
#endif

/* If the build explicitly requests the full DEVPROPKEY layout (ADDID_DEFINE_DEVPROPKEY),
   provide the struct definition here. Otherwise provide only a forward declaration to
   avoid conflicts with platform SDK headers that may define DEVPROPKEY elsewhere.
*/
#if defined(ADDID_DEFINE_DEVPROPKEY)

#if !defined(GUID_DEFINED) && !defined(_GUID_DEFINED) && !defined(_GUID)
typedef struct _GUID {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[8];
} GUID;
#define GUID_DEFINED 1
#endif

typedef unsigned long DEVPROPID;

#if !defined(DEVPROPKEY) && !defined(_DEVPROPKEY)
typedef struct _DEVPROPKEY {
    GUID fmtid;
    DEVPROPID pid;
} DEVPROPKEY;
#define _DEVPROPKEY
#endif

extern const DEVPROPKEY DEVPKEY_Device_InstanceId;
extern const DEVPROPKEY DEVPKEY_Device_ContainerId;

#else /* !ADDID_DEFINE_DEVPROPKEY */

/* Forward declaration only to avoid redefinition when SDK headers are present. */
#ifndef _DEVPROPKEY_FWD
typedef struct _DEVPROPKEY DEVPROPKEY;
#define _DEVPROPKEY_FWD
#endif

extern const DEVPROPKEY DEVPKEY_Device_InstanceId;
extern const DEVPROPKEY DEVPKEY_Device_ContainerId;

#endif /* ADDID_DEFINE_DEVPROPKEY */

#endif /* ADDID_COMMON_DEVPROP_H */