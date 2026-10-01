// Tiny stand-ins for <fstream> and <sstream>.
//
// The real ones need libstdc++'s locale and file machinery, which a bare-metal
// kernel doesn't have. wopr_willy.cpp reads embedded assets through
// std::istringstream + std::getline (its std::ifstream fallbacks only run
// when an asset is missing, never here); highscores.cpp keeps its table in a
// file, which goes to the boot floppy (storage.cpp). These are explicit
// specializations of the <iosfwd> class templates for char with just the
// members that code uses; the primary templates are never defined.
#pragma once
#include <iosfwd>
#include <string>
#include <string.h>

namespace std _GLIBCXX_VISIBILITY(default) {

template<> class basic_ios<char, char_traits<char>> {
public:
    enum openmode_ { in = 1, out = 2, binary = 4, trunc = 8, app = 16, ate = 32 };
    enum seekdir_ { beg = 0, cur = 1, end = 2 };
};

// Files live on the boot floppy when there is one (storage.cpp); otherwise
// every open fails, as before. A file is read whole at open and written
// whole at close.
struct __shim_nobuf {};
} // namespace std
bool platform_file_load(const char* path, std::string& out);
bool platform_file_store(const char* path, const std::string& data);
namespace std _GLIBCXX_VISIBILITY(default) {

template<> class basic_ifstream<char, char_traits<char>> {
public:
    basic_ifstream() {}
    explicit basic_ifstream(const char* p, int = 0) { open(p); }
    explicit basic_ifstream(const string& p, int = 0) { open(p.c_str()); }
    void open(const char* p) { ok_ = open_ = platform_file_load(p, buf_); pos_ = 0; }
    bool good() const { return ok_; }
    bool is_open() const { return open_; }
    explicit operator bool() const { return ok_; }
    bool operator!() const { return !ok_; }
    void close() { open_ = false; }
    basic_ifstream& seekg(long off, int dir = 0) {
        long base = dir == 1 ? (long)pos_ : dir == 2 ? (long)buf_.size() : 0;
        long p = base + off;
        pos_ = p < 0 ? 0 : (size_t)p > buf_.size() ? buf_.size() : (size_t)p;
        return *this;
    }
    long tellg() { return ok_ ? (long)pos_ : -1; }
    basic_ifstream& read(char* d, long n) {
        size_t k = pos_ + n <= buf_.size() ? (size_t)n : buf_.size() - pos_;
        memcpy(d, buf_.data() + pos_, k);
        pos_ += k;
        if (k < (size_t)n) ok_ = false;
        return *this;
    }
    bool getline(string& line) {
        if (!ok_ || pos_ >= buf_.size()) { ok_ = false; return false; }
        size_t e = buf_.find('\n', pos_);
        if (e == string::npos) e = buf_.size();
        line.assign(buf_, pos_, e - pos_);
        pos_ = e + 1;
        return true;
    }
    __shim_nobuf rdbuf() { return {}; }
private:
    string buf_;
    size_t pos_ = 0;
    bool ok_ = false, open_ = false;
};

inline basic_ifstream<char>& getline(basic_ifstream<char>& in, string& line) { in.getline(line); return in; }

// highscores.cpp saves its table this way.
template<> class basic_ofstream<char, char_traits<char>> {
public:
    basic_ofstream() {}
    explicit basic_ofstream(const char* p, int = 0) { open(p); }
    explicit basic_ofstream(const string& p, int = 0) { open(p.c_str()); }
    basic_ofstream(const basic_ofstream&) = delete;
    ~basic_ofstream() { close(); }
    void open(const char* p) { close(); path_ = p; buf_.clear(); open_ = true; }
    bool good() const { return open_; }
    bool is_open() const { return open_; }
    explicit operator bool() const { return open_; }
    bool operator!() const { return !open_; }
    void close() {
        if (!open_) return;
        open_ = false;
        platform_file_store(path_.c_str(), buf_);
    }
    basic_ofstream& operator<<(const string& s) { buf_ += s; return *this; }
    basic_ofstream& operator<<(const char* s) { buf_ += s; return *this; }
    basic_ofstream& operator<<(char c) { buf_ += c; return *this; }
    basic_ofstream& operator<<(int v) { buf_ += to_string(v); return *this; }
    basic_ofstream& operator<<(long v) { buf_ += to_string(v); return *this; }
    basic_ofstream& operator<<(unsigned v) { buf_ += to_string(v); return *this; }
    basic_ofstream& operator<<(unsigned long v) { buf_ += to_string(v); return *this; }
private:
    string path_, buf_;
    bool open_ = false;
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
