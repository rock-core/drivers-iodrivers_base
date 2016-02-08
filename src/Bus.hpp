#ifndef IODRIVERS_BASE_BUS_HH
#define IODRIVERS_BASE_BUS_HH

#include <iodrivers_base/Driver.hpp>
#include <list>
#include <inttypes.h>
#include <boost/thread/recursive_mutex.hpp>

namespace iodrivers_base {
class Bus;

/**
 * This Class implements an Parser, classes they inherit this, are able to "dock" on an IOBus.
 * Its used for e.g. for RS-485 Communication busses, there may different devices on one bus.
 * Its not pratcicable to wrote an huge extract_packed for this, each device can with this clas
 * implement it's own extractor.
 */
class Parser{
public:
	/*
	 * Give the Constructer the bus, which it belongs to
	 */
	Parser(Bus *bus);
	virtual ~Parser(){};

	/**
	 * read packed calls the readPacked from IOBus, if this readPacked is used only the extract packed
	 * from this Device is called, other device-extractors are not involved, to no call to packedReady 
	 * (see IOBusHandler) is done. If youre devices not request->answer like you need to use the IOBusHandler,
	 * and call the readPacked from IOBus.
	 */
	virtual int readPacket(uint8_t* buffer, int buffer_size, int packet_timeout, int first_byte_timeout=-1);
	/**
	 * Decription see IODriver
	 */
	virtual int extractPacket(uint8_t const* buffer, size_t buffer_size) const = 0;
	/**
	 * See IODriver
	 */
    	bool writePacket(uint8_t const* buffer, int bufsize, int timeout);
protected:
	Bus *bus;
};


/**
 * This class Extends the Parser, this class should be inherit for all devices that send periodicly 
 * data to the bus, and are not request->answer based. 
 * In case an Parser detects an packed for this Device, the function packedReady get's call IF this class is registered to
 * the Bus.
 */
class BusHandler : public Parser{
public:
	/**
	 * Give the bus to which this Device belogns, auto_registration is done by the second parameter
	 */
	BusHandler(Bus *bus, bool auto_register=true);
	~BusHandler();
	
	/**
	 * see IODriver
	 */
	virtual void packedReady(uint8_t const* buffer, size_t size)=0;

	/**
	 * See IODriver
	 */
	virtual int readPacket(uint8_t* buffer, int buffer_size, int packet_timeout, int first_byte_timeout=-1);
};


/**
 * Bus Version of the IODriver, to this class can "dock" IOBusHandler or Parser classes
 * If you have Periodic devices you should use IOBusHandler, otherwise Parser, for more Details
 * See the Classes.
 */
class Bus : public Driver{
public:
	Bus(int max_packet_size, bool extract_last = false);
	int readPacket(uint8_t* buffer, int buffer_size, int packet_timeout, int first_byte_timeout=-1, Parser *parser=0);
	void addParser(Parser *parser);
	void removeParser(Parser *parser);
	int extractPacket(uint8_t const* buffer, size_t buffer_size) const;
        bool writePacket(uint8_t const* buffer, int buffer_size, int timeout);
protected:
	std::list<Parser*> parser;
	Parser *caller;
        boost::recursive_mutex mutex;
};
}

#endif

