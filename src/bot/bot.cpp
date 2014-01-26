#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <boost/foreach.hpp>
#include <boost/bind.hpp>

#include "connection.h"
#include "bot.h"
#include "../Config.h"

namespace clever_bot {

bot::bot()
{
	auto cfg = Config::getConfig();
	// -3 to be on the safe side
	m_max_msg = cfg.get<size_t>("config.twitch_bot.max_msg") - 3;
	m_timeframe = cfg.get<float>("config.twitch_bot.timeframe");

	m_uptime = boost::timer();

	m_conn.set_write_handler([this]() { this->write_handler(); });
	m_conn.set_read_handler([this](const std::string& m) {
		this->read_handler(m);
	});

	add_read_handler([this](const std::string& m) {
		std::istringstream iss(m);
		std::string type, from, rest;

		iss >> type >> from >> rest;
		if (type == "PING") {
			this->pong(from);
		}
	});

	m_owners.push_back("zeforte");

	connect();
}

void bot::connect() {
	auto cfg = Config::getConfig();
	m_conn.connect(cfg.get<std::string>("config.twitch_bot.server"), cfg.get<std::string>("config.twitch_bot.server_port"));

	pass(cfg.get<std::string>("config.twitch_bot.bot_pass"));
	nick(cfg.get<std::string>("config.twitch_bot.bot_nick"));

	std::string channel = cfg.get<std::string>("config.twitch_bot.channel");
	if (channel.empty()) {
		channel = "#" + cfg.get<std::string>("config.stream.streamer");
	}

	join(channel);
}

void bot::pong_handler(const std::string& message) {
	std::cout << message << std::endl;
}

void bot::write_handler()
{
	while (m_conn.alive()) {
        boost::unique_lock<boost::mutex> lock(m_mutex);

        while (m_unsent_commands.empty() && m_conn.alive()) {
            m_cond.wait(lock);
        }
        if (!m_conn.alive()) continue;

        //sort by timestamp
        std::sort(m_unsent_commands.begin(), m_unsent_commands.end(),
            [](const std::pair<std::string, double>& a, const std::pair<std::string, double>& b) {
                 return a.second < b.second;
            }
        );

    	//remove commands from history that are too far into the past
    	const float elapsed = m_uptime.elapsed();
    	while (m_sent_command_timestamps.size() > 0 && (elapsed - m_sent_command_timestamps.front()) > m_timeframe) {
    		m_sent_command_timestamps.pop_front();
    	}

    	//send as many commands as possible
    	size_t send_count = m_max_msg - m_sent_command_timestamps.size();
    	while (send_count > 0 && m_unsent_commands.size() > 0 && elapsed > m_unsent_commands.front().second) {
    		m_conn.write(m_unsent_commands.front().first);
    		m_sent_command_timestamps.push_back(m_uptime.elapsed());
    		m_unsent_commands.pop_front();
    		send_count--;
    	}

    	if (m_sent_command_timestamps.size() == 0 && m_unsent_commands.size() == 0 && m_uptime.elapsed() > 86400.0) {
    		m_uptime.restart(); //avoid overflows for very long running times
    	}
	}

	std::cout << "IRC connection died" << std::endl;
}

void bot::read_handler(const std::string& message)
{
	BOOST_FOREACH(boost::function<void (const std::string&)> func, m_read_handlers) {
		func(message);
	}
}

void bot::add_read_handler(boost::function<void (const std::string&)> func)
{
	m_read_handlers.push_back(func);
}

void bot::loop()
{
	m_conn.run();
}

void bot::pass(const std::string& pass) {
	queue_write(std::string("PASS ") + pass);
}

void bot::pong(const std::string& server) {
	queue_write(std::string("PONG ") + server);
}

void bot::nick(const std::string& nck)
{
	queue_write(std::string("NICK ") + nck);
	queue_write(std::string("USER ") + nck + " * * :" + nck);
}

void bot::join(const std::string& chann, std::string key)
{
	m_current_channel = chann;
	queue_write(std::string("JOIN ") + chann + " " + key);
}

void bot::queue_message(std::string receiver, std::string message, double delay)
{
	queue_write(std::string("PRIVMSG ") + receiver + " :" + message, delay);
}

void bot::message(std::string message, double delay)
{
    queue_message(m_current_channel, message, delay);
}

void bot::repeat_message(std::string message, size_t amount, double delta, double start_delay)
{
	for (size_t i = 0; i < amount; i++) {
		this->message(message, start_delay + i * delta);
	}
}

void bot::queue_write(const std::string& command, double delay)
{
	boost::unique_lock<boost::mutex> lock(m_mutex);

	m_unsent_commands.push_back(std::make_pair(command, delay + m_uptime.elapsed()));
	m_cond.notify_one();
}

void bot::quit(const std::string& message)
{
	queue_write(std::string("QUIT :") + message);
	m_conn.close();
}

void bot::allow_user(std::string user) {
	if (!is_contained(m_allowed_users, user)) {
		m_allowed_users.push_back(user);
	}
}

void bot::unallow_user(std::string user) {
	if (is_contained(m_allowed_users, user)) {
		m_allowed_users.erase(std::find(m_allowed_users.begin(), m_allowed_users.end(), user));
	}
}

bool bot::isallowed(std::string user) {
	return is_contained(m_allowed_users, user);
}

bool bot::isowner(std::string user) {
	return is_contained(m_owners, user);
}

} // ns clever_bot
