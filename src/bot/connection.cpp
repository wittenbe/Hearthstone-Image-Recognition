#include "connection.h"
#include "../Logger.h"

#include <stdexcept>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

namespace clever_bot {

//using boost::asio::ip::tcp;

void connection::connect() 
{
	boost::asio::ip::tcp::resolver resolver(m_io_service);
	boost::asio::ip::tcp::resolver::query query(m_addr, m_port);
	boost::system::error_code error = boost::asio::error::host_not_found;
	boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query);
	
	boost::asio::ip::tcp::resolver::iterator end;

	while (iter != end) {
		if (!error) {
			break;
		}
		
		m_socket.close();
		HS_INFO << "Trying to connect: " + m_addr + ":" + m_port << std::endl;
		
		m_socket.connect(*iter++, error);
		
		if (error) {
			HS_ERROR << error.message() << std::endl;
		}
	}
	
	if (alive()) {
		HS_INFO << "Connected!" << std::endl;
	} else {
		HS_WARNING << "Could not connect to " + m_addr + ":" + m_port << std::endl;
	}
}

void connection::run()
{
	boost::thread write_handler_thread(m_write_handler);
	m_read_thread = boost::thread(boost::bind(&boost::asio::io_service::run, &m_io_service));

	while (alive()) {
		boost::asio::read_until(m_socket, m_buffer, "\r\n");

	    std::string line;
	    std::istream is(&m_buffer);
	    std::getline(is, line);
	    line = line.substr(0, line.length() - 1); //delete last character, i.e. \n
		m_read_handler(line);
	}

	write_handler_thread.join();
}

bool connection::alive() const
{
	return m_socket.is_open();
}

void connection::close()
{
	m_socket.close();
	m_io_service.stop();
}

void connection::connect(const std::string& addr, const std::string& port)
{
	m_addr = addr;
	m_port = port;
	
	connect();
}

void connection::write(const std::string& content)
{
	HS_INFO_NOSPACE << "[Write] " << content << std::endl;
	boost::asio::write(m_socket, boost::asio::buffer(content + "\r\n"));
}

void connection::set_read_handler(const read_handler_type& handler)
{
	m_read_handler = handler;
}

void connection::set_write_handler(const write_handler_type& handler)
{
	m_write_handler = handler;
}

} // ns clever_bot
