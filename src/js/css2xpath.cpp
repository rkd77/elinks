// This code is based on github.com/theseer/css2xpath and other sources

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

#include "js/css2xpath.h"

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

	if (s.back() == delim)
	{
		result.push_back(std::string());
	}

	return result;
}

std::string
preg_replace(std::string & pattern, const char *replacement, std::string & subject)
{
	try {
		return std::regex_replace(subject, std::regex(pattern), replacement);
	} catch (const std::regex_error &e) {
		std::cout << e.what() << " " << pattern << "\n";
	}

	return subject;
}

using namespace std;

typedef std::string (*my_callback_t)(const std::smatch &m);

std::string
preg_replace_callback(std::string & pattern, my_callback_t callback, std::string & subject)
{
	return std::regex_replace2(subject, std::regex(pattern), callback);
}

std::string
dollar_equal_rule_callback(const std::smatch &matches)
{
	std::ostringstream os;
	os << "[substring(@" << matches[1].str() << ",string-length(@" << matches[1].str() << ")-";
	os << (matches[2].str().size() - 3) << ")=" << matches[1].str() << "]";
	return os.str();
}

std::string
dollar_equal_rule_apply(std::string &selector)
{
	std::string pattern("\\[([a-zA-Z0-9\\_\\-]+)\\$=([^\\]]+)\\]");

	return preg_replace_callback(pattern, dollar_equal_rule_callback, selector);
}

class Translator;

class Rule
{
	public:
		virtual std::string apply(std::string &selector)
		{
			return selector;
		}
	Rule()
	{
	}
};

class RegexRule : public Rule
{
	private:
		const char *pattern, *replacement;
	public:
		RegexRule(const char *pat, const char *repl) : pattern(pat), replacement(repl)
		{
		}
		std::string apply(std::string &selector)
		{
			std::string r(pattern);

			return preg_replace(r, replacement, selector);
		}
};

class NotRule : public Rule
{
	private:
		Translator *t;
	public:
		NotRule(Translator *tt);
		std::string apply(std::string &selector);
		std::string callback(const std::smatch &matches);
};

std::string
not_rule_callback(const std::smatch &m);

class NotRule *currentNotRule;


static bool
is_digits(const std::string &str)
{
	return std::all_of(str.begin(), str.end(), ::isdigit); // C++11
}

class NthChildRule;

class NthChildRule *currentNth;

std::string
nth_callback(const std::smatch &m);

class NthChildRule : public Rule
{
	public:
	NthChildRule()
	{
	}

	std::string apply(std::string & selector)
	{
		currentNth = this;
		std::string pat("([a-zA-Z0-9_\\-*]+):nth-child\\(([^)]*)\\)");
		return preg_replace_callback(pat, nth_callback, selector);
	}

	std::string callback(const std::smatch &matches)
	{
		std::ostringstream os;
		std::ostringstream res;
		os << matches[2];

		if (os.str() == "n")
		{
			res << matches[1];
		}
		else if (os.str() == "even")
		{
			res << matches[1] << "[(count(preceding-sibling::*) + 1) mod 2=0]";
		}
		else if (os.str() == "odd")
		{
			res << matches[1] << "[(count(preceding-sibling::*) + 1) mod 2=1]";
		}
		else if (is_digits(os.str()))
		{
			res << "*[" << matches[2] << "]/self::" << matches[1];
		}
		else
		{
			std::string pat("^([\\d]*)n.*?([\\d]*)$");
			std::string m = matches[2].str();
			std::string b1 = preg_replace(pat, "$1+$2", m);
			auto b = explode('+', b1);
			int bint = atoi(b[1].c_str());
			res << matches[1] << "[(count(preceding-sibling::*)+1)>=" << bint << " and ((count(preceding-sibling::*)+1)-"
			<< bint << ") mod " << b[0] << "=0]";
		}

		return res.str();
	}
};

