//#include <iostream>
//#include <cstdlib>
//#include <chrono>
//#include <ctime>
//#include "bot.h"
//
//using namespace std;
//
//int main()
//{
//	try {
//		// Starts a new cleverbot with the variables set in the file 'config'
//		clever_bot::bot bot("config");
//		bot.join("#trumpsc");
//
//		// Read handlers example (will be improved soon)
//		bot.add_read_handler([&bot](const std::string& m) {
//			std::istringstream iss(m);
//			std::string from, type, to, msg;
//
//			iss >> from >> type >> to >> msg;
//
//			if (msg == ":!time") {
//			}
//		});
//
//		bot.add_read_handler([&bot](const std::string& m) {
//			std::istringstream iss(m);
//			std::string from, type, to, msg, text;
//
//			iss >> from >> type >> to >> msg;
//
//			if (msg == ":!echo") {
//				text = "";
//				while ((iss >> msg)) {
//					text += msg + " ";
//				}
//
//				if (text != "") {
//					bot.message(to, text);
//				}
//			}
//		});
//
//		bot.add_read_handler([&bot](const std::string& m) {
//			std::istringstream iss(m);
//			std::ostringstream oss;
//			std::string from, type, to, msg;
//
//			iss >> from >> type >> to >> msg;
//
//			if (msg == ":!rand") {
//				int mx, ans = std::rand();
//
//				if (iss >> mx) {
//					ans = ans % mx;
//				}
//
//				oss << ans;
//				bot.message(to, from + ": " + oss.str());
//			}
//		});
//
//		// Main execution
//		bot.loop();
//	}
//	catch (const std::exception& e) {
//		std::cout << e.what() << std::endl;
//	}
//
//	return 0;
//}
