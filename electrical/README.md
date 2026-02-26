# Production-Ready Status.

The following list below are files considered "production-ready".
This is an informal status given to the code to essentially say
that bugs and errors from them are unlikely and should not be expected.
New features, API changes, etc. can also be expected to be much less frequent,
and if any changes are made,
careful documentation and testing will be done to validate them.



- `ringbuffer.c`
> The ring-buffer implementation is **production-ready**.
> Issues concerning usage and behavior of ring-buffers are unlikely.
>
> \- *Phuc Doan. Feburary 22nd, 2026.*



- `timekeeping.c`
> The driver for keeping track of elapsed time with microsecond granularity is **production-ready**.
> Timer accuracy is not accounted for in this implementation,
> but for our application, this should be fine.
>
> \- *Phuc Doan. Feburary 25th, 2026.*



- `sd_initer.c`
> The state-machine implementation for initializing SD cards is **production-ready**.
> The implementation is not guaranteed to support all SD cards,
> but all error conditions are well-defined and handled accordingly.
> Issues concerning usage and behavior of the SD-initer are unlikely.
>
> \- *Phuc Doan. Feburary 22nd, 2026.*



- `sd_cmder.c`
> The state-machine implementation for handling SDMMC transfers is **production-ready**.
> The implementation is harden towards hardware issues,
> such as unexpected SD card ejection/reinsertion.
> Hardware issues such as card glitches are still possible,
> but the state-machine is designed to be easily resettable in such rare events.
> Issues concerning usage and behavior of the SD-cmder are unlikely.
>
> \- *Phuc Doan. Feburary 23rd, 2026.*



- `sd.c`
> The API for doing read/writes to SD cards is **production-ready**.
> Issues concerning usage and behavior of the SD API are unlikely.
>
> \- *Phuc Doan. Feburary 23rd, 2026.*



- `ovcam.c`
> The driver for the OV5640 camera module is **production-ready**.
> The register initialization sequence is open to being modified.
> The driver is designed to work with the OV5640 module specifically,
> but other compatiable camera modules may work too.
> Issues concerning usage and behavior of the OVCAM driver are unlikely.
>
> \- *Phuc Doan. Feburary 25th, 2026.*



- `uxart.c`
> The driver for UART communication is **production-ready**.
> Because of how simple the interface is,
> the implementation does not raise any error conditions to the user;
> checksums in any data-transfer should be sufficient.
> Operationally, the driver should behave as expected
> when doing full-duplex and half-duplex transfers.
>
> \- *Phuc Doan. Feburary 26th, 2026.*



Files not listed above are not considered **production-ready**.
This does not mean they are unusable, just that development is still undergoing.
