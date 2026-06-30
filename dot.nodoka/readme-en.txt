# About Nodoka, a General-Purpose Keybinding Remapping Software

https://appletllc.com/  applet@bp.iij4u.or.jp 2026-06-27

## 1. Overview

This is a derivative version of the software “Madousai no Yuutsu” (“Windowmaker’s Melancholy”). Please note that the support and development organization are different.

Using a configuration file named `dot.nodoka`, you can freely remap the keyboard layout to a certain extent.

The supported OS is Windows 10 22H2 or later.

The device driver is WHQL-signed for Windows 10 22H2 on x86 (32-bit) and Windows 11 24H2 on x64 (64-bit).

The executable files, DLLs, and device driver are digitally signed.

The trial version terminates automatically 30 minutes after startup.

This is shareware.

The source code is published under the EPL 2.0 (Eclipse Public License 2.0) at https://gitlab.com/appletllc/nodoka4.

The full version, priced at USD 11 including tax, can be purchased from https://appletllc.com/%e3%82%bd%e3%83%95%e3%83%88%e3%82%a6%e3%82%a7%e3%82%a2/.

Payment is handled through Gumroad, and a credit card is required.

## 2. Installation, Launch, Uninstallation, and Customization

There are limitations and known issues, so please see the following pages for details.

- Information page: https://appletllc.com/web/nodoka.htm
- Q&A: https://appletllc.com/web/nodoka-QandA.htm
- Help: https://appletllc.com/web/nodoka-doc/README-ja.html

### (1) Installation

First, exit any running instance of Nodoka. If startup is enabled in Task Manager’s Startup apps, disable it.

To install Nodoka, you first need the Visual C++ v14 Redistributable Package.

From the page below, download and install the x64 or x86 file according to your OS architecture, 64-bit or 32-bit:

- Download the latest supported Visual C++ Redistributable | Microsoft Learn  
  https://learn.microsoft.com/ja-jp/cpp/windows/latest-supported-vc-redist?view=msvc-170  
  https://aka.ms/vc14/vc_redist.x64.exe  
  https://aka.ms/vc14/vc_redist.x86.exe

Then restart Windows. Running the setup while Nodoka is not running helps prevent installation failure.

Run either `nodoka-4.31_sample_setup.exe` (trial version) or `nodoka-4.31_setup.exe` (full version) with administrator privileges.

The device driver is installed during setup. A confirmation dialog may appear if necessary.

After the files are installed, you will be prompted to restart, so please restart.

After rebooting, re-enable the Startup setting in Task Manager if you disabled it earlier.

### (2) Running Nodoka

Double-click “Nodoka” registered in the Start menu. If you register it in Startup, it will run automatically when you log in to Windows.

In the trial version, it automatically exits 30 minutes after startup.

### (3) Uninstallation

Find “Nodoka” in Control Panel’s Add/Remove Programs and uninstall it.

Even after uninstalling, files may remain in `c:\Program Files\nodoka` because running files cannot be deleted, and shortcuts may remain in the Start menu or Startup.

In that case, delete them manually from Explorer.

The device driver is removed at the same time.

### (4) Usage and Customization

Please refer to the help file mentioned above for usage and customization.

There are also several configuration examples on the English page:

https://www.appletllc.com/web/en/sample.htm

## 3. Limitations and User Support

Please refer to the help file mentioned in section 2.

We make efforts to ensure stable operation, but if a malfunction occurs, whether in the trial version or the full version, the responsibility lies with the user. Thank you for your understanding.

The user support bulletin board is here: https://jbbs.shitaraba.net/computer/41517/.

You can use it to report bugs, request investigations, and exchange information with other users.

## 4. Copyright Notice

For Mado Version 4.00.0 and Nodoka Version 4.01 through 4.30:

Copyright (C) 2008–2026 applet <applet@bp.iij4u.or.jp> All rights reserved.

