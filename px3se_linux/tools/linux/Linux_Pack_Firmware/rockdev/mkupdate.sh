#!/bin/bash
pause()
{
echo "Press any key to quit:"
read -n1 -s key
exit 1
}
echo "start to make update.img..."
if [ ! -f "rockimg/parameter" -a ! -f "rockimg/parameter-emmc.txt" ]; then
	echo "Error:No found parameter!"
	exit 1
fi
if [ ! -f "package-file" ]; then
	echo "Error:No found package-file!"
	exit 1
fi
./afptool -pack ./ rockimg/update.img || pause
./rkImageMaker -RK312A rockimg/MiniLoaderAll.bin rockimg/update.img update.img -os_type:androidos || pause
echo "Making update.img OK."
#echo "Press any key to quit:"
#read -n1 -s key
exit $?
