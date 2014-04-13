#ifndef DATABASE_H_
#define DATABASE_H_

#include "PerceptualHash.h"
#include <boost/shared_ptr.hpp>
#include <boost/property_tree/ptree.hpp>

namespace hs {

struct Card {
	int id;
	std::string name;
	std::string heroClass;
	int cost;
	int quality;
	int type;
	ulong64 phash;
};

struct Hero {
	int id;
	std::string name;
	ulong64 phash;
};

class Database {
public:
    Database(const std::string& path);
    void save();

    std::vector<Card> cards;
    std::vector<Hero> heroes;
    bool hasMissingData(); //phashes for now

private:
    std::string path;
    boost::property_tree::ptree data;
};

typedef boost::shared_ptr<Database> DatabasePtr;

}

#endif /* DATABASE_H_ */
