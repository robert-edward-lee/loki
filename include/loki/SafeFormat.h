////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005 by Andrei Alexandrescu
// Permission to use, copy, modify, distribute, and sell this software for any
//     purpose is hereby granted without fee, provided that the above copyright
//     notice appear in all copies and that both that copyright notice and this
//     permission notice appear in supporting documentation.
// The author makes no representations about the suitability of this software 
//     for any purpose. It is provided "as is" without express or implied 
//     warranty.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// This file contains definitions for SafePrintf. SafeScanf coming soon (the 
//   design is similar). 
// See Alexandrescu, Andrei: Type-safe Formatting, C/C++ Users Journal, Jul 2005
////////////////////////////////////////////////////////////////////////////////

#ifndef LOKI_SAFEFORMAT_H_
#define LOKI_SAFEFORMAT_H_

#include <cstdio>
#include <string>
#include <stdexcept>
#include <utility>
#include <cassert>
#include <locale>

// Crude writing method: writes straight to the file, unbuffered
// Must be combined with a buffer to work properly (and efficiently)

void write(std::FILE* f, const char* from, const char* to) {
    assert(from <= to);
    fwrite(from, 1, to - from, f);
}

// Write to a string

void write(std::string& s, const char* from, const char* to) {
    assert(from <= to);
    s.append(from, to);
}

// Write to a fixed-size buffer

template <class Char>
void write(std::pair<Char*, std::size_t>& s, const Char* from, const Char* to) {
    assert(from <= to);
    if (from + s.second > to) throw std::overflow_error("");
    s.first = copy(from, to, s.first);
    s.second -= to - from;
}

////////////////////////////////////////////////////////////////////////////////
// PrintfState class template
// Holds the formatting state, and implements operator() to format stuff
// Todo: make sure errors are handled properly
////////////////////////////////////////////////////////////////////////////////

template <class Device, class Char>
struct PrintfState {
    PrintfState(Device dev, const Char * format) 
            : device_(dev)
            , format_(format)
            , result_(0) {
        Advance();
    }
    
    ~PrintfState() {
    }

    #define LOKI_PRINTF_STATE_FORWARD(type) \
        PrintfState& operator()(type par) {\
            return (*this)(static_cast< unsigned long >(par)); \
        }

    LOKI_PRINTF_STATE_FORWARD(bool)
    LOKI_PRINTF_STATE_FORWARD(char)
    LOKI_PRINTF_STATE_FORWARD(signed char)
    LOKI_PRINTF_STATE_FORWARD(unsigned char)
    LOKI_PRINTF_STATE_FORWARD(short)
    LOKI_PRINTF_STATE_FORWARD(unsigned short)
    LOKI_PRINTF_STATE_FORWARD(int)
    LOKI_PRINTF_STATE_FORWARD(unsigned)
    LOKI_PRINTF_STATE_FORWARD(long)

    // Print (or gobble in case of the "*" specifier) an int
    PrintfState& operator()(unsigned long i) {
        if (result_ == -1) return *this; // don't even bother
        // % [flags] [width] [.prec] [modifier] type_char
        // Fetch the flags 
        ReadFlags();
        if (*format_ == '*') {
            // read the width and get out
            SetWidth(static_cast<size_t>(i));
            ++format_;
            return *this;
        }
        ReadWidth();
        // precision
        if (*format_ == '.') {
            // deal with precision
            if (format_[1] == '*') {
                // read the precision and get out
                SetPrec(static_cast<size_t>(i));
                format_ += 2;
                return *this;
            }
            ReadPrecision();
        }
        ReadModifiers();
        // input size modifier
        if (ForceShort()) {
            // short int
            const Char c = *format_;
            if (c == 'x' || c == 'X' || c == 'u' || c == 'o') {
                i = static_cast<unsigned long>(static_cast<unsigned short>(i));
            }
        }
        FormatWithCurrentFlags(i);
        return *this;
    }
    
