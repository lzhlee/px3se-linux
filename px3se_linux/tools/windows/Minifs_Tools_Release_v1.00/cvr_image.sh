;db�����ʾ����boot
db Image\RV1108_usb_boot.bin
;wd�����ʾ�ȴ��豸,����Ϊ0�ǵȴ�maskrom�豸,Ϊ1�ǵȴ�loader�豸
wd 0
;wl�����ǰ�lbaд�ļ�,��1��������ƫ�ƣ���2���������ļ�·��
wl 0x10000000 ..\..\rockimg\Image-cvr\Firmware.img
rd 1
