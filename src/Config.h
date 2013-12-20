#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <fstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#define CONFIG_PATH "/../config.xml" //relative to src

class Config {
public:
    static boost::property_tree::ptree& getConfig()
    {
        static boost::property_tree::ptree instance;
        if (instance.empty()) {
            std::ifstream cfgFile(std::string(HSIR_BASE_DIR CONFIG_PATH).c_str());
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
