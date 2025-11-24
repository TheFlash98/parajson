#include <iostream>
#include "simdjson.h"
using namespace simdjson;
int main(void) {
    ondemand::parser parser;
    padded_string json = padded_string::load("sample.json");
    ondemand::document tweets = parser.iterate(json);
    std::cout << uint64_t(tweets["age"]) << " results." << std::endl;
    
}