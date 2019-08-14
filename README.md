# iodrivers_base

This package mostly contains a generic implementation of a packet reassembly
implementation.

The core problem this aims at solving is the (lot of) misunderstandings that
stems from the complexity of low-level I/O. Additionally, it provides an I/O
independence layer that is used to build a test harness for C++ libraries.
Finally, within the Rock oroGen integration, it allows to monitor and log
byte-level I/O.

Within Rock, it is highly encouraged to use this package to build custom
device drivers, even for protcols that are naturally packet-based such as
e.g. UDP, as the driver packet-extraction logic should do as many sanity
checks on the bytestream as possible.

## Testing harness

This package provides a testing harness that allows you to write integration
tests drivers based on `iodrivers_base::Driver`. Check the documentation of
`iodrivers_base/Fixture.hpp` for more information.