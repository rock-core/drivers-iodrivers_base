#ifndef IODRIVERS_BASE_FORWARD_HPP
#define IODRIVERS_BASE_FORWARD_HPP

#include <base/Time.hpp>

namespace iodrivers_base {
    class Driver;

    /** Forward data between two subclasses of iodrivers_base::Driver
     *
     * It blocks until one of the connections is closed (the driver
     * detects an EOF)
     *
     * The timeouts are meant to be used to avoid passing data byte-by-byte.
     * This is important when forwarding from a relatively slow connection (e.g.
     * serial) to a packet-based connection that has an overhead (e.g. UDP).
     *
     * With a non-zero timeout, the driver will read few bytes, push them as a
     * packet and repeat. This would have a dramatic effect on performance on
     * the packet-based side. A well-chosen timeout would:
     *
     * <li>
     * <ul> allow the driver to read bytes in bigger chunks. This is dependent
     *      on the connection type and speed (and, in smaller part, on the
     *      sending device)
     * <ul> not have a significant effect on latency for your application. The
     *      timeout as it is implemented is a maximum latency value between the
     *      reception of a first byte and the sending of the bytes received.
     * </li>
     *
     * @param raw_mode whether bytes are read using readRaw (true) or readPacket(false)
     * @param driver1 one of the two drivers
     * @param timeout1 how long we should wait, while reading, before
     *                 forwarding the data received from driver2. Set to a
     *                 nonzero value to gather bytes into bigger chunks when
     *                 forwarding from a slow connection
     * @param driver2 the other driver
     * @param timeout2 how long we should wait, while reading, before
     *                 forwarding the data received from driver2. Set to avoid
     *                 nonzero value to gather bytes into bigger chunks when
     *                 forwarding from a slow connection
     * @param buffer_size the size of the reading buffer size. In practice,
     *                    it sets the maximum size of a forwarded packet
     */
    void forward(bool raw_mode, Driver& driver1, Driver& driver2,
                 base::Time timeout1 = base::Time(),
                 base::Time timeout2 = base::Time(),
                 size_t buffer_size = 32768);
}

#endif
