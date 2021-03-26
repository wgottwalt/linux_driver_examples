--- acpi-get-gpio ---

This is a test driver for ACPI based GPIOs, which can be found on the
MSC C6C ComExpress boards based on Baytrail and Apollo Lake Intel Atoms.
That Hardware has 8 freely usable GPIOs on the ComExpress connector
which are exposed to the OS by the ACPI node "MEX0001". This driver
shows how to acquire and release a GPIO using the modern destructor-like
devm_* functions.
