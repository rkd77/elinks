;;; USER CODE

(use-modules (ice-9 optargs)		;let-optional
	     (ice-9 regex)
	     (srfi srfi-2)		;and-let*
	     (srfi srfi-8)		;receive
	     (srfi srfi-13)		;string-lib
	     )


;;; goto-url-hooks
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Handle search URLs

;; Makes a searcher routine.  If the routine is called without any
;; arguments, return the home page location.  Otherwise, construct a
;; URL searching for the arguments specified.
;; e.g.
;;  (define f (make-searcher "http://www.google.com/"
;;                           "http://www.google.com/search?q="
;;                           "&btnG=Google%20Search"))
;;  (f '())
;;   => "http://www.google.com/"
;;  (f '("google" "me"))
;;   => "http://www.google.com/search?q=google%20me&btnG=Google%20Search"
(define (make-searcher home-page prefix . maybe-postfix)
  (let-optional maybe-postfix ((postfix ""))
    (lambda (words)
      (if (null? words)
	  home-page
	  (string-append prefix (string-join words "%20") postfix)))))

;; TODO: ,gg -> gg: format update to the standard ELinks one. --pasky

(define goto-url-searchers
  `((",gg"   . ,(make-searcher "http://www.google.com/"
			       "http://www.google.com/search?q=" "&btnG=Google%20Search"))
    (",fm"   . ,(make-searcher "http://www.freshmeat.net/"
			       "http://www.freshmeat.net/search/?q="))
    (",dict" . ,(make-searcher "http://www.dictionary.com/"
			       "http://www.dictionary.com/cgi-bin/dict.pl?db=%2A&term="))
    (",wtf"  . ,(make-searcher "http://www.ucc.ie/cgi-bin/acronym?wtf"
			       "http://www.ucc.ie/cgi-bin/acronym?"))))

(add-hook! goto-url-hooks
	   (lambda (url)
	     (let* ((words (string-tokenize url))
		    (key (car words))
		    (rest (cdr words)))
	       (cond ((assoc key goto-url-searchers) =>
		      (lambda (x) ((cdr x) rest)))
		     (else #f)))))


;;; goto-url-hooks
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Handle simple URLs

(define goto-url-simples
  `((",forecast" . "http://www.bom.gov.au/cgi-bin/wrap_fwo.pl?IDV10450.txt")
    (",local" . "XXXXXXXXXXXXXXXXXXX")
    ))

(add-hook! goto-url-hooks
	   (lambda (url)
	     (cond ((assoc url goto-url-simples) => cdr)
		   (else #f))))


;;; goto-url-hooks
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Expand ~/ and ~user/ URLs

(define (home-directory . maybe-user)
  (let-optional maybe-user ((user (cuserid)))
    (and-let* ((user (catch 'misc-error
			    (lambda () (getpwnam user))
			    (lambda ignore #f))))
	      (passwd:dir user))))

(define (expand-tilde-file-name file-name)
  (and (string-prefix? "~" file-name)
       (let* ((slash/end (or (string-index file-name #\/)
			     (string-length file-name)))
	      (user (substring file-name 1 slash/end)))
	 (string-append (if user
			    (home-directory)
			    (home-directory user))
			(substring file-name slash/end)))))

(add-hook! goto-url-hooks
	   (lambda (url)
	     (and (string-prefix? "~" url)
		  (expand-tilde-file-name url))))


;;; pre-format-html-hooks
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Mangle linuxgames.com pages

(add-hook! pre-format-html-hooks
	   (lambda (url html)
	     (and (string-contains url "linuxgames.com")
		  (and-let* ((start (string-contains html "<CENTER>"))
			     (end (string-contains html "</center>" (+ start 1))))
			    (string-append (substring/shared html 0 start)
					   (substring/shared html (+ end 10)))))))


;;; pre-format-html-hooks
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Mangle dictionary.com result pages

(add-hook! pre-format-html-hooks
  (lambda (url html)
    (and (string-contains url "dictionary.reference.com/search?")
	 (and-let* ((m (string-match
			(string-append
			 "<table border=\"0\" cellpadding=\"2\" width=\"100%\">"
			 ".*<td width=\"120\" align=\"center\">")
			html)))
	   (string-append "<html><head><title>Dictionary.com lookup</title>"
			  "</head><body>"
			  (regexp-substitute/global #f
			      "<br>\n<p><b>" (match:substring m 0)
			    'pre "<br>\n<hr>\n<p><b>" 'post))))))


;;; get-proxy-hooks
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Some addresses require a special proxy 

(add-hook! get-proxy-hooks
	   (lambda (url)
	     (and (or (string-contains url "XXXXXXXXXXXXXX")
		      (string-contains url "XXXXXXXXXXXXXX"))
		  "XXXXXXXXXXXXXXXXXXXXXXXXXXX")))


;;; get-proxy-hooks
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Some addresses work better without a proxy

(add-hook! get-proxy-hooks
	   (lambda (url)
	     (and (or (string-contains url "XXXXXXXXXXXXXXXXXXX")
		      (string-contains url "XXXXXXXXXX"))
		  "")))


;;; quit-hooks
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Delete temporary files when quitting

(define temporary-files '())

(add-hook! quit-hooks
	   (lambda ()
	     (for-each delete-file temporary-files)))

;;; The end
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
