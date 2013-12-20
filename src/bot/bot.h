#ifndef BOT_H
#define BOT_H

#include <map>
#include <deque>
#include <string>
#include <functional>
#include "connection.h"
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/timer.hpp>
#include <boost/thread.hpp>

namespace clever_bot {

class bot
{
public:
	bot();
	void pong(const std::string& server);
	void pass(const std::string& pass);
	void nick(const std::string& nck);
	void join(const std::string& chann, std::string key = "");
	void queue_message(std::string receiver, std::string message, double delay = 0);
	void repeat_message(std::string message, size_t amount, double delta, double start_delay = 0.0);
	void message(std::string message, double delay = 0);
	void quit(const std::string& message);
	void queue_write(const std::string& command, double delay = 0);
	void loop();
	void connect();
	
	void allow_user(std::string user);
	void unallow_user(std::string user);
	bool isallowed(std::string user);

	template <class T>
	bool is_contained(std::vector<T> vec, T elem) {
		auto elem_ptr = std::find(vec.begin(), vec.end(), elem);
		return elem_ptr != vec.end();
	}

	void add_read_handler(boost::function<void (const std::string&)>);
protected:
	void write_handler();
	void read_handler(const std::string& message);
	void pong_handler(const std::string& message);
	
	std::vector<boost::function<void (const std::string&)> > m_read_handlers;
	connection m_conn;

	//info about current state
	std::string m_current_channel;
	boost::timer m_uptime;

	//allowed user handling
	std::vector<std::string> m_allowed_users;

	//command handling
	std::deque<double> m_sent_command_timestamps;
	std::deque<std::pair<std::string, double> > m_unsent_commands;
    boost::mutex m_mutex;
    boost::condition_variable m_cond;
	size_t m_max_msg;
	float m_timeframe;

};

typedef boost::shared_ptr<bot> botPtr;

} // ns clever_bot

#endif