The license is CPL (Common Public License). For details, see `Common_Public_License_1_0.txt`. `Common_Public_License_1_0_JP.txt` is the Japanese reference translation.
Note: these files were bundled with past versions (4.01 through 4.30).

For Nodoka Version 4.31 or later:

Copyright (C) 2008–2026 applet <applet@bp.iij4u.or.jp> All rights reserved.

The license has been changed to EPL 2.0 (Eclipse Public License 2.0). For details, see `LICENSE.txt`. `LICENSE_JP.txt` is the Japanese reference translation.

For Madousai no Yuutsu Version 3.30 and earlier:

Copyright (C) 1999–2005, TAGA Nayuta <nayuta@users.sourceforge.net>  
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions, and the following disclaimer.  
Redistributions in binary form must reproduce the above copyright notice, this list of conditions, and the following disclaimer in the documentation and/or other materials provided with the distribution.  
The name of the author may not be used to endorse or promote products derived from this software without specific prior written permission.  
THIS SOFTWARE IS PROVIDED BY THE AUTHOR “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Yet Another Mado tsukai no Yuutsu (YAMY)

Copyright (C) 2009, KOBAYASHI Yoshiaki <gimy@users.sourceforge.jp>  
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions, and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions, and the following disclaimer in the documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Boost.Regex  
Copyright (c) 1998–2007 John Maddock

Boost.Program_options  
Copyright (c) 2002–2004 Vladimir Prus

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization obtaining a copy of the software and accompanying documentation covered by this license (the “Software”) to use, reproduce, display, distribute, execute, and transmit the Software, and to prepare derivative works of the Software, and to permit third-parties to whom the Software is furnished to do so, all subject to the following:

The copyright notices in the Software and this entire statement, including the above license grant, this restriction and the following disclaimer, must be included in all copies of the Software, in whole or in part, and all derivative works of the Software, unless such copies or derivative works are solely in the form of machine-executable object code generated by a source language processor.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Sirius  
http://www.sirius.spline.tv/wiki/WikiStart

The MIT License  
Copyright (C) 2008–2010, MATSUMOTO Reiji <matsumoto@spline.oc.to>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## 5. Acknowledgements

To TAGA Nayuta, who created the great and indispensable “Madousai no Yuutsu,” my sincere thanks.

Also, to the anonymous person who created the 3.30.1 source for Vista and made it public in a 2ch thread, my sincere thanks.

Also, to KOBAYASHI Yoshiaki, who developed YAMY, my sincere thanks.

## 6. Release Notes

### 2026-06-27 4.31

The license has been changed from CPL (Common Public License) to EPL 2.0 (Eclipse Public License 2.0).

#### (1) Changes to supported OSes and notes about the next version

Previously, Windows 2000, XP, Vista, 7, 8, and 10 were supported. However, this version supports Windows 10 22H2 or later only.

It can also run on Windows 10 version 1903 (May 2019 Update), but on older versions than that, installation may result in an abnormal state such as inability to input keystrokes.

Windows Server is not supported. Judging from the kernel version, installation should be possible on Windows Server 2022 or later. On Windows Server 2019 or earlier, issues will occur.

For Windows 10 32-bit (x86), this 4.31 release will be the last; the next version is planned to support Windows 11 only.

The next version is also planned to be a major upgrade, and the following features are envisioned:

- Implementation of Combo / TapDance / TapHold.
- Full support for multiple keyboards.
- A trial GUI-based configuration tool.

These plans are not finalized, so implementation is not guaranteed, but Combo / TapDance / TapHold have already been experimentally implemented and can be tried in this version. See the end of this Readme.

#### (2) Bug fixes and specification changes

1. Windows 11 support for the device driver and a fix for the inability to input keys during Windows Update.
2. An improvement for the problem where modifier keys remain pressed.
3. Fixes to the LL Hook implementation.
4. Fixes for cooperation issues with applications running with administrator privileges.
5. Improved coordinate accuracy in window move/maximize functions.
6. Fixed a configuration file loading bug.
7. Changes to the development environment.
8. Remote Desktop support.
9. Experimental implementation for the next version.

