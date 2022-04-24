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
    $ELINKS --dump ./test/hello.html
  elif [ "$1" = "http" ]; then
    echo "[=] assumes You're running https server on port 9452"
    echo "[=] see http_server option"
    $ELINKS \
      --dump http://127.0.0.1:9452 | head
  elif [ "$1" = "https" ]; then
    echo "[=] assumes You're running https server on port 9453"
    echo "[=] see https_server option"
    $ELINKS \
      -eval 'set connection.ssl.cert_verify = 0' \
      --dump https://127.0.0.1:9453 | head
  elif [ "$1" = "interactive" ]; then
    # set 256 colors terminal
    # use: export TERM=xterm-256color
    # and use compiled elinks with the tests folder
    if [ "$ARCH" = "win64" ]; then 
      export TERM=dumb
      $ELINKS \
        --config-dir `pwd`/test/etc \
        ./test
    else
      export TERM=xterm-256color
      $ELINKS \
        --config-dir `pwd`/test/etc \
        -eval 'set terminal.xterm-256color.colors = 3' \
        ./test
    fi
  fi
}

# HTTP SERVER MENU
http_menu() {
  HTTP_OPTS="start_http stop_http return"
  echo ""
  echo "--[ http server menu ]--"
  echo ""
  select SEL in $HTTP_OPTS; do
    echo "  [*] http server menu "
    if [ "$SEL" = "start_http" ]; then
      python3 ./test/server/httpf.py &
      PID=`echo $!`
      echo $PID > /tmp/eltmpf.pid
      echo "[*} Starting http server (pid $PID)"
    elif [ "$SEL" = "stop_http" ]; then
      PID=`cat /tmp/eltmpf.pid`
      echo "[*] Stopping http server (pid $PID)" 
      kill $PID
    elif [ "$SEL" = "return" ]; then
      break
    fi
  done
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
#
# When called from build.sh it will get first
# parameter as binary suffix and second parameter
# as architecture
#
if [ -d $1 ]; then
  BIN_SUFFIX=""
else
  BIN_SUFFIX=$1
fi
if [ -d $2 ]; then
  ARCHIT="lin64"
else
  ARCHIT=$2
fi
echo $ARCH $BIN_SUFFIX
if [ -f ../bin/elinks_$ARCH$BIN_SUFFIX ]; then
  ELINKS=../bin/elinks_$ARCH$BIN_SUFFIX
else
  ELINKS=./src/elinks$BIN_SUFFIX
fi
SEL="none"
OPTS="hello http https interactive null null null null null http_server https_server exit"
select SEL in $OPTS; do
  echo "[*] Current test : " $SEL 
  echo "[*] Current bin  : " $ELINKS
  if [ "$SEL" = "hello" ]; then
    run_test $SEL
  elif [ "$SEL" = "http" ]; then
    run_test $SEL
  elif [ "$SEL" = "https" ]; then
    run_test $SEL
  elif [ "$SEL" = "interactive" ]; then
    run_test $SEL
  elif [ "$SEL" = "http_server" ]; then
    http_menu
  elif [ "$SEL" = "https_server" ]; then
    https_menu
  elif [ "$SEL" = "exit" ]; then
    exit
  fi
done
