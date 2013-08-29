#include <iodrivers_base/Bus.hpp>
#include <stdlib.h>

#include <boost/thread/locks.hpp>

using namespace iodrivers_base;

Parser::Parser(Bus *bus):
	bus(bus)
{
}

int Parser::readPacket(uint8_t* buffer, int buffer_size, int packet_timeout, int first_byte_timeout){
	return bus->readPacket(buffer,buffer_size, packet_timeout, first_byte_timeout,this);
}
    	
bool Parser::writePacket(uint8_t const* buffer, int bufsize, int timeout){
	return bus->writePacket(buffer,bufsize,timeout);
}

BusHandler::BusHandler(Bus *bus, bool auto_register):
	Parser(bus)
{
	if(auto_register){
		bus->addParser(this);
	}
}

BusHandler::~BusHandler(){
	bus->removeParser(this);
}

int BusHandler::readPacket(uint8_t* buffer, int buffer_size, int packet_timeout, int first_byte_timeout){
	return bus->readPacket(buffer,buffer_size,packet_timeout,first_byte_timeout);
}


Bus::Bus(int max_packet_size, bool extract_last):
	Driver(max_packet_size,extract_last)
{
	caller =0;

}

typedef boost::lock_guard<boost::recursive_mutex> LockGuard;

void Bus::addParser(Parser *parser){	
        LockGuard guard(mutex);
	this->parser.push_back(parser);
}

void Bus::removeParser(Parser *parser){
        LockGuard guard(mutex);
	this->parser.remove(parser);
}

bool Bus::writePacket(uint8_t const* buffer, int buffer_size, int timeout){
        LockGuard guard(mutex);
        return Driver::writePacket(buffer, buffer_size, timeout); 
}

int Bus::readPacket(uint8_t* buffer, int buffer_size, int packet_timeout, int first_byte_timeout, Parser *parser){
        LockGuard guard(mutex);

	caller = parser;
        try{
	    return Driver::readPacket(buffer, buffer_size, packet_timeout, first_byte_timeout);
        } catch(...) {
            caller = 0;
            throw;
        }
}


int Bus::extractPacket(uint8_t const* buffer, size_t buffer_size) const{
	if(caller){
		return caller->extractPacket(buffer,buffer_size);
	}

	int minSkip=buffer_size;

	for(std::list<Parser*>::const_iterator it = parser.begin();it != parser.end();it++){
		int tmp = (*it)->extractPacket(buffer,buffer_size);
		if(tmp > 0){
			BusHandler *handler = dynamic_cast<BusHandler*>(*it);	
			if(handler)
				handler->packedReady(buffer,minSkip);
		}
		if(abs(tmp) < minSkip){
			minSkip= abs(tmp);
		}
	}
	return minSkip;
}