1) Windows 11 support for the device driver and a fix for the inability to input keys during Windows Update

When Windows is updated or upgraded, the code has been redesigned as a primitive driver for Windows 10 version 1903 or later so that the driver registration state is handled correctly.

The version number has been changed to 1.38 (previously 1.33). Versions 1.34, 1.35, 1.36, and 1.37 were skipped.

Installation was changed back from INF-based installation to setup-based installation, as before.

The x86 (32-bit) version is WHQL-signed on Windows 10 22H2, and the x64 (64-bit) version is WHQL-signed on Windows 11 24H2.

Because it is now a primitive driver, it is compatible with Windows Update. The device driver installation destination has changed from `C:\Windows\System32\drivers` to per-driver folders under `C:\Windows\System32\DriverStore\FileRepository`.

If you install Nodoka 4.31 in an environment that already has 4.30 (nodokad 1.31), the old driver settings are removed and the new driver settings are written to the registry.

So far, we have not encountered cases where Windows Update from Windows 10 20H2 to 21H2 or 22H2 causes key input to stop working while Nodoka is installed and version 4.31 is used.

At least the likelihood has been reduced. However, there are cases where `nodokad.sys` is not present, and in that case the installation may need to be repeated.

If you still encounter a state where you cannot type on the login screen after Windows Update, you can try the following.

If the on-screen keyboard works on the login screen, use that.

If the on-screen keyboard does not work, force shutdown several times during the boot screen just before the login screen to enter recovery mode, then boot using the last known good configuration to recover.

2) Added mitigations for modifier keys remaining pressed

The following two options were added. They are disabled by default. Both coexist with the existing `def option CheckModifier`.

They are intended for cases where Shift or Ctrl frequently remain stuck down, and are worth trying if needed.

- `def option SyncModifierGracePeriod = <ms>`  (example: 500; 0 disables the feature)  

  When performing key remapping, there may be a delay of several tens of milliseconds before the system reflects the key output (a delay in the LL Hook and an asynchronous WriteFile to the device driver). If the next key is pressed during this delay, `GetAsyncKeyState` will return 0, causing legitimate modifier keys to be incorrectly cleared.

To prevent this false positive, the grace period ignores clearing within a specified number of milliseconds after a modifier key is recorded as "pressed," thus preventing false positives. **A value of approximately 500 ms** is recommended. If this does not appear to be occurring, no configuration is necessary.

- `def option ModifierAutoClear = <seconds>`  (example: 5; 0 disables the feature)  

This function measures the time elapsed since pressing a modifier key (Shift, Ctrl, Alt, Win) and, after the specified time has passed, sends an Up signal to the modifier key, automatically clearing it.

It's easier to specify than CheckModifer, but it doesn't offer fine-grained control.

As a side note, it seems that Hyper-V environments employ a workaround of periodically sending Up signals to each modifier key. This can sometimes be observed in the log window.


3) Fixes to the LL Hook implementation

A bug where logs were not displayed when the log dialog had focus was fixed.

In `engine.cpp`, the LL Hook callback handling was changed to use Events rather than Windows Messages. This is the same implementation as YAMY.

As a result, the polling-based key input delay was reduced from 5 ms to none, improving responsiveness.

4) Cooperation with administrator-privilege applications

Even though `uiAccess=true` was written in the manifest, things were not working properly, such as being unable to retrieve window information from administrator-privilege applications, so this was fixed.

However, if you look at the debug output in DebugView, applications using Microsoft Edge WebView2 refuse DLL loading, so window information cannot be obtained.

Also, executables such as `nodoka64.exe` with `uiAccess=true` will show an error dialog and fail to function properly if run outside `Program Files`, so `nodoka64_nua.exe` was prepared. Its manifest sets `uiAccess=false`.