std::string
nth_callback(const std::smatch &m)
{
	auto fun = std::bind(&NthChildRule::callback, ::currentNth, std::placeholders::_1);

	return fun(m);
}

std::string
not_rule_callback(const std::smatch &m)
{
	auto fun = std::bind(&NotRule::callback, ::currentNotRule, std::placeholders::_1);

	return fun(m);
}

class DollarEqualRule : public Rule
{
	public:
	DollarEqualRule()
	{
	}

	std::string apply(std::string &selector)
	{
		std::string pattern("\\[([a-zA-Z0-9\\_\\-]+)\\$=([^\\]]+)\\]");

		return preg_replace_callback(pattern, dollar_equal_rule_callback, selector);
	}
};

class Translator
{
	public:
		std::string translate(std::string & selector)
		{
			for (auto r : rules)
			{
				selector = r->apply(selector);
			}

			std::string quotes("\"\"");
			selector = preg_replace(quotes, "\"", selector);

			return selector == "/" ? "/" : ("//" + selector);
		}

	Translator()
	{
	}

private:
	Rule *rules[34] = {
		// prefix|name
		new RegexRule("([a-zA-Z0-9\\_\\-\\*]+)\\|([a-zA-Z0-9\\_\\-\\*]+)", "$1:$2"),

		// add @ for attribs
		new RegexRule("\\[([^G\\]~\\$\\*\\^\\|\\!]+)(=[^\\]]+)?\\]", "[@$1$2]"),

		// multiple queries
		new RegexRule("\\s*,\\s*", "|"),
		// , + ~ >
		new RegexRule("\\s*([\\+~>])\\s*", "$1"),

		//* ~ + >
		new RegexRule("([a-zA-Z0-9\\_\\-\\*])~([a-zA-Z0-9\\_\\-\\*])", "$1/following-sibling::$2"),
		new RegexRule("([a-zA-Z0-9\\_\\-\\*])\\+([a-zA-Z0-9\\_\\-\\*])", "$1/following-sibling::*[1]/self::$2"),
		new RegexRule("([a-zA-Z0-9\\_\\-\\*])>([a-zA-Z0-9\\_\\-\\*])", "$1/$2"),
		// all unescaped stuff escaped
		new RegexRule("\\[([^=]+)=([^'|][^\\]]*)\\]", "[$1=\"$2\"]"),
		// all descendant or self to //
		new RegexRule("(^|[^a-zA-Z0-9\\_\\-\\*])([#\\.])([a-zA-Z0-9\\_\\-]+)", "$1*$2$3"),
		new RegexRule("([\\>\\+\\|\\~\\,\\s])([a-zA-Z\\*]+)", "$1//$2"),
		new RegexRule("\\s+\\/\\/", "//"),

		// :first-child
		new RegexRule("([a-zA-Z0-9\\_\\-\\*]+):first-child", "*[1]/self::$1"),

		// :last-child
		new RegexRule("([a-zA-Z0-9\\_\\-\\*]+)?:last-child", "$1[not(following-sibling::*)]"),

		// :only-child
		new RegexRule("([a-zA-Z0-9\\_\\-\\*]+):only-child", "*[last()=1]/self::$1"),

		// :empty
		new RegexRule("([a-zA-Z0-9\\_\\-\\*]+)?:empty", "$1[not(*) and not(normalize-space())]"),

		// :not
		new NotRule(this),

		// :nth-child
		new NthChildRule(),

		// :contains(selectors)
		new RegexRule(":contains\\(([^\\)]*)\\)", "[contains(string(.),\"$1\")]"),

		// |= attrib
		new RegexRule("\\[([a-zA-Z0-9\\_\\-]+)\\|=([^\\]]+)\\]", "[@$1=$2 or starts-with(@$1,concat($2,\"-\"))]"),

		// *= attrib
		new RegexRule("\\[([a-zA-Z0-9\\_\\-]+)\\*=([^\\]]+)\\]", "[contains(@$1,$2)]"),

		// ~= attrib
		new RegexRule("\\[([a-zA-Z0-9\\_\\-]+)~=([^\\]]+)\\]", "[contains(concat(\" \",normalize-space(@$1),\" \"),concat(\" \",$2,\" \"))]"),

		// ^= attrib
		new RegexRule("\\[([a-zA-Z0-9\\_\\-]+)\\^=([^\\]]+)\\]", "[starts-with(@$1,$2)]"),

		// $= attrib
		new DollarEqualRule(),

		// != attrib
		new RegexRule("\\[([a-zA-Z0-9\\_\\-]+)\\!=[\\\"']+?([^\\\"\\]]+)[\\\"']+?\\]", "[not(@$1) or @$1!=\"$2\"]"),

		// ids
		new RegexRule("#([a-zA-Z0-9\\_\\-]+)", "[@id=\"$1\"]"),

		// classes
		new RegexRule("\\.([a-zA-Z0-9_-]+)(?![^[]*])", "[contains(concat(\" \",normalize-space(@class),\" \"),\" $1 \")]"),

		// normalize multiple filters
		new RegexRule("\\]\\[([^\\]]+)", " and ($1)"),

		// tag:pseudo selectors
		new RegexRule("(:enabled)", "[not(@disabled)]"),
		new RegexRule("(:checked)", "[@checked=\"checked\"]"),
		new RegexRule(":(disabled)", "[@$1]"),
		new RegexRule(":root", "/"),
		new RegexRule(":scope", ""),

		// use * when tag was omitted
		new RegexRule("^\\[", "*["),
		new RegexRule("\\|\\[", "|*[")
	};
};

