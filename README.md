# iodrivers_base

This package mostly contains a generic implementation of a packet reassembly
logic. It takes byte-oriented I/O and allows you to turn this stream of bytes
into a stream of well-defined, validated packages to be interpreted by
a higher-level logic (e.g. data demarshalling).

The core problem this aims at solving is the (lot of) misunderstandings that
stems from the complexity of low-level I/O. Additionally, it provides an I/O
independence layer that is used to build a test harness for C++ libraries.
Finally, within the Rock oroGen integration, it allows to monitor and log
byte-level I/O.

Within Rock, it is highly encouraged to use this package to build custom
device drivers, even for protcols that are naturally packet-based such as
e.g. UDP, as the driver packet-extraction logic should do as many sanity
checks on the bytestream as possible.

## Functionality

The main functionalities that `iodrivers_base` provide are:

- infrastructure to convert byte-oriented streams into streams of validated
  packages
- library-level test harness
- generic oroGen integration (for Rock users)
- built-in support for multiple type of I/O:
  - serial
  - udp server: listens on a UDP port. Sends to the IP of the last received
    UDP message.
  - udp client: connects to a given IP and port.
  - tcp client: connects to a given IP and port.
  - file-based (e.g. for named pipes or Unix sockets)

Note that `iodrivers_base` can (and maybe should) be used even for
datagram-oriented mediums such as UDP. These mediums do not need
the packet-reassembly, but do need packet validation.

## Writing a device driver using `iodrivers_base::Driver`

One starts by subclassing `iodrivers_base::Driver` and providing the size of
the driver's static internal buffer. This buffer should be a multiple of the maximum
packet size (twice as big is usually enough):

~~~cpp
class Driver : iodrivers_base::Driver
{
    static const int MAX_PACKET_SIZE = 32;
    static const int INTERNAL_BUFFER_SIZE = MAX_PACKET_SIZE * 4;
public:
    Driver();

};
~~~

~~~cpp
Driver::Driver()
    : iodrivers_base::Driver::Driver(INTERNAL_BUFFER_SIZE);
{
}
~~~

The second required step is to implement `iodrivers_base::Driver::extractPacket`.
This is the method that tells `iodrivers_base`'s internal logic what is meaningful
data and what isn't.

See below the pseudo-code implementation of most `extractPacket` methods. Note
that the amount of validation you do in `extractPacket` may vary, but usually
"more is better", as it is validation that won't have to be done later.
The return value is fully documented within the method documentation. Check it
out for a complete specification.

~~~cpp
int Driver::extractPacket(uint8_t const* buffer, size_t buffer_size) const {
    if (not enough bytes in buffer to contain a start code) {
        return 0; // wait for new bytes
    }

    if (buffer does not start with packet start code) {
        for (int i = 1; i < buffer_size; ++i) {
            if (packet start code found at i) {
                return -i; // skip i bytes from buffer and call again
            }
        }
        return -buffer_size; // discard the whole buffer
    }

    if (not enough bytes in buffer to validate packet starting at zero) {
        return 0; // wait for new bytes
    }
    else if (packet starting at zero is valid) {
        return packet_size;
    }
    else {
        return -1; // discard first byte and start searching again
    }
}
~~~

## Supported URIs

- serial ports are open with `serial:/${DEVICE_PATH}[:BAUDRATE]`, e.g.
  `serial:///dev/ttyUSB0:115200`
- a UDP server is open with `udpserver://[PORT]`. The UDP server will
  receive from any source, and send to the IP/Port of the last received
  packet it received
- a UDP client is open with `udp://host:REMOTE_PORT[:LOCAL_PORT]`. The UDP client will
  receive from anywhere, and send to the specified host/Port. Host can
  be an IP or a hostname. The port of the UDP socket itself can be optionally
  specified
- a TCP client is open with `tcp://host:PORT`

## Test harness

This package provides a testing harness that allows you to write integration
tests drivers based on `iodrivers_base::Driver`.

From a design perspective, one should start by separating the protocol implementation
(including the `extractPacket` logic by `iodrivers_base::Driver`) into a separate
set of stateless functions. This ensures they will be fully and easily testable.

The test harness, then, allows you to check the `Driver` class logic from the
perspective of the device, that is by checking what is being sent by the
driver, and sending data to it. The goal is to verify the `Driver`'s logic, not
the protocol parsing since this one has been implemented in separation (and
separately tested).

Check the documentation of `iodrivers_base/Fixture.hpp` for more information
on how to use the harness.

## Gotchas

* do not overload `openURI`. This will make testing harder (you have to
  "mock" whathever is being done in `openURI` for each test), and will definitely
  reduce the usefulness of your driver. Finally, it is incompatible with Rock's
  oroGen integration for `iodrivers_base`.

## Design Guidelines

See [this document](https://rock-robotics.org/rock-and-syskit/cookbook/device_drivers.html)

## License

This software is licensed under the GNU LGPL version 2 or later

Copyright 2008-2017 DFKI Robotics Innovation Center
          2014-2017 SENAI-CIMATEC
          2017-2019 13 Robotics
          2019      TideWise
