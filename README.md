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

All URIs follow the general format `scheme://NAME:NUMBER?option1=value1&option2=value2`

Which parts of the URI is accepted by which scheme is detailed below

### serial://

Open a serial device and configure it. A basic serial URI is
`serial://${DEVICE_PATH}:${BAUDRATE}`. Note that absolute paths lead to
having **three** consecutive slashes (two for the `://` and one for the path)

The serial URIs accept the following options:
- `byte_size` byte size in bits, from 5 to 8. The default is 8
- `parity` parity, either `none`, `even` or `odd`. The default is `none`
- `stop_bits` stop bits (either 1 or 2). The default is 1

Examples:

- `serial:///dev/ttyUSB0:115200`
- `serial:///dev/ttyUSB0:115200?parity=even`

### udp://

Open an UDP socket on a random port which sends to the given host and port.

If the `local_port` option is given, the socket is bound to the given local port

Examples:
- `udp://localhost:4000`
- `udp://localhost:4000?local_port=4001`

**The Connection Refused error** If configured to do so, UDP streams will report
a connection refused error if there are no processes listening on the configured
remote peer. This is controlled by the `ignore_connrefused` parameter which has
to be set to 0 or 1. For backward compatibility reasons, the default behavior of
UDP streams with respect to this option is complex, see below for details.

**Connected UDP sockets** If configured to do so, UDP streams are _connected_,
that is will only accept packets from the configured remote host. If unconnected,
they will receive from any host (but still send to the configured host). This
is controlled by the `connected` parameter which has to be set to 0 or 1. For
backward compatibility reasons, the default behavior of UDP streams with
respect to this option is complex, see below for details.

**Default `connected` and `ignore_connrefused` parameters**

For backward compatibility reasons, the default values for `connected` and
`ignore_connrefused` depends on whether the UDP stream was created with or without
a local port. The behavior described below is deprecated. In the future, the default
will be set to `connected=1` and `ignore_connrefused=1`. Warnings are currently issued
when the current defaults are used. Explicitely set these parameters to shut the
warnings and ensure your code will continue working as-is when the defaults change.

- `ignore_connrefused` may be set to zero (to report connrefused) only on connected
  sockets. Attempting to set `ignore_connrefused=0&connected=0` will throw in `openURI`
- setting `connected=0` automatically sets `ignore_connrefused=1` even if the default
  (as described below) would be 0

- UDP sockets for which the local port is unspecified (without the
  `local_port` option) are configured with `connected=1` and `ignore_connrefused=0`
  by default.
- UDP sockets for which the local port is given (with the
  `local_port` option) are configured with `connected=0` and `ignore_connrefused=1`
  by default.

### udpserver://

Passively listens to UDP packets on a given port. Writing to the driver will send
data back to the last UDP client whose packet was received (and does nothing if
nothing has been received yet).

Examples:
- `udpserver://5000`

### tcp://

Open a TCP connection to the given remote host and port

Examples:

- `tcp://localhost:5000`

### file://

Open a file. Note that absolute paths lead to having **three** consecutive slashes
(two for the `://` and one for the file)

Examples:

- `file:///path/to/file

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

## Command-line tools

This package provides two command-line utilities:

- `iodrivers_base_forward` forwards one data stream to another. Both streams are
  defined by iodrivers_base's URIs
- `iodrivers_base_cat` outputs the data from a stream to stdout, in hex and
  ascii formats

For anything more complicated, we recommend usage of
[socat](https://linux.die.net/man/1/socat)

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
