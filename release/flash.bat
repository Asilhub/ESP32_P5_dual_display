@echo off
REM ============================================================
REM  P5 Carwash Display DUAL (2 panel, 64x64) - firmware flasher
REM  Ishlatish:  flash.bat            (COM3 default)
REM              flash.bat COM7       (boshqa port)
REM ============================================================
setlocal

set PORT=%1
if "%PORT%"=="" set PORT=COM3

echo.
echo  P5 Carwash Display DUAL v1.0.0 (4 ta bin fayl)
echo.

where esptool >nul 2>&1
if errorlevel 1 goto NOTOOL

if not exist "%~dp0parts\firmware.bin" goto NOFILE

esptool --chip esp32 -p %PORT% -b 921600 write_flash 0x1000 "%~dp0parts\bootloader.bin" 0x8000 "%~dp0parts\partitions.bin" 0xe000 "%~dp0parts\boot_app0.bin" 0x10000 "%~dp0parts\firmware.bin"
if errorlevel 1 goto FLASHFAIL

echo.
echo  TAYYOR. Ekran o'rtasida MECANUZ yozuvi chiqishi kerak.
echo.
pause
exit /b 0

:NOTOOL
echo  XATO: esptool topilmadi.
echo.
echo  O'rnatish:  pip install esptool
echo  Yoki Espressif Flash Download Tool dan foydalaning:
echo    - 0x1000  : bootloader.bin
echo    - 0x8000  : partitions.bin
echo    - 0xe000  : boot_app0.bin
echo    - 0x10000 : firmware.bin
echo.
pause
exit /b 1

:NOFILE
echo  XATO: parts\firmware.bin topilmadi.
pause
exit /b 1

:FLASHFAIL
echo.
echo  XATO: flash qilinmadi.
echo  - Arduino IDE Serial Monitor ochiq bo'lsa, yoping.
echo  - Port raqamini tekshiring (Device Manager).
echo  - USB kabel ma'lumot uzatuvchi (data) kabel ekaniga ishonch hosil qiling.
echo.
pause
exit /b 1
