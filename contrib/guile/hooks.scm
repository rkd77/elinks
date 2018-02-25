;;; Bare interface to C code


(define-module (elinks internal)
  :export (goto-url-hooks
	   follow-url-hooks
	   pre-format-html-hooks
	   get-proxy-hooks
	   quit-hooks))


;;; GOTO-URL-HOOKS: Each hook is called in turn with a single argument
;;; (a URL string).  Each may return one of:
;;;
;;;    a string    to visit the returned url
;;;    ()          to go nowhere
;;;    #f          to continue with the next hook
;;;
;;; If no hooks return a string or empty list, the default action is
;;; to visit the original URL passed.

(define goto-url-hooks (make-hook 1))

(define (%goto-url-hook url)
  (%call-hooks-until-truish goto-url-hooks
			    (lambda (h) (h url))
			    url))


;;; FOLLOW-URL-HOOKS: Each hook is called in turn with a single
;;; argument (a URL string).  Each may return one of:
;;;
;;;    a string    to visit the returned url
;;;    ()          to go nowhere
;;;    #f          to continue with the next hook
;;;
;;; If no hooks return a string or empty list, the default action is
;;; to visit the original URL passed.

(define follow-url-hooks (make-hook 1))

(define (%follow-url-hook url)
  (%call-hooks-until-truish follow-url-hooks
			    (lambda (h) (h url))
			    url))


;;; PRE-FORMAT-HTML-HOOKS:

(define pre-format-html-hooks (make-hook 2))
(define (%pre-format-html-hook url html)
  (%call-hooks-until-truish pre-format-html-hooks
			    (lambda (h) (h url html))
			    #f))


;;; GET-PROXY-HOOKS:
(define get-proxy-hooks (make-hook 1))
(define (%get-proxy-hook url)
  (%call-hooks-until-truish get-proxy-hooks
			    (lambda (h) (h url))
			    #f))


;;; QUIT-HOOKS: ...

(define quit-hooks (make-hook))

(define (%quit-hook)
  (run-hook quit-hooks))


;;; Helper: calls hooks one at a time until one of them returns
;;; non-#f.
(define (%call-hooks-until-truish hooks caller default)
  (let lp ((hs (hook->list hooks)))
    (if (null? hs)
	default
	(or (caller (car hs))
	    (lp (cdr hs))))))
