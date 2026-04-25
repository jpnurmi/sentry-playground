#ifndef SENTRYCRASH_H
#define SENTRYCRASH_H

class SentryCrash
{
public:
    static void segfault();
    static void stackOverflow();
    static void fastfail();
    static void failAssert();
    static void doAbort();
    static void throwException();
};

#endif // SENTRYCRASH_H
