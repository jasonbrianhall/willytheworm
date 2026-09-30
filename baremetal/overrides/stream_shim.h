// Tiny stand-ins for <fstream> and <sstream>.
//
// The real ones need libstdc++'s locale and file machinery, which a bare-metal
// kernel doesn't have. wopr_willy.cpp only reads embedded assets through
// std::istringstream + std::getline, and only falls back to std::ifstream
// when an embedded asset is missing (never, here). So these are explicit
// specializations of the <iosfwd> class templates for char with just the
// members that code uses; the primary templates are never defined.
#pragma once
#include <iosfwd>
#include <string>

namespace std _GLIBCXX_VISIBILITY(default) {

template<> class basic_ios<char, char_traits<char>> {
public:
    enum openmode_ { in = 1, out = 2, binary = 4, trunc = 8, app = 16, ate = 32 };
    enum seekdir_ { beg = 0, cur = 1, end = 2 };
};

// No filesystem: every file open fails and reads nothing.
struct __shim_nobuf {};
template<> class basic_ifstream<char, char_traits<char>> {
public:
    basic_ifstream() {}
    explicit basic_ifstream(const char*, int = 0) {}
    explicit basic_ifstream(const string&, int = 0) {}
    bool good() const { return false; }
    bool is_open() const { return false; }
    explicit operator bool() const { return false; }
    void close() {}
    basic_ifstream& seekg(long, int = 0) { return *this; }
    long tellg() { return 0; }
    basic_ifstream& read(char*, long) { return *this; }
    __shim_nobuf rdbuf() { return {}; }
};

inline basic_ifstream<char>& getline(basic_ifstream<char>& in, string&) { return in; }

// Writes go nowhere (highscores.cpp saves its table this way; on bare metal
// the table simply lives in RAM until reboot).
template<> class basic_ofstream<char, char_traits<char>> {
public:
    basic_ofstream() {}
    explicit basic_ofstream(const char*, int = 0) {}
    explicit basic_ofstream(const string&, int = 0) {}
    bool good() const { return false; }
    bool is_open() const { return false; }
    explicit operator bool() const { return false; }
    void close() {}
    template<class T> basic_ofstream& operator<<(const T&) { return *this; }
};

template<> class basic_istringstream<char, char_traits<char>, allocator<char>> {
public:
    explicit basic_istringstream(const string& s) : buf_(s) {}
    bool getline(string& line) {
        if (pos_ >= buf_.size()) { fail_ = true; return false; }
        size_t e = buf_.find('\n', pos_);
        if (e == string::npos) e = buf_.size();
        line.assign(buf_, pos_, e - pos_);
        pos_ = e + 1;
        return true;
    }
    explicit operator bool() const { return !fail_; }
private:
    string buf_;
    size_t pos_ = 0;
    bool fail_ = false;
};

inline basic_istringstream<char>& getline(basic_istringstream<char>& in, string& line) {
    in.getline(line);
    return in;
}

template<> class basic_stringstream<char, char_traits<char>, allocator<char>> {
public:
    basic_stringstream& operator<<(__shim_nobuf) { return *this; }
    basic_stringstream& operator<<(const string& s) { buf_ += s; return *this; }
    string str() const { return buf_; }
private:
    string buf_;
};

} // namespace std
