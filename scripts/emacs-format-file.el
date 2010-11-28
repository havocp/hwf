;;; File: emacs-format-file
;;; Stan Warford
;;; 17 May 2006

(defun emacs-format-function ()
   "Format the whole buffer."
   (c-set-style "gnu")
   (setq c-basic-offset 4)
   (setq indent-tabs-mode nil)
   (setq make-backup-files nil)
   (setq backup-inhibited t)
   (setq auto-save-default nil)
   (untabify (point-min) (point-max))
   (indent-region (point-min) (point-max) nil)
   (save-buffer)
)
