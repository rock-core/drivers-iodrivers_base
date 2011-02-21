#include "iodrivers_bus.hh"
#include <stdlib.h>



IOParser::IOParser(IOBus *bus):
	bus(bus)
{
}

int IOParser::readPacket(uint8_t* buffer, int buffer_size, int packet_timeout, int first_byte_timeout){
	return bus->readPacket(buffer,buffer_size, packet_timeout, first_byte_timeout,this);
}
    	
bool IOParser::writePacket(uint8_t const* buffer, int bufsize, int timeout){
	return bus->writePacket(buffer,bufsize,timeout);
}

IOBusHandler::IOBusHandler(IOBus *bus, bool auto_register):
	IOParser(bus)
{
	if(auto_register){
		bus->addParser(this);
	}
}

IOBusHandler::~IOBusHandler(){
	bus->removeParser(this);
}

int IOBusHandler::readPacket(uint8_t* buffer, int buffer_size, int packet_timeout, int first_byte_timeout){
	return bus->readPacket(buffer,buffer_size,packet_timeout,first_byte_timeout);
}


IOBus::IOBus(int max_packet_size, bool extract_last):
	IODriver(max_packet_size,extract_last){
	caller =0;

}

void IOBus::addParser(IOParser *parser){	
	this->parser.push_back(parser);
}

void IOBus::removeParser(IOParser *parser){
	this->parser.remove(parser);
}

int IOBus::readPacket(uint8_t* buffer, int buffer_size, int packet_timeout, int first_byte_timeout, IOParser *parser){
	caller = parser;
	int erg= IODriver::readPacket(buffer, buffer_size, packet_timeout, first_byte_timeout);
	caller = 0;
	return erg;
}


int IOBus::extractPacket(uint8_t const* buffer, size_t buffer_size) const{
	if(caller){
		return caller->extractPacket(buffer,buffer_size);
	}

	int minSkip=buffer_size;

	for(std::list<IOParser*>::const_iterator it = parser.begin();it != parser.end();it++){
		int tmp = (*it)->extractPacket(buffer,buffer_size);
		if(tmp > 0){
			IOBusHandler *handler = dynamic_cast<IOBusHandler*>(*it);	
			if(handler)
				handler->packedReady(buffer,minSkip);
		}
		if(abs(tmp) < minSkip){
			minSkip= abs(tmp);
		}
	}
	return minSkip;
}

