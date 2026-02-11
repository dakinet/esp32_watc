@echo off
C:\Users\Korisnik\AppData\Roaming\Python\Python310\Scripts\platformio.exe run --target upload --project-dir C:\Users\Korisnik\prog\ESP32S3\firmware > C:\Users\Korisnik\prog\ESP32S3\firmware\build_log.txt 2>&1
echo EXIT_CODE=%ERRORLEVEL% >> C:\Users\Korisnik\prog\ESP32S3\firmware\build_log.txt
