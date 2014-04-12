#ifndef SYSTEMINTERFACE_H_
#define SYSTEMINTERFACE_H_

#include "Config.h"

#include <string>
#include <iostream>
#include <fstream>
#include <stdio.h>

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <cv.h>

#define FORMAT_ACCESS_TOKEN "http://api.twitch.tv/api/channels/%s/access_token"
#define FORMAT_USHER "\"http://usher.twitch.tv/select/%s.xml?nauth=%s&nauthsig=%s&allow_source=true&type=any&private_code=\""

class SystemInterface {
public:
	static std::string exec(const std::string& cmd) {
	    FILE* pipe = popen(cmd.c_str(), "r");
	    if (!pipe) return "ERROR";
	    char buffer[512];
	    std::string result = "";
	    while(!feof(pipe)) {
	    	if(fgets(buffer, 512, pipe) != NULL)
	    		result += buffer;
	    }
	    pclose(pipe);
	    return result;
	}

	static std::string callCurl(const std::string& params) {
		std::string curl(Config::getConfig().get<std::string>("config.paths.curl_path") + " -s ");
		return exec(curl + params);
	}

	static std::string callLivestreamer(std::string streamer) {
		static std::string quality = Config::getConfig().get<std::string>("config.stream.stream_quality");
		static std::string livestreamer(Config::getConfig().get<std::string>("config.paths.livestreamer_path") + " -j ");
		return exec(livestreamer + "twitch.tv/" + streamer + " " + quality);
	}

	static std::string createPastebin(const std::string& text, const std::string& key) {
		std::string str = text;
		boost::replace_all(str, "\n", "%0d");

		std::string params = "--data-binary \"api_dev_key=" + key + "&api_option=paste&api_paste_code=" + str + "\" http://pastebin.com/api/api_post.php";
		return callCurl(params);
	}

	static std::string createHastebin(const std::string& text) {
		std::ofstream temp;
		//since transmitting line breaks is difficult(/impossible?)
		temp.open("temp.txt");
		temp << text;
		temp.close();

		std::string params = "--data-binary \"@temp.txt\" hastebin.com/documents";
		std::string keyValue = callCurl(params);

		std::string response;
		if (keyValue.length() >= 8 && keyValue.find_last_of("/\"") >= 8 && keyValue.find("false") >= keyValue.length()) {
			response = "http://hastebin.com/" + keyValue.substr(8, keyValue.find_last_of("/\"") - 8) + ".hs";
		}

		return response;
	}

	static std::string createStrawpoll(const std::string& text, const std::vector<std::string>& choices, const bool& multi = true) {
		std::string params = "\"title=" + text;
		for (auto& s : choices) {
			params += "&options%5B%5D=" + s;
		}
		params += "&multi=";
		params += (multi)? "true" : "false";
		params += "\"";

		std::string curlParams = "-d " + params + " http://strawpoll.me/ajax/new-poll";
		std::string keyValue = callCurl(curlParams);

		std::string response;
		if (keyValue.length() >= 6 && keyValue.find_last_of("}") >= 6 && keyValue.find("false") >= keyValue.length()) {
			response = "http://strawpoll.me/" + keyValue.substr(6, keyValue.find_last_of("}") - 6);
		}

		return response;
	}

	static std::string createImgur(const cv::Mat& image, std::string key = "b3625162d3418ac51a9ee805b1840452") {
		saveImage(image, "temp.png");
		std::string curlParams = "curl -F \"key=" + key + "\" -F \"image=@temp.png\" http://imgur.com/api/upload.xml";
		std::string keyValue = callCurl(curlParams);
		boost::property_tree::ptree pt;
		std::stringstream ss; ss << keyValue;
		boost::property_tree::read_xml(ss, pt);

		std::string result;
		result = pt.get<std::string>("rsp.original_image", result);
		return result;
	}

	static void saveImage(const cv::Mat& image, std::string name) {
		cv::imwrite(name, image);
	}

	static std::string getStreamURL(const std::string& streamer, const std::string& quality) {
		std::string result;
		std::string accessToken = callCurl((boost::format(FORMAT_ACCESS_TOKEN) % streamer).str());
		if (accessToken.empty()) return result;
		if (accessToken.find("html") < accessToken.length()) return result;
		boost::property_tree::ptree pt;
		std::stringstream ss; ss << accessToken;
		boost::property_tree::read_json(ss, pt);

		std::string token = pt.get<std::string>("token");
		std::string sig = pt.get<std::string>("sig");
		token = urlencode(token);

		std::string usher = (boost::format(FORMAT_USHER) % streamer % token % sig).str();
		std::string m3u8 = SystemInterface::callCurl(usher);
		auto resultMap = getStreamURLsFromM3U8(m3u8);
		auto it = resultMap.find(quality);
		if (it != resultMap.end()) {
			result = it->second;
		}
		return result;
	}

private:
	static std::map<std::string, std::string> getStreamURLsFromM3U8(const std::string& m3u8) {
		static const std::string nameHeuristic = "NAME=\"";

		std::map<std::string, std::string> m;
		std::istringstream is(m3u8);

		std::string line;
		std::string name;

		int streamUrl = -1;
		while (std::getline(is, line)) {
			size_t heuristicStart = line.find(nameHeuristic);
			if (heuristicStart <= line.length() && streamUrl < 0) {
				size_t nameStart = heuristicStart + nameHeuristic.length();
				name = line.substr(nameStart, std::string::npos);
				name = name.substr(0, name.find("\""));
				streamUrl = 2;
			}
			if (streamUrl == 0) {
				m[name] = line;
			}

			streamUrl--;
		}
		return m;
	}

	static std::string char2hex(char dec) {
	    char dig1 = (dec&0xF0)>>4;
	    char dig2 = (dec&0x0F);
	    if ( 0<= dig1 && dig1<= 9) dig1+=48;    //0,48inascii
	    if (10<= dig1 && dig1<=15) dig1+=97-10; //a,97inascii
	    if ( 0<= dig2 && dig2<= 9) dig2+=48;
	    if (10<= dig2 && dig2<=15) dig2+=97-10;

	    std::string r;
	    r.append( &dig1, 1);
	    r.append( &dig2, 1);
	    return r;
	}

	static std::string urlencode(const std::string &c)
	{

		std::string escaped="";
	    int max = c.length();
	    for(int i=0; i<max; i++)
	    {
	        if ( (48 <= c[i] && c[i] <= 57) ||//0-9
	             (65 <= c[i] && c[i] <= 90) ||//abc...xyz
	             (97 <= c[i] && c[i] <= 122) || //ABC...XYZ
	             (c[i]=='~' || c[i]=='!' || c[i]=='*' || c[i]=='(' || c[i]==')' || c[i]=='\'' || c[i]=='_')
	        )
	        {
	            escaped.append( &c[i], 1);
	        }
	        else
	        {
	            escaped.append("%");
	            escaped.append( char2hex(c[i]) );//converts char 255 to string "ff"
	        }
	    }
	    return escaped;
	}
};

#endif /* SYSTEMINTERFACE_H_ */
