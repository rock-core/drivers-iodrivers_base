#include "iodrivers_bus.hh"
#include <stdlib.h>



IOParser::IOParser(IOBus *bus):
	bus(bus)
{
}

int IOParser::readPacket(uint8_t* buffer, int buffer_size, int packet_timeout, int first_byte_timeout){
        int value;
	value = bus->readPacket(buffer,buffer_size, packet_timeout, first_byte_timeout,this);
        return value; 
}
    	
bool IOParser::writePacket(uint8_t const* buffer, int bufsize, int timeout){
        int value;
	value = bus->writePacket(buffer,bufsize,timeout);
        return value;
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
	IODriver(max_packet_size,extract_last),
        mutex(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
{
	caller =0;

}

void IOBus::addParser(IOParser *parser){	
        pthread_mutex_lock(&mutex);
	this->parser.push_back(parser);
        pthread_mutex_unlock(&mutex);
}

void IOBus::removeParser(IOParser *parser){
        pthread_mutex_lock(&mutex);
	this->parser.remove(parser);
        pthread_mutex_unlock(&mutex);
}

bool IOBus::writePacket(uint8_t const* buffer, int buffer_size, int timeout){
        pthread_mutex_lock(&mutex);
        bool erg;
        try{
            erg = IODriver::writePacket(buffer, buffer_size, timeout); 
        }catch(timeout_error e){
            pthread_mutex_unlock(&mutex);
            throw;
        }
        pthread_mutex_unlock(&mutex);
        return erg;
}

int IOBus::readPacket(uint8_t* buffer, int buffer_size, int packet_timeout, int first_byte_timeout, IOParser *parser){
        int value;
        pthread_mutex_lock(&mutex);
	caller = parser;
        int erg=0;
        try{
	    erg= IODriver::readPacket(buffer, buffer_size, packet_timeout, first_byte_timeout);
        }catch(timeout_error e){
            caller = 0;
            pthread_mutex_unlock(&mutex);
            throw;
        }
	caller = 0;
        pthread_mutex_unlock(&mutex);
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

