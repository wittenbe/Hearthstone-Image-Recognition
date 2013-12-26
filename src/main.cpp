#include <string>
#include <iostream>
#include <stdio.h>
#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
//this include causes some errors with asio/winsock, at least on windows
//#include <boost/property_tree/json_parser.hpp>

#include "Stream.h"
#include "StreamManager.h"
#include "bot.h"
#include "SystemInterface.h"
#include "Config.h"

void botThread(clever_bot::botPtr& bot) {
	while (true) {
		std::cout << "connecting to IRC..." << std::endl;
		bot->loop();
		bot->connect();
		boost::this_thread::sleep_for(boost::chrono::seconds(10));
	}
}

int main(int argc, char* argv[]) {
    boost::property_tree::ptree cfg = Config::getConfig();
	bool live = cfg.get<bool>("config.stream.live");
	std::string streamer = cfg.get<std::string>("config.stream.streamer");
	bool connected = true;
	std::vector<std::string> addrs;

	clever_bot::botPtr bot = clever_bot::botPtr(new clever_bot::bot());

    hs::StreamManager sm(hs::StreamPtr(new hs::Stream(addrs)), bot);

    //define commands
	bot->add_read_handler([&bot, &sm](const std::string& m) {
		boost::regex regx(":([^!]++)!(\\S++)\\s++(\\S++)\\s++:?+(\\S++)\\s*+(?:[:+-]++(.*+))?");
		boost::smatch what;
		boost::regex_match(m, what, regx);

		if (what[2].length() != 0 && what[3] == "PRIVMSG") {
			std::string user = what[1];
			std::string cmdArgsString = what[5];
			std::vector<std::string> cmdArgs;
			boost::split(cmdArgs, cmdArgsString, boost::is_any_of(" "), boost::token_compress_on);

			const bool owner = bot->isowner(user);
			const bool allowed = owner || bot->isallowed(user);
			std::string response = sm.processCommand(user, cmdArgs, allowed, owner);
			if (!response.empty()) {
				bot->message(response);
			}
		}
	});

	bot->add_read_handler([&bot](const std::string& m) {
		if (m.find(":jtv ") == 0) {
			std::istringstream iss(m);
			std::string from, mode, channel, plus_o, user;
			iss >> from >> mode >> channel >> plus_o >> user;
			if (plus_o == "+o") {
				bot->allow_user(user);
			} else if (plus_o == "-o") {
				bot->unallow_user(user);
			}
		}
	});

	boost::thread botT(botThread, bot);

	if (!live) {
		const std::string execOutput = SystemInterface::callCurl(cfg.get<std::string>("config.stream.vod"));
		boost::property_tree::ptree pt;
		std::stringstream ss;
		ss << execOutput;
		boost::property_tree::read_xml(ss, pt);

	    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pt.get_child("archives")) {
	    	if (v.first == "archive") {
	            addrs.push_back(v.second.get<std::string>("transcode_file_urls.transcode_480p"));
//	    		addrs.push_back(v.second.get<std::string>("video_file_url"));
	    	}
	    }

	    sm.setStream(hs::StreamPtr(new hs::Stream(addrs)));
	    sm.startAsyn();
	    sm.wait();
	} else {
		while (true) {
			bool connected = true;
			const std::string execOutput = SystemInterface::callLivestreamer(streamer);
			if (execOutput.find("\"error\"") <= execOutput.length()) {
				std::cout << execOutput << std::endl;
				connected = false;
			} else {
				const std::string subStr = execOutput.substr(execOutput.find("http"), execOutput.npos);
				std::string streamUrl = subStr.substr(0, subStr.find("\""));
				addrs.clear();
				addrs.push_back(streamUrl);
				sm.setStream(hs::StreamPtr(new hs::Stream(addrs)));
			}

			std::cout << "connecting" << std::endl;
			if (connected) {
				std::cout << "connected to stream!" << std::endl;
			    sm.startAsyn();
			    sm.wait();
			}
			boost::this_thread::sleep_for(boost::chrono::seconds(60));
		}
	}

	return 0;
}
