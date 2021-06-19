These ACPI drivers are for the MSC C6C ComExpress boards based on
Baytrail and Apollo Lake Intel Atoms. That Hardware has 8 freely usable
GPIOs on the ComExpress connector which are exposed to the OS by the
ACPI node "MEX0001"



--- acpi-get-gpio ---

This driver shows how to acquire and release a GPIO using the modern
destructor-like devm_* functions.



--- acpi-gpio-pps-client ---

This driver uses the acpi-get-gpio example and enhances it to provide
a PPS source to OS by using the PPS framework. This driver can support
all 8 available GPIOs at once if loaded with the bitmask module parameter
set to 255 (all 8 bits). Though, that may not work on all boards, because
some define 4 GPI and 4 GPO, where the direction can not be changed.
