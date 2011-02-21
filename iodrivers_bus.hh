#ifndef _IOBUS_HH_
#define _IOBUS_HH_

#include <iodrivers_base.hh>
#include <list>
#include <inttypes.h>

class IOBus;

/**
 * This Class implements an Parser, classes they inherit this, are able to "dock" on an IOBus.
 * Its used for e.g. for RS-485 Communication busses, there may different devices on one bus.
 * Its not pratcicable to wrote an huge extract_packed for this, each device can with this clas
 * implement it's own extractor.
 */
class IOParser{
public:
	/*
	 * Give the Constructer the bus, which it belongs to
	 */
	IOParser(IOBus *bus);

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
	IOBus *bus;
};


/**
 * This class Extends the IOParser, this class should be inherit for all devices that send periodicly 
 * data to the bus, and are not request->answer based. 
 * In case an Parser detects an packed for this Device, the function packedReady get's call IF this class is registered to
 * the IOBus.
 */
class IOBusHandler : public IOParser{
public:
	/**
	 * Give the bus to which this Device belogns, auto_registration is done by the second parameter
	 */
	IOBusHandler(IOBus *bus, bool auto_register=true);
	~IOBusHandler();
	
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
 * Bus Version of the IODriver, to this class can "dock" IOBusHandler or IOParser classes
 * If you have Periodic devices you should use IOBusHandler, otherwise IOParser, for more Details
 * See the Classes.
 */
class IOBus : public IODriver{
public:
	IOBus(int max_packet_size, bool extract_last = false);
	int readPacket(uint8_t* buffer, int buffer_size, int packet_timeout, int first_byte_timeout=-1, IOParser *parser=0);
	void addParser(IOParser *parser);
	void removeParser(IOParser *parser);
	int extractPacket(uint8_t const* buffer, size_t buffer_size) const;
protected:
	std::list<IOParser*> parser;
	IOParser *caller;
};

#endif
