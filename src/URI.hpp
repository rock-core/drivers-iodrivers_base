#ifndef IODRIVERS_BASE_URI_HPP
#define IODRIVERS_BASE_URI_HPP

#include <string>
#include <map>

namespace iodrivers_base {
    class URI {
    public:
        typedef std::map<std::string, std::string> Options;

    private:
        std::string m_scheme;
        std::string m_host;
        int m_port;

        Options m_options;

    public:
        static URI parse(std::string const& string);

        URI();
        URI(std::string const& scheme, std::string const& host, int port, Options options);

        std::string getScheme() const;
        std::string getHost() const;
        int getPort() const;
        Options const& getOptions() const;
        std::string getOption(std::string const& key,
                              std::string const& defaultValue = "") const;
    };

    std::string to_string(URI const& uri);
}

#endif