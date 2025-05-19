;;; nodoka-mode.el --- nodoka setting editing commands for Emacs

;;; Copyright (C) 2000-2005 TAGA Nayuta <nayuta@ganaware.org>

;; Author: TAGA Nayuta <nayuta@ganaware.org>
;; Maintainer: TAGA Nayuta <nayuta@ganaware.org>
;; Keywords: languages, faces
;; Modify by applet. 2008-10-25

(require 'font-lock)

(defvar
  nodoka-font-lock-keywords
  (let* ((warning-face 
	  (if (boundp 'font-lock-warning-face)
	      font-lock-warning-face
	    font-lock-function-name-face))
	 (preprocessor-face 
	  (if (boundp 'font-lock-builtin-face)
	      font-lock-builtin-face
	    font-lock-preprocessor-face))
	 (function-name-face 
	  (if (boundp 'font-lock-builtin-face)
	      font-lock-builtin-face
	    font-lock-function-name-face)))
    `((,(concat
	 "\\<\\("
	 "[AMCWS]-"
	 "\\|IC-"
	 ;;"\\|I-"
	 "\\|[INCSK]L-"
	 "\\|M[0-9]-"
	 "\\|L[0-9]-"
	 "\\|U-"
	 "\\|D-"
	 "\\|R-"
	 "\\|E[01]-"
	 "\\|MAX-"
	 "\\|MIN-"
	 "\\|MMAX-"
	 "\\|MMIN-"
	 "\\)"
	 ) . font-lock-keyword-face)
      ("#.*$" . font-lock-comment-face)
      ("/[^/\n]*/" . font-lock-string-face)
      ("\\\\$" . ,warning-face)
      (,(concat
	 "^\\s *\\<\\("
	 "key"
	 "\\|event\\s +\\(prefixed\\|after-key-up\\|before-key-down\\)"
	 "\\|keyseq"
	 "\\|def\\s +\\(key\\|alias\\|mod\\|sync\\|subst\\|option\\)"
	 "\\|mod"
	 "\\|keymap"
	 "\\|keymap2"
	 "\\|window"
	 "\\|include"
	 "\\|if"
	 "\\|define"
	 "\\|else"
	 "\\|elseif"
	 "\\|elsif"
	 "\\|endif"
	 "\\)\\>"
	 ) . ,preprocessor-face)
      (,(concat
	 "&\\("
	 "Default"
	 "\\|KeymapParent"
	 "\\|KeymapWindow"
	 "\\|KeymapPrevPrefix"
	 "\\|OtherWindowClass"
	 "\\|Prefix"
	 "\\|Keymap"
	 "\\|Sync"
	 "\\|Toggle"
	 "\\|EditNextModifier"
	 "\\|Variable"
	 "\\|Repeat"
	 "\\|Undefined"
	 "\\|CancelPrefix"
	 "\\|IconColor"
	 "\\|Ignore"
	 "\\|PostMessage"
	 "\\|ShellExecute"
	 "\\|SendPostMessage"
	 "\\|SendText"
	 "\\|SetForegroundWindow"
	 "\\|SetImeConvStatus"
	 "\\|LoadSetting"
	 "\\|VK"
	 "\\|Wait"
	 "\\|InvestigateCommand"
	 "\\|NodokaDialog"
	 "\\|DescribeBindings"
	 "\\|HelpMessage"
	 "\\|HelpVariable"
	 "\\|WindowRaise"
	 "\\|WindowLower"
	 "\\|WindowMinimize"
	 "\\|WindowMaximize"
	 "\\|WindowHMaximize"
	 "\\|WindowVMaximize"
	 "\\|WindowHVMaximize"
	 "\\|WindowMove"
	 "\\|WindowMoveTo"
	 "\\|WindowMoveVisibly"
	 "\\|WindowClingToLeft"
	 "\\|WindowClingToRight"
	 "\\|WindowClingToTop"
	 "\\|WindowClingToBottom"
	 "\\|WindowClose"
	 "\\|WindowToggleTopMost"
	 "\\|WindowIdentify"
	 "\\|WindowSetAlpha"
	 "\\|WindowRedraw"
	 "\\|WindowResizeMoveTo"
	 "\\|WindowResizeMoveToPer"
	 "\\|WindowResizeTo"
	 "\\|WindowResizeToPer"
	 "\\|WindowMonitor"
	 "\\|WindowMonitorTo"
	 "\\|MouseMove"
	 "\\|MouseWheel"
	 "\\|ClipboardChangeCase"
	 "\\|ClipboardUpcaseWord"
	 "\\|ClipboardDowncaseWord"
	 "\\|ClipboardCopy"
	 "\\|EmacsEditKillLinePred"
	 "\\|EmacsEditKillLineFunc"
	 "\\|LogClear"
	 "\\|DirectSSTP"
	 "\\|PlugIn"
	 "\\|Recenter"
	 "\\|SetImeStatus"
	 "\\|SetImeString"
	 "\\)\\>"
	 ) . ,function-name-face)
      "Default font-lock-keywords for nodoka mode.")))

(defvar nodoka-mode-syntax-table nil
  "syntax table used in nodoka mode")
(setq nodoka-mode-syntax-table (make-syntax-table))
(modify-syntax-entry ?# "<\n" nodoka-mode-syntax-table)
(modify-syntax-entry ?\n ">#" nodoka-mode-syntax-table)

(defvar nodoka-mode-map
  (let ((map (make-sparse-keymap)))
    (define-key map "\C-c\C-c" 'comment-region)
    map))

;;;###autoload
(defun nodoka-mode ()
  "A major mode to edit nodoka setting files."
  (interactive)
  (kill-all-local-variables)
  (use-local-map nodoka-mode-map)

  (make-local-variable 'comment-start)
  (setq comment-start "# ")
  (make-local-variable 'comment-start-skip)
  (setq comment-start-skip "\\(^\\|\\s-\\);?#+ *")
  (make-local-variable 'comment-indent-function)
  (setq comment-indent-function 'nodoka-comment-indent)
  (make-local-variable 'parse-sexp-ignore-comments)
  (setq parse-sexp-ignore-comments t)

  (make-local-variable	'font-lock-defaults)
  (setq major-mode 'nodoka-mode
	mode-name "nodoka"
	font-lock-defaults '(nodoka-font-lock-keywords nil)
	)
  (set-syntax-table nodoka-mode-syntax-table)
  (run-hooks 'nodoka-mode-hook))

;;; derived from perl-mode.el
(defun nodoka-comment-indent ()
  (if (and (bolp) (not (eolp)))
      0					;Existing comment at bol stays there.
    (save-excursion
      (skip-chars-backward " \t")
      (max (if (bolp)			;Else indent at comment column
 	       0			; except leave at least one space if
 	     (1+ (current-column)))	; not at beginning of line.
 	   comment-column))))

(provide 'nodoka-mode)

;;; nodoka-mode.el ends here