NotRule::NotRule(Translator *tt) : t(tt)
{
}

std::string
NotRule::apply(std::string &selector)
{
	currentNotRule = this;
	std::string pat("([a-zA-Z0-9\\_\\-\\*]+):not\\(([^\\)]*)\\)");
	return preg_replace_callback(pat, not_rule_callback, selector);
}

std::string
NotRule::callback(const std::smatch &matches)
{
	std::string m(matches[2].str());
	std::string pat("^[^\\[]+\\[([^\\]]*)\\].*$");
	std::string ret(t->translate(m));
	std::string subresult = preg_replace(pat, "$1", ret);
	return matches[1].str() + "[not(" + subresult + ")]";
}

std::string
css2xpath(std::string &selector)
{
	static Translator *translator;

	if (!translator)
	{
		translator = new Translator();
	}
	return translator->translate(selector);
}

#if 0

std::string
next_year(const std::smatch& matches)
{
	std::ostringstream os;

	os << matches[1] << atoi(matches[2].str().c_str()) + 1;
	return os.str();
}

typedef const char *test_t[2];

void
tests_t()
{
	test provider[] = {
            {"div", "//div"},
            {"body div", "//body//div"},
            {"div p", "//div//p"},
            {"div > p", "//div/p"},
            {"div + p", "//div/following-sibling::*[1]/self::p"},
            {"div ~ p", "//div/following-sibling::p"},
            {"div p a", "//div//p//a"},
            {"div, p, a", "//div|//p|//a"},
            {".note", "//*[contains(concat(\" \",normalize-space(@class),\" \"),\" note \")]"},
            {"div.example", "//div[contains(concat(\" \",normalize-space(@class),\" \"),\" example \")]"},
            {"ul .tocline2", "//ul//*[contains(concat(\" \",normalize-space(@class),\" \"),\" tocline2 \")]"},
            {"div.example, div.note", "//div[contains(concat(\" \",normalize-space(@class),\" \"),\" example \")]|//div[contains(concat(\" \",normalize-space(@class),\" \"),\" note \")]"},
            {"#title", "//*[@id=\"title\"]"},
            {"h1#title", "//h1[@id=\"title\"]"},
            {"div #title", "//div//*[@id=\"title\"]"},
            {"ul.toc li.tocline2", "//ul[contains(concat(\" \",normalize-space(@class),\" \"),\" toc \")]//li[contains(concat(\" \",normalize-space(@class),\" \"),\" tocline2 \")]"},
            {"ul.toc > li.tocline2", "//ul[contains(concat(\" \",normalize-space(@class),\" \"),\" toc \")]/li[contains(concat(\" \",normalize-space(@class),\" \"),\" tocline2 \")]"},
            {"h1#title + div > p", "//h1[@id=\"title\"]/following-sibling::*[1]/self::div/p"},
            {"h1[id]:contains(Selectors)", "//h1[@id and (contains(string(.),\"Selectors\"))]"},
            {"a[href][lang][class]", "//a[@href and (@lang) and (@class)]"},
            {"div[class]", "//div[@class]"},
            {"div[class=example]", "//div[@class=\"example\"]"},
            {"div[class^=exa]", "//div[starts-with(@class,\"exa\")]"},
            {"div[class$=mple]", "//div[substring(@class,string-length(@class)-3)=class]"},
            {"div[class*=e]", "//div[contains(@class,\"e\")]"},
            {"div[class|=dialog]", "//div[@class=\"dialog\" or starts-with(@class,concat(\"dialog\",\"-\"))]"},
            {"div[class!=made_up]", "//div[not(@class) or @class!=\"made_up\"]"},
            {"div[property!=\"made_up\"]", "//div[not(@property) or @property!=\"made_up\"]"},
            {"div[class~=example]", "//div[contains(concat(\" \",normalize-space(@class),\" \"),concat(\" \",\"example\",\" \"))]"},
            {"div:not(.example)", "//div[not(contains(concat(\" \",normalize-space(@class),\" \"),\" example \"))]"},
            {"p:contains(selectors)", "//p[contains(string(.),\"selectors\")]"},
            {"p:nth-child(n)", "//p"},
            {"p:nth-child(even)", "//p[(count(preceding-sibling::*) + 1) mod 2=0]"},
            {"p:nth-child(odd)", "//p[(count(preceding-sibling::*) + 1) mod 2=1]"},
            {"p:nth-child(3n+8)", "//p[(count(preceding-sibling::*)+1)>=8 and ((count(preceding-sibling::*)+1)-8) mod 3=0]"},
            {"p:nth-child(2n+1)", "//p[(count(preceding-sibling::*)+1)>=1 and ((count(preceding-sibling::*)+1)-1) mod 2=0]"},
            {"p:nth-child(3)", "//*[3]/self::p"},
            {"p:nth-child(4n)", "//p[(count(preceding-sibling::*)+1)>=0 and ((count(preceding-sibling::*)+1)-0) mod 4=0]"},
            {"p:only-child", "//*[last()=1]/self::p"},
            {"p:last-child", "//p[not(following-sibling::*)]"},
            {"p:first-child", "//*[1]/self::p"},
            {"foo|bar", "//foo:bar"},
            {"div[class^=exa][class$=mple]", "//div[starts-with(@class,\"exa\") and (substring(@class,string-length(@class)-3)=class)]"},

            {"input:enabled", "//input[not(@disabled)]"},
            {"input:checked", "//input[@checked=\"checked\"]"},
            {"input:disabled", "//input[@disabled]"},

            {":empty", "//*[not(*) and not(normalize-space())]"},
            {":root", "/"},
            {"meta[name=\"gaf\"]", "//meta[@name=\"gaf\"]"}
	};

	for (auto t: provider)
	{
		std::string selector(t[0]);
		std::string expected(t[1]);

		std::string result = css2xpath(selector);
		std::cout << t[0] << " ";
		std::cout << ((result == expected) ? "\033[32mOK\033[0m" : "\033[31mFAIL\033[0m");
		std::cout << "\n";
	}
}

int
main(int argc, char **argv)
{
#if 0
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
#endif

#if 0
	auto v = explode('+', "4+");

	for (auto e : v)
	{
		std::cout << e << "\n";
	}
#endif

	tests();

	return 0;
}
#endif
