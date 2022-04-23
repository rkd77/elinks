#!/bin/bash
#
# shell script menus for elinks binary testing
#

CUR_TEST=hello

clear

echo '       --/                                    \--'
echo '       --[ Welcome to the elinks test helper  ]--'
echo '       --[                                    ]--'
echo '       --[ [*] use option 1 to run dump test  ]--'
echo '       --\                                    /--'
echo '                                                 '

# RUN TEST
run_test() {
  if [ "$1" = "hello" ]; then
    ./src/elinks --dump ./test/hello.html
  elif [ "$1" = "http" ]; then
    ./src/elinks http://elinks.or.cz | head
  elif [ "$1" = "https" ]; then
    echo "[=] assumes You're running https server on port 9453"
    echo "[=] see https_server option"
    ./src/elinks \
      -eval 'set connection.ssl.cert_verify = 0' \
      --dump https://127.0.0.1:9453 | head
  fi
}

# HTTPS SERVER MENU
https_menu() {
  HTTPS_OPTS="start_https stop_https certgen return"
  echo "--[ https server menu ]--"
  echo ""
  echo "  [*] use certgen to generate server certificate"
  echo ""
  select SEL in $HTTPS_OPTS; do
    echo "  [*] https server menu "
    if [ "$SEL" = "start_https" ]; then
      python3 ./test/server/https.py &
      PID=`echo $!`
      echo $PID > /tmp/eltmp.pid
      echo "[*} Starting https server (pid $PID)"
    elif [ "$SEL" = "stop_https" ]; then
      PID=`cat /tmp/eltmp.pid`
      echo "[*] Stopping https server (pid $PID)" 
      kill $PID
    elif [ "$SEL" = "certgen" ]; then
      echo "[*] generation ssl certificate for the https server"
      ./test/server/gen.sh
    elif [ "$SEL" = "return" ]; then
      break
    fi
  done
}

# MAIN LOOP
SEL = "none"
OPTS="hello http https null null null null null null null https_server exit"
select SEL in $OPTS; do
  echo "--[ Current test : " $SEL" ]--"
  if [ "$SEL" = "hello" ]; then
    run_test $SEL
  elif [ "$SEL" = "http" ]; then
    run_test $SEL
  elif [ "$SEL" = "https" ]; then
    run_test $SEL
  elif [ "$SEL" = "https_server" ]; then
    https_menu
  elif [ "$SEL" = "exit" ]; then
    exit
  fi
done
