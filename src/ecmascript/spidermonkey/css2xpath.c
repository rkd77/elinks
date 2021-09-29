#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include <utility>

std::string
implode(const char *const delim, std::vector<std::string> x)
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

#if 0
#include <iostream>
int
main(int argc, char **argv)
{
#if 0
	std::vector<std::string> x;

	for (int i = 2; i < argc; i++) {
		x.push_back(argv[i]);
	}
	std::string res = implode(argv[1], x);
	std::cout << res << "\n";
#endif
	auto v = explode(' ', "hello world foo bar");

	for (auto e : v)
	{
		std::cout << e << "\n";
	}

	return 0;
}
#endif
