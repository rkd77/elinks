@ECHO OFF
SET ELDIR=%~dp0
:: Remove trailing \ from %~p0
SET ELDIR=%ELDIR:~0,-1%
SET ELINKS_CONFDIR=%ELDIR%
SET XDG_CONFIG_HOME=%ELDIR%
SET CURL_CA_BUNDLE=%ELDIR%\curl-ca-bundle.crt
"%ELDIR%\src\elinks.exe"
