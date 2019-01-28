Afptool -pack .\backupimage backupimage\backup.img
Afptool -pack ./ rockimg\update.img


RKImageMaker.exe -RK312A rockimg\MiniLoaderAll.bin  rockimg\update.img update.img -os_type:androidos

rem update.img is new format, rockimg\update.img is old format, so delete older format
del  rockimg\update.img

pause 
