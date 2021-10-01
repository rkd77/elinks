#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include <utility>
#include <regex>
#include <functional>
#include <cstdlib>
#include <iostream>

#include <cstdlib>
#include <iostream>
#include <string>
#include <regex>

namespace std
{

template<class BidirIt, class Traits, class CharT, class UnaryFunction>
std::basic_string<CharT> regex_replace2(BidirIt first, BidirIt last,
    const std::basic_regex<CharT,Traits>& re, UnaryFunction f)
{
    std::basic_string<CharT> s;

    typename std::match_results<BidirIt>::difference_type
        positionOfLastMatch = 0;
    auto endOfLastMatch = first;

    auto callback = [&](const std::match_results<BidirIt>& match)
    {
        auto positionOfThisMatch = match.position(0);
        auto diff = positionOfThisMatch - positionOfLastMatch;

        auto startOfThisMatch = endOfLastMatch;
        std::advance(startOfThisMatch, diff);

        s.append(endOfLastMatch, startOfThisMatch);
        s.append(f(match));

        auto lengthOfMatch = match.length(0);

        positionOfLastMatch = positionOfThisMatch + lengthOfMatch;

        endOfLastMatch = startOfThisMatch;
        std::advance(endOfLastMatch, lengthOfMatch);
    };

    std::regex_iterator<BidirIt> begin(first, last, re), end;
    std::for_each(begin, end, callback);

    s.append(endOfLastMatch, last);

    return s;
}

template<class Traits, class CharT, class UnaryFunction>
std::string regex_replace2(const std::string& s,
    const std::basic_regex<CharT,Traits>& re, UnaryFunction f)
{
    return regex_replace2(s.cbegin(), s.cend(), re, f);
}

} // namespace std


using namespace std;

std::string
implode(const char *const delim, std::vector<std::string> & x)
{
	switch (x.size())
	{
		case 0:
			return std::string("");
		case 1:
			return x[0];
		default:
			std::ostringstream os;
			std::copy(x.begin(), x.end() - 1,
				std::ostream_iterator<std::string>(os, delim));
			os << *x.rbegin();
			return os.str();
	}
}

std::vector<std::string>
explode(char delim, std::string const & s)
{
	std::vector<std::string> result;
	std::istringstream iss(s);

	for (std::string token; std::getline(iss, token, delim); )
	{
		result.push_back(std::move(token));
	}

	return result;
}

std::string
preg_replace(std::string & pattern, const char *replacement, std::string & subject)
{
	return std::regex_replace(subject, std::regex(pattern), replacement);
}

using namespace std;

typedef std::string (*my_callback)(const std::smatch &m);


std::string
preg_replace_callback(std::string & pattern, my_callback callback, std::string & subject)
{
	return std::regex_replace2(subject, std::regex(pattern), callback);
}

#if 0

std::string
next_year(const std::smatch& matches)
{
	std::ostringstream os;

	os << matches[1] << atoi(matches[2].str().c_str()) + 1;
	return os.str();
}

int
main(int argc, char **argv)
{
#if 1
	std::vector<std::string> x;

	for (int i = 2; i < argc; i++) {
		x.push_back(argv[i]);
	}
	std::string res = implode(argv[1], x);
	std::cout << res << "\n";
#endif
#if 1
	auto v = explode(' ', "hello world foo bar");

	for (auto e : v)
	{
		std::cout << e << "\n";
	}
#endif

#if 1
	std::string str = "April 15, 2003";
	std::string pattern = "(\\w+) (\\d+), (\\d+)";
	const char *replacement = "$1,1,$3";
	std::cout << preg_replace(pattern, replacement, str);
#endif

	std::string text = "April fools day is 04/01/2002\n";
	text += "Last christmas was 12/24/2001\n";

	std::string pat2 = "(\\d{2}/\\d{2}/)(\\d{4})";

	std::cout << preg_replace_callback(pat2, next_year, text);

	return 0;
}
#endif