If an application does not allow key remapping or window information retrieval, you may be able to work around it by launching `nodoka64_hil.exe` directly so that it runs with higher privileges.

Alternatively, running `nodoka64_nua.exe` with administrator privileges may also work.

5) Improved coordinate accuracy in window move/maximize functions

In `&WindowMaximize`, `&WindowHMaximize`, `&WindowVMaximize`, `&WindowMoveTo`, `&WindowMonitorTo`, `&WindowResizeTo`, `&WindowResizeToPer`, `&WindowResizeMoveTo`, and `&WindowResizeMoveToPer`, the problem of window coordinate calculations not being performed in physical pixels in DPI environments above 125% was fixed.

It was also corrected to use the accurate window rectangle excluding the invisible border (shadow area) introduced by DWM on Windows 10 and later.

6) Configuration file loading bug fix

A bug where the first 1 or 2 bytes were skipped was fixed.

Support for UTF-8 BOM was also added, so files can now be loaded in ASCII, Shift-JIS, UTF-8 with or without BOM, or UTF-16.

7) Development environment changes

The build environment was migrated from Visual Studio 2005 plus DDK to Visual Studio 2022 plus EWDK.

At the same time, all source code was converted to UTF-8.

64-bit pointer casts that were left over from the 64-bit port were also fixed, such as `DWORD -> DWORD_PTR` and `(HWND) -> (HWND)(ULONG_PTR)`, reducing build warnings.

All executable files, DLL files, and SYS files are EV-signed.

DLLs loaded from `nodoka64.exe` must be digitally signed. This is one of the measures used to mitigate DLL injection vulnerabilities.

Azure Artifact Signing has not been obtained.

8) Remote Desktop support

In a Remote Desktop environment, the software traditionally operates in LL Hook mode. However, if you add `-f` to the startup arguments for `nodoka64.exe`, it will run in device-driver mode.

In Hyper-V environments, `-f` is required if you want to use the device driver.

This feature had already been implemented starting with 4.27. This is because in the RDP functionality of Windows 7 and later, the Remote Desktop Keyboard Device composed of `terminpt.sys` exists in the same way as other keyboards, making it possible to cooperate with `nodokad.sys`. Reference information is available at the following site:

- How to enable a third-party driver to intercept and disable the SAS keyboard sequence in Remote Desktop Protocol (RDP) sessions for Windows 7, Windows Server 2008 R2, Windows 8 and Windows Server 2012  
  http://web.archive.org/web/20150111112645/http://support.microsoft.com/kb/2867446

Note that `nodokad.sys` does not support multiple sessions on Windows Server.

9) Experimental implementation for the next version

The following three proposals are being worked on. Of these, Combo / TapDance / TapHold can be tried in this 4.31 version.

- GUI configuration editor.  
  This is a separate application, so it is not implemented in 4.31.

- Full support for multiple keyboards.  
  Code for multiple-keyboard support using the unpublished `kdbaddid.sys` driver has been implemented, but it will not work in environments without that driver.  
  This function binds external keyboards to K1 through K7 modifiers on a per-device or per-instance basis.  
  Unlike the implementation in 4.20, keys can be distinguished from the very start of key down processing.

- Implementation to verify Combo / TapDance / TapHold details.  
  The documentation is as follows. The reason the maximum number of keys for Combo is six is that it was designed with Braille six-dot input in mind.  
  Also, using many simultaneous keys in Combo may require an N-key rollover keyboard, so it may not always work exactly as configured.

--- Combo

Realize key remapping through simultaneous pressing of 2 to 6 keys.

`combo [modifier-] KEY1 KEY2 [KEY3 … KEY6] [window=<ms>] = ACTION`

# Combo options

`def option ComboWindow = <ms>`  
Default: 50  
Time window for the timeout method.

`def option ComboDetector = timeout | immediate | rollover | strict-order | zero-latency`  
Specify the simultaneous-press detection method.