    PrintfState& operator()(double n) {
        if (result_ == -1) return *this; // don't even bother
        PrintFloatingPoint(n);
        return *this;
    }

    PrintfState& operator()(long double n) {
        if (result_ == -1) return *this; // don't even bother
        PrintFloatingPoint(n);
        return *this;
    }

    // Store the number of characters printed so far
    PrintfState& operator()(int * pi) {
        return StoreCountHelper(pi);
    }
    
    // Store the number of characters printed so far
    PrintfState& operator()(short * pi) {
        return StoreCountHelper(pi);
    }
    
    // Store the number of characters printed so far
    PrintfState& operator()(long * pi) {
        return StoreCountHelper(pi);
    }
    
    PrintfState& operator()(const char *const s) {
        if (result_ == -1) return *this;
        ReadLeaders();
        const char fmt = *format_;
        if (fmt == 'p') {
            FormatWithCurrentFlags(reinterpret_cast<unsigned long>(s));
            return *this;
        }
        if (fmt != 's') {
            result_ = -1;
            return *this;
        }
        const size_t len = std::min(strlen(s), prec_);
        if (width_ > len) {
            if (LeftJustify()) {
                Write(s, s + len);
                Fill(' ', width_ - len);
            } else {
                Fill(' ', width_ - len);
                Write(s, s + len);
            }
        } else {
            Write(s, s + len);
        }
        Next();
        return *this;
    }
    
    PrintfState& operator()(const void *const p) {
        return (*this)(reinterpret_cast<unsigned long>(p));
    }
    
    // read the result
    operator int() const {
        return result_;
    }
    
private:
    template <typename T>
    PrintfState& StoreCountHelper(T *const pi) {
        if (result_ == -1) return *this; // don't even bother
        ReadLeaders();
        const char fmt = *format_;
        if (fmt == 'p') { // pointer
            FormatWithCurrentFlags(reinterpret_cast<unsigned long>(pi));
            return *this;
        }
        if (fmt != 'n') {
            result_ = -1;
            return *this;
        }
        assert(pi != 0);
        *pi = result_;
        Next();
        return *this;
    }

    void FormatWithCurrentFlags(const unsigned long i) {
        // look at the format character
        Char formatChar = *format_;
        bool isSigned = formatChar == 'd' || formatChar == 'i';
        if (formatChar == 'p') {
            formatChar = 'x'; // pointers go to hex
            SetAlternateForm(); // printed with '0x' in front
            isSigned = true; // that's what gcc does
        }
        if (!strchr("cdiuoxX", formatChar)) {
            result_ = -1;
            return;
        }
        Char buf[
            sizeof(unsigned long) * 3 // digits
            + 1 // sign or ' '
            + 2 // 0x or 0X
            + 1]; // terminating zero
        const Char *const bufEnd = buf + (sizeof(buf) / sizeof(Char));
        Char * bufLast = buf + (sizeof(buf) / sizeof(Char) - 1);
        Char signChar = 0;
        unsigned int base = 10;
        
        if (formatChar == 'c') {
            // Format only one character
            // The 'fill with zeros' flag is ignored
            ResetFillZeros();
            *bufLast = static_cast<char>(i);
        } else {
            // TODO: inefficient code, refactor
            const bool negative = isSigned && static_cast<long>(i) < 0;
            if (formatChar == 'o') base = 8;
            else if (formatChar == 'x' || formatChar == 'X') base = 16;
            bufLast = isSigned
                ? RenderWithoutSign(static_cast<long>(i), bufLast, base,
                    formatChar == 'X')
                : RenderWithoutSign(i, bufLast, base, 
                    formatChar == 'X');
            // Add the sign
            if (isSigned) {
                negative ? signChar = '-'
                : ShowSignAlways() ? signChar = '+'
                : Blank() ? signChar = ' '
                : 0;
            }
        }
        // precision 
        size_t 
            countDigits = bufEnd - bufLast,
            countZeros = prec_ != size_t(-1) && countDigits < prec_ && 
                    formatChar != 'c'
                ? prec_ - countDigits 
                : 0,
            countBase = base != 10 && AlternateForm() && i != 0
                ? (base == 16 ? 2 : countZeros > 0 ? 0 : 1)
                : 0,
            countSign = (signChar != 0),
            totalPrintable = countDigits + countZeros + countBase + countSign;
        size_t countPadLeft = 0, countPadRight = 0;
        if (width_ > totalPrintable) {
            if (LeftJustify()) {
                countPadRight = width_ - totalPrintable;
                countPadLeft = 0;
            } else {
                countPadLeft = width_ - totalPrintable;
                countPadRight = 0;
            }
        }
        if (FillZeros() && prec_ == size_t(-1)) {
            // pad with zeros and no precision - transfer padding to precision
            countZeros = countPadLeft;
            countPadLeft = 0;
        }
        // ok, all computed, ready to print to device
        Fill(' ', countPadLeft);
        if (signChar != 0) Write(&signChar, &signChar + 1);
        if (countBase > 0) Fill('0', 1);
        if (countBase == 2) Fill(formatChar, 1);
        Fill('0', countZeros);
        Write(bufLast, bufEnd);
        Fill(' ', countPadRight);
        // done, advance
        Next();
    }
    
