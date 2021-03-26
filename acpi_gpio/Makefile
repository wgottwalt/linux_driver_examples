ccflags-y = -DEXPORT_SYMTAB -Wall -Werror
obj-m := acpi-get-gpio.o
obj-m += acpi-gpio-pps-client.o

KDIR := /lib/modules/$(shell uname -r)/build/
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

-include $(KDIR)/Rules.make
