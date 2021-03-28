That PCI watchdog driver is for the Quancom PWDOG12N series watchdog
cards and shows how to use the latest watchdog framework of the Linux
kernel.

The hardware itself is quite simple. There are only two io ports
documented, one to retrigger the watchdog and one to disable it.
Triggering it is the same like starting it. There seem to be much more
usable io ports (mem-mapped area is 256 bytes after all), but it is not
easy to figure them out.
