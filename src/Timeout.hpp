#ifndef IODRIVERS_BASE_TIMEOUT_HPP
#define IODRIVERS_BASE_TIMEOUT_HPP

#include <sys/time.h>

namespace iodrivers_base {

/** A timeout tracking class
 */
class Timeout {
private:
    unsigned int timeout;
    timeval start_time;

public:
    /**
     * Initializes and starts a timeout
     * @param timeout  time in ms
     */
    Timeout(unsigned int timeout = 0);

    /**
     * Restarts the timeout
     */
    void restart();

    /**
     * Checks if the timeout is already elapsed.
     * This uses a syscall, so use sparingly and cache results
     * @returns  true if the timeout is elapsed
     */
    bool elapsed() const;

    /**
     * Checks if the timeout is already elapsed.
     * This uses a syscall, so use sparingly and cache results
     * @param timeout  a custom timeout
     * @returns  true if the timeout is elapsed
     */
    bool elapsed(unsigned int timeout) const;

    /**
     * Calculates the time left for this timeout
     * This uses a syscall, so use sparingly and cache results
     * @returns  number of milliseconds this timeout as left
     */
    unsigned int timeLeft() const;

    /**
     * Calculates the time left for this timeout
     * This uses a syscall, so use sparingly and cache results
     * @param timeout  a custom timeout
     * @returns  number of milliseconds this timeout as left
     */
    unsigned int timeLeft(unsigned int timeout) const;
};

}

#endif