`def option ComboIdleThreshold = <ms>`  
Default: 0 (disabled)  
Grace period for simultaneous-press detection.

`def option ComboOverlapRatio = <0–100>`  
Default: 0 (disabled)  
Adds overlap ratio to simultaneous-press judgment to improve accuracy.

`def option ComboNestedAlwaysMatch = on | off`  
Default: off  
If the first key of the Combo remains pressed after timeout, continue Combo detection mode.  
This option only works in modes with timers: `timeout`, `strict-order`, and `zero-latency`.

Meaning of `ComboDetector` modes:

- `timeout`: Timeout-based mode (default). Fires if all keys are pressed within `ComboWindow`.
- `immediate`: No timer. Fires immediately the moment all keys are pressed.
- `rollover`: No timer. Allows interruption by keys outside the combo.
- `strict-order`: Requires presses to occur in the defined order.
- `zero-latency`: Outputs `key1` immediately, and if the combo is completed, cancels it with IME backspace correction.  
  Works when IME is on; when IME is off, it behaves like `immediate`.

`&SetComboDetector` function  
This changes Combo-related parameters from the point of execution onward.

`&SetComboDetector(mode, window, overlap, nested, idle)`

Only `mode` is a string; the remaining arguments are numeric only.

`&SetComboDetector(immediate, -1, -1, -1, -1)`  
`&SetComboDetector(rollover, 80, -1, -1, -1)`

Argument | Type | Meaning
---|---|---
mode | mode name | `timeout` / `immediate` / `rollover` / `strict-order` / `zero-latency`
window | ms / -1 | Combo window
overlap | 0–100 / -1 | Overlap ratio threshold
nested | 0 or 1 / -1 | `ComboNestedAlwaysMatch`
idle | ms / -1 | `ComboIdleThreshold`

--- TapDance

Performs different actions for single / double / triple taps within a given time period.

# Global option (outside keymap)

`def option TapDanceTimeout = <ms>`  
Default: 300

# Rule definition (inside keymap)

`tapdance [modifier-] KEY [=]`
- `tap1=ACTION`  Required for one tap.
- `[tap2=ACTION]`  Optional for two taps.
- `[tap3=ACTION]`  Optional for three taps.
- `[timeout=<ms>]`  Timeout specific to this rule; omitted to use the global value.

`tapdance Escape = tap1=Escape  tap2=C-OpenBracket  timeout=200`

--- TapHold

Determines whether a key press is a short press (tap) or a long press (hold) based on the press duration, and performs different actions.

# Global option (outside keymap)

`def option TapHoldThreshold = <ms>`  
Default: 200  
Threshold for tap/hold judgment.

`def option TapHoldInterrupt = tap | hold`  
Default: tap  
If another key is pressed while TapHold is being judged, treat it as the specified tap or hold.

`def option TapHoldPermissiveHold = on | off`  
Default: off  
Hold is confirmed only after the other key’s DOWN→UP completes.

`def option TapHoldOnOtherKeyPress = on | off`  
Default: off  
Hold is confirmed the moment another key goes DOWN.

`def option TapHoldQuickTapTerm = 120`  
0 = disabled, default: 0  
If the same key is pressed again within the specified time after the previous tap, do not hold it pending; treat it as a tap.

# Rule definition (inside keymap)

`taphold [modifier-] KEY [=]`
- `tap=ACTION`
- `hold=ACTION`
- `[threshold=<ms>]`  Threshold specific to this rule; omitted to use the global value.
- `[interrupt=tap|hold]`  Interrupt behavior specific to this rule; omitted to use the global value.

Per-rule `def option` settings can override the global ones.

`taphold A  tap=a     hold=Shift_L              permissive_hold=on`  
`taphold T  tap=t     hold=&Layer(layer_edit)   hold_on_other_key=on`  
`taphold SP tap=Space hold=&Layer(num)          quick_tap_term=120`

`taphold F  tap=F  hold=C-                     # F: short press -> f / long press -> Ctrl`