    void Write(const Char* b, const Char* e) {
        if (result_ < 0) return;
        const ptrdiff_t x = e - b;
        write(device_, b, e);
        result_ += x;
    }

    template <class Double>
    void PrintFloatingPoint(Double n) {
        const Char *const fmt = format_ - 1;
        assert(*fmt == '%');
        // enforce format string validity
        ReadLeaders();
        // enforce format spec
        if (!strchr("eEfgG", *format_)) {
            result_ = -1;
            return;
        }
        // format char validated, copy it to a temp and use legacy sprintf
        ++format_;
        Char fmtBuf[128], resultBuf[1024];
        if (format_  >= fmt + sizeof(fmtBuf) / sizeof(Char)) {
            result_ = -1;
            return;
        }
        memcpy(fmtBuf, fmt, (format_ - fmt) * sizeof(Char));
        fmtBuf[format_ - fmt] = 0;
#ifdef _MSC_VER
        const int stored = _snprintf(resultBuf, 
#else
        const int stored = snprintf(resultBuf, 
#endif        
            sizeof(resultBuf) / sizeof(Char), fmtBuf, n);
        if (stored < 0) {
            result_ = -1;
            return;
        }
        Write(resultBuf, resultBuf + strlen(resultBuf));
        Advance(); // output stuff to the next format directive
    }

    void Fill(const Char c, size_t n) {
        for (; n > 0; --n) {
            Write(&c, &c + 1);
        }
    }
    
    Char* RenderWithoutSign(unsigned long n, char* bufLast, 
            unsigned int base, bool uppercase) {
        const Char hex1st = uppercase ? 'A' : 'a';
        for (;;) {
            const unsigned long next = n / base;
            Char c = n - next * base;
            c += (c <= 9) ? '0' : hex1st - 10;
            *bufLast = c;
            n = next;
            if (n == 0) break;
            --bufLast;
        }
        return bufLast;
    }
    
    char* RenderWithoutSign(long n, char* bufLast, unsigned int base, 
            bool uppercase) {
        if (n != LONG_MIN) {
            return RenderWithoutSign(static_cast<unsigned long>(n < 0 ? -n : n),
                bufLast, base, uppercase);            
        }
        // annoying corner case
        char* save = bufLast;
        ++n;
        bufLast = RenderWithoutSign(static_cast<unsigned long>(n),
            bufLast, base, uppercase);
        --(*save);
        return bufLast;
    }
    
    void Next() {
        ++format_;
        Advance();
    }
    
    void Advance() {
        ResetAll();
        const Char* begin = format_;
        for (;;) {
            if (*format_ == '%') { 
                if (format_[1] != '%') { // It's a format specifier
                    Write(begin, format_);
                    ++format_;
                    break;
                }
                // It's a "%%"
                Write(begin, ++format_);
                begin = ++format_;
                continue; 
            }
            if (*format_ == 0) {
                Write(begin, format_);
                break;
            }
            ++format_;
        }
    }
    
    void ReadFlags() {
        for (;; ++format_) {
            switch (*format_) {
                case '-': SetLeftJustify(); break;
                case '+': SetShowSignAlways(); break;
                case ' ': SetBlank(); break;
                case '#': SetAlternateForm(); break;
                case '0': SetFillZeros(); break;
                default: return;
            }
        }
    }
    
    void ParseDecimalUInt(unsigned int& dest) {
        if (!std::isdigit(*format_, std::locale())) return;
        unsigned int r = 0;
        do {
            // TODO: inefficient - rewrite
            r *= 10;
            r += *format_ - '0';
            ++format_;
        } while (std::isdigit(*format_, std::locale()));
        dest = r;
    }
    
    void ReadWidth() {
        ParseDecimalUInt(width_);
    }    
    
    void ReadPrecision() {
        assert(*format_ == '.');
        ++format_;
        ParseDecimalUInt(prec_);
    }    
    
    void ReadModifiers() {
        switch (*format_) {
            case 'h': SetForceShort(); ++format_; break;
            case 'l': ++format_; break;
            // more (C99 and platform-specific modifiers) to come
        }
    }
    
    void ReadLeaders() {
        ReadFlags();
        ReadWidth();
        if (*format_ == '.') ReadPrecision();
        ReadModifiers();
    }
    
    enum { 
        leftJustify = 1,
        showSignAlways = 2,
        blank = 4,
        alternateForm = 8,
        fillZeros = 16,
        forceShort = 32
    };
    
    bool LeftJustify() const { return (flags_ & leftJustify) != 0; }
    bool ShowSignAlways() const { return (flags_ & showSignAlways) != 0; }
    void SetWidth(size_t w) { width_  = w; }
    void SetLeftJustify() { flags_  |= leftJustify; }
    void SetShowSignAlways() { flags_ |= showSignAlways; }
    bool Blank() const { return (flags_ & blank) != 0; }
    bool AlternateForm() const { return (flags_ & alternateForm) != 0; }
    bool FillZeros() const { return (flags_ & fillZeros) != 0; }
    bool ForceShort() const { return (flags_ & forceShort) != 0; }

    void SetPrec(size_t p) { prec_ = p; }
    void SetBlank() { flags_ |= blank; }
    void SetAlternateForm() { flags_ |=  alternateForm; }
    void SetFillZeros() { flags_ |= fillZeros; }
    void ResetFillZeros() { flags_ &= ~fillZeros; }
    void SetForceShort() { flags_ |= forceShort; }
    
    void ResetAll() {
        assert(result_ != EOF);
        width_ = 0;
        prec_ = size_t(-1);
        flags_ = 0;
    }

    // state
    Device device_;
    const Char* format_;
    size_t width_;
    size_t prec_;
    unsigned int flags_;
    int result_;
};

PrintfState<std::FILE*, char> Printf(const char* format) {
    return PrintfState<std::FILE*, char>(stdout, format);
}

PrintfState<std::FILE*, char> FPrintf(FILE* f, const char* format) {
    return PrintfState<std::FILE*, char>(f, format);
}

PrintfState<std::string&, char> SPrintf(std::string& s, const char* format) {
    return PrintfState<std::string&, char>(s, format);
}

template <class T, class Char>
PrintfState<T&, Char> XPrintf(T& device, const Char* format) {
    return PrintfState<T&, Char>(device, format);
}

template <class Char, std::size_t N>
PrintfState<std::pair<Char*, std::size_t>, Char> 
BufPrintf(Char (&buf)[N], const Char* format) {
    std::pair<Char*, std::size_t> temp(buf, N);
    return PrintfState<std::pair<Char*, std::size_t>, Char>(temp, format);
}

#endif //SAFEFORMAT_H_
