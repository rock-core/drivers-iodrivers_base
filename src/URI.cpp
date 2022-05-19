#include <iodrivers_base/URI.hpp>
#include <stdexcept>

using namespace std;

using namespace iodrivers_base;

URI URI::parse(string const& uri) {
    size_t scheme_end = uri.find_first_of(":");
    size_t host_start = scheme_end + 3;
    if (scheme_end == string::npos || string(uri, scheme_end, 3) != "://") {
        throw invalid_argument("expected " + uri + " to start with SCHEME://");
    }
    string scheme = string(uri, 0, scheme_end);

    size_t host_end = uri.find_first_of("?:", host_start);
    string host = uri.substr(host_start, host_end - host_start);

    if (host_end == string::npos) {
        return URI(scheme, host, 0, Options());
    }

    int port = 0;
    size_t one_before_options = 0;
    if (uri[host_end] == ':') {
        size_t port_size;
        port = stoi(uri.substr(host_end + 1, string::npos), &port_size);
        size_t port_end = host_end + 1 + port_size;
        if (port_end == uri.size()) {
            one_before_options = string::npos;
        }
        else if (uri[port_end] == '?') {
            one_before_options = port_end;
        }
        else {
            throw std::invalid_argument("expected port field to be only numbers");
        }
    }
    else {
        one_before_options = host_end;
    }

    // Now parse the options. We don't do a full URI parsing: we do assume that
    // there are no question marks and ampersands in the option names and values
    Options options;
    while (one_before_options != string::npos) {
        size_t options_start = one_before_options + 1;
        size_t separator = uri.find_first_of("=", options_start);
        size_t options_end = uri.find_first_of("&", options_start);

        if (separator == string::npos || separator > options_end) {
            throw std::invalid_argument(
                "invalid options syntax in " + uri + ", expected key=value pairs "
                "separated by &"
            );
        }

        string key = uri.substr(options_start, separator - options_start);
        string value = uri.substr(separator + 1, options_end - separator - 1);
        options[key] = value;

        one_before_options = options_end;
    }

    return URI(scheme, host, port, options);
}

URI::URI()
    : m_port(0) {
}

URI::URI(string const& scheme, string const& host, int port, Options options)
    : m_scheme(scheme)
    , m_host(host)
    , m_port(port)
    , m_options(options) {
}

string URI::getScheme() const {
    return m_scheme;
}
string URI::getHost() const {
    return m_host;
}
int URI::getPort() const {
    return m_port;
}
URI::Options const& URI::getOptions() const {
    return m_options;
}
string URI::getOption(string const& key, string const& default_value) const {
    auto it = m_options.find(key);
    if (it == m_options.end()) {
        return default_value;
    }
    else {
        return it->second;
    }
}