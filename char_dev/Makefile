obj-m += sun_char.o
 
all:
	echo "Copying Module.symvers to resolve sun_bus exports"
	#cp -v $(PWD)/../sun_bus/Module.symvers $(PWD)
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) KBUILD_EXTRA_SYMBOLS=$(PWD)/../sun_bus/Module.symvers modules
	gcc test.c -o test
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm test
