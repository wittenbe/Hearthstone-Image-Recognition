#ifndef CONFIG_H
#define CONFIG_H

#include "Logger.h"

#include <string>
#include <fstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

//relative to executable
#define CONFIG_PATH "../config.xml"

class Config {
public:
    static boost::property_tree::ptree& getConfig() {
        static boost::property_tree::ptree instance;
        if (instance.empty()) {
            std::ifstream cfgFile(std::string(CONFIG_PATH).c_str());
            if (cfgFile.fail()) {
            	HS_ERROR << "Could not find config file at " << CONFIG_PATH << std::endl;
            }
            boost::property_tree::read_xml(cfgFile, instance);
        }
        return instance;
    }

private:
    Config() {}
    Config(Config const&);
    void operator=(Config const&);
};

#endif // CONFIG_H
