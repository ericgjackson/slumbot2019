#ifndef _SPLIT_H_
#define _SPLIT_H_

#include <string>
#include <vector>

void Split(const char *line, char sep, bool allow_empty, std::vector<std::string> *comps);
void ParseDoubles(const std::string &s, std::vector<double> *values);
void ParseInts(const std::string &s, std::vector<int> *values);
void ParseUnsignedInts(const std::string &s, std::vector<unsigned int> *values);

#endif
