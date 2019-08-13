#ifndef IODRIVERS_BASE_FORWARD_HPP
#define IODRIVERS_BASE_FORWARD_HPP

#include <base/Time.hpp>

namespace iodrivers_base {
    class Driver;

    /** Forward data between two subclasses of iodrivers_base::Driver
     *
     * It blocks until one of the drivers is closed
     *
     * @param raw_mode whether bytes are read using readRaw (true) or readPacket(false)
     * @param driver1 one of the two drivers
     * @param timeout1 how long we should wait, while reading, before
     *                 forwarding the data received from driver2. Set to avoid
     *                 unnecessary fragmentation
     * @param driver2 the other driver
     * @param timeout2 how long we should wait, while reading, before
     *                 forwarding the data received from driver2. Set to avoid
     *                 unnecessary fragmentation
     * @param buffer_size the size of the reading buffer size. In practice,
     *                    it sets the maximum size of a forwarded packet
     */
    void forward(bool raw_mode, Driver& driver1, Driver& driver2,
                 base::Time timeout1 = base::Time(),
                 base::Time timeout2 = base::Time(),
                 size_t buffer_size = 32768);
}

#endif