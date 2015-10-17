srcDir:=../src

obj-m += sessionmodule.o

sessionmodule-objs += $(srcDir)/module.o $(srcDir)/sessionsyscall.o $(srcDir)/sessionFileOperations.o

all: module

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	rm $(srcDir)/module.o
	rm $(srcDir)/sessionsyscall.o
	rm $(srcDir)/sessionFileOperations.o
	
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
