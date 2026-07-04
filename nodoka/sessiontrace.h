//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sessiontrace.h
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#ifndef _SESSIONTRACE_H
#define _SESSIONTRACE_H

// Windows App(旧 Remote Desktop)等でのRDP接続/切断・ロック/アンロック時に
// nodokaが応答なしになる不具合の調査用トレーススイッチ。
//
// 有効化: 次行のコメントを外してビルドする。
//         または nodoka.vcxproj / dll.vcxproj (および x64版) の
//         プリプロセッサ定義に NODOKA_SESSION_TRACE を追加する。
// 無効化: マクロを未定義のままにする(デフォルトで無効。本番ビルドへの影響なし)。
//
// 出力先: OutputDebugString (DebugView で "Capture Win32" 有効時に見える) と
//         m_log (ログウィンドウ / ログファイル) の両方。
//         ログウィンドウはUIスレッドのメッセージポンプ(WM_APP_msgStreamNotify)に
//         依存しているため、UIスレッドが応答なしになるとログウィンドウには
//         何も表示されない。OutputDebugStringはメッセージポンプに依存しないため、
//         UIスレッドが完全にブロックしていても記録される。
//         tomsgstream (m_log) は Engine::m_cs とは独立したロックを持つため、
//         Engine側がデッドロックしていても m_log への書き込み自体はブロックされない
//         (が、ログウィンドウへの反映はUIスレッド依存なので上記の通り信頼できない)。
//         debugLevel=0固定で出力するため、ログウィンドウの「詳細(D)」チェックの
//         状態に関わらず必ず表示される。
//
// #define NODOKA_SESSION_TRACE

#include "multithread.h"
#include "stringtool.h"
#include <sstream>

#ifdef NODOKA_SESSION_TRACE

#define SESSTRACE(stream, expr)                                             \
	do                                                                       \
	{                                                                        \
		std::basic_ostringstream<_TCHAR> sesstrace_oss_;                    \
		sesstrace_oss_ << _T("[SESSTRACE] tid=") << GetCurrentThreadId()    \
					  << _T(" t=") << GetTickCount()                        \
					  << _T(" ") << _T(__FUNCTION__) << _T(": ") << expr;   \
		tstring sesstrace_s_ = sesstrace_oss_.str();                        \
		OutputDebugString((sesstrace_s_ + _T("\n")).c_str());               \
		{                                                                    \
			Acquire sesstrace_a_(&(stream), 0);                            \
			(stream) << sesstrace_s_ << std::endl;                          \
		}                                                                    \
	} while (0)

#else

#define SESSTRACE(stream, expr) ((void)0)

#endif // NODOKA_SESSION_TRACE

#endif // !_SESSIONTRACE_H
