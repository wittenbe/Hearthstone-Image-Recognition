#include <stdexcept>
#include <functional>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include "logger.h"
#include "connection.h"

namespace clever_bot {

//using boost::asio::ip::tcp;

void connection::connect() 
{
	boost::asio::ip::tcp::resolver resolver(m_io_service);
	boost::asio::ip::tcp::resolver::query query(m_addr, m_port);
	boost::system::error_code error = boost::asio::error::host_not_found;
	auto iter = resolver.resolve(query);
	
	decltype(iter) end;

	while (iter != end) {
		if (!error) {
			break;
		}
		
		m_socket.close();
		LOG("Info", "Trying to connect: " + m_addr + ":" + m_port);
		
		m_socket.connect(*iter++, error);
		
		if (error) {
			LOG("ERROR", error.message());
		}
	}
	
	if (error) {
		throw std::runtime_error(error.message());
	}
	
	LOG("Info", "Connected!");
}

void connection::run()
{
	boost::thread write_handler_thread(m_write_handler);

	boost::asio::async_read_until(m_socket, m_buffer, '\n',
			boost::bind(&connection::read, this, _1)
	);
//	m_io_service.run();
	m_read_thread = boost::thread(boost::bind(&boost::asio::io_service::run, &m_io_service));

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
	LOG("Write", content);
	boost::asio::write(m_socket, boost::asio::buffer(content + "\r\n"));
}

void connection::read(const boost::system::error_code& error)
{
	if (error) {
		LOG("Error", "Some error occured");
		close();
	}
	else {
	    std::string line;
	    std::istream is(&m_buffer);
	    std::getline(is, line);
	    std::cout << line << std::endl;
	    line = line.substr(0, line.length() - 1); //delete last character, i.e. \n
		m_read_handler(line);

		boost::asio::async_read_until(m_socket, m_buffer, '\n',
				boost::bind(&connection::read, this, _1)
		);
	}
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
