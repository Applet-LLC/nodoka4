# About Nodoka, a General-Purpose Keybinding Remapping Software

https://appletllc.com/  appletllc@gmail.com 2026-08-26

## 1. Overview

This is a derivative version of the software “Madousai no Yuutsu” (“Windowmaker’s Melancholy”). Please note that the support and development organization are different.

Using a configuration file named `dot.nodoka`, you can freely remap the keyboard layout to a certain extent.

The supported OS is Windows 10 22H2 or later.

The device driver is WHQL-signed for Windows 10 22H2 on x86 (32-bit) and Windows 11 24H2 on x64 (64-bit).

The executable files, DLLs, and device driver are digitally signed.

The trial version terminates automatically 30 minutes after startup.

This is shareware.

The source code is published under the EPL 2.0 (Eclipse Public License 2.0) at https://github.com/Applet-LLC/nodoka4.

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

Run either `nodoka-4.33_sample_setup.exe` (trial version) or `nodoka-4.33_setup.exe` (full version) with administrator privileges.

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

The user support bulletin board (anonymous posting allowed) is here: https://jbbs.shitaraba.net/computer/41517/.

You can also get support there, but if you want to report a bug directly, please use the following instead. This is faster for confirming and reporting issues right after a release.

https://github.com/Applet-LLC/nodoka4/issues

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

Boost C++ Libraries 1.85.0

This product includes software from the Boost C++ Libraries, distributed under the Boost Software License, Version 1.0.
https://www.boost.org/

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

### 2026-08-26 4.33

**Summary**

Fixed a bug in Combo/TapDance where a modifier key (Shift/Ctrl, etc.) could remain stuck down and not return. Also fixed a TapDance mis-recognition issue, and cleaned up the documentation around Combo/TapDance/TapHold. In addition, added the `.nodoka` extension to Explorer's "New" menu.

#### (1) Fixed the Combo/TapDance modifier-key stuck-down bug

Combo, TapDance, and TapHold each have timer-driven confirmation processing (confirmation on timeout). This confirmation processing could bypass the modifier-key (Shift/Ctrl/Alt/Win) release handling that runs at the end of the main loop, so when a Combo fired or a TapDance tap was confirmed, a modifier key that should have been released could remain stuck down. This has been fixed.

We also fixed a race condition where, if the next key input arrived at almost the same moment a TapDance timeout was reached, two taps that should have been processed independently could be incorrectly merged into a single double-tap, depending on timing.

Note that for Combo/TapHold, we have confirmed that, right at the judgment boundary (cases where a key operation overlaps almost exactly with a threshold or timeout), Combo success/failure or the tap/hold judgment can occasionally flicker. Since this does not cause harmful behavior such as a stuck modifier key, we have decided not to address it this time.

#### (2) Added the `.nodoka` extension to Explorer's "New" menu

You can now create a "Nodoka file" (`.nodoka`) directly from Explorer's right-click → New menu. This menu entry is also removed on uninstall.

#### (3) Documentation cleanup for Combo/TapDance/TapHold

In the customization help (CUSTOMIZE-ja.html), we made the documentation more accurate: clarifying that a FUNCTION can be specified anywhere a KEY is accepted, noting that TapDance accepts only a single KEY, adding example syntax for TapHold's per-rule options (`permissive_hold=`, `hold_on_other_key=`, `quick_tap_term=`), and replacing samples that used undefined functions with ones that actually work. We also clarified that the Layer concept can be realized using lock keys (L0-LF) or mod0-9 (M0-M9).

#### (4) Added `-t` / `-d` startup arguments (for automated testing)

For use by external automated-testing tools and scripts, we added two startup arguments: `-t <name>` and `-d <path>`.

- `-t <name>`: Switches to, and reloads, the registered configuration (`.nodoka` file) whose name matches `<name>` — the same name used in the "Switch setting" tray-icon menu, etc. If Nodoka is already running, it switches and reloads; if not yet running, it starts up with that configuration selected. If the name is not found, nothing changes.
- `-d <path>`: Also writes the contents of the log window to the specified file (UTF-8, no BOM), as they occur. Specifying this also automatically enables the detail log.

See the "Options: Startup Arguments" section of the customization help for details.

#### (5) Other fixes

1. Fixed a logging gap in the Combo/TapDance/TapHold state-transition handling where key input details were sometimes missing from the detail log.
2. Updated the contact email address shown in the version dialog's copyright notice to appletllc@gmail.com.

