#ifndef IODRIVERS_BASE_TIMEOUT_HPP
#define IODRIVERS_BASE_TIMEOUT_HPP

#include <base/Time.hpp>

namespace iodrivers_base {

/** A timeout tracking class
 */
class Timeout {
private:
    base::Time timeout;
    base::Time start_time;

public:

    /**
     * Initializes and starts a timeout
     */
    Timeout(base::Time timeout = base::Time());

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

    /** Returns the timeout set at construction time */
    base::Time getTimeout() const;

    /**
     * Checks if the timeout is already elapsed.
     * This uses a syscall, so use sparingly and cache results
     * @param timeout  a custom timeout
     * @returns  true if the timeout is elapsed
     */
    bool elapsed(base::Time timeout) const;

    /**
     * Calculates the time left for this timeout
     */
    base::Time remaining() const;

    /**
     * Calculates the time left before the given timeout expires
     *
     * @param timeout a custom timeout
     * @returns the time left until the given timeout expires. It returns a null
     *   time if the timeout expired already.
     */
    base::Time remaining(base::Time timeout) const;

    /** @overloaded @deprecated */
    Timeout(unsigned int timeout);

    /** * @deprecated
     *
     * Use remaining instead
     */
    unsigned int timeLeft() const;

    /** @overloaded @deprecated */
    unsigned int timeLeft(unsigned int timeout) const;

    /** @overloaded @deprecated */
    bool elapsed(unsigned int timeout) const;

};

}

#endif

