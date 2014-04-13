#include "Database.h"
#include "Logger.h"

#include <boost/property_tree/xml_parser.hpp>

namespace hs {

Database::Database(const std::string& path) {
	this->path = path;
	std::ifstream dataFile(path);
	boost::property_tree::read_xml(dataFile, data, boost::property_tree::xml_parser::trim_whitespace);
	size_t cardCount = data.get_child("hs_data.cards").size();
	size_t heroCount = data.get_child("hs_data.heroes").size();
	HS_INFO << "Database contains [" << cardCount << "] cards and [" << heroCount << "] heroes" << std::endl;
	cards = std::vector<Card>(cardCount);
	heroes = std::vector<Hero>(heroCount);

    for (auto& v : data.get_child("hs_data.cards")) {
    	if (v.first == "entry") {
    		Card c;
    		c.id = v.second.get<int>("ID");
            c.name = v.second.get<std::string>("name");
            c.heroClass = v.second.get<std::string>("class");
            c.cost = v.second.get<int>("cost");
            c.quality = v.second.get<int>("quality");
            c.type = v.second.get<int>("type");
            c.phash = v.second.get<ulong64>("phash", 0);
            cards[c.id] = c;
    	}
    }

    for (auto& v : data.get_child("hs_data.heroes")) {
    	if (v.first == "entry") {
    		Hero h;
    		h.id = v.second.get<int>("ID");
            h.name = v.second.get<std::string>("name");
            h.phash = v.second.get<ulong64>("phash", 0);
            heroes[h.id] = h;
    	}
    }
}

void Database::save() {
	HS_INFO << "Saving database" << std::endl;
    for (auto& v : data.get_child("hs_data.cards")) {
    	if (v.first == "entry") {
            int id = v.second.get<int>("ID");
            v.second.put("phash", cards[id].phash);
    	}
    }

    for (auto& v : data.get_child("hs_data.heroes")) {
    	if (v.first == "entry") {
            int id = v.second.get<int>("ID");
            v.second.put("phash", cards[id].phash);
    	}
    }

    boost::property_tree::xml_writer_settings<char> settings('\t', 1);
    write_xml(path, data, std::locale(), settings);
}

bool Database::hasMissingData() {
	return cards.size() > 0 && cards[0].phash == 0;
}

}
