#ifndef __REQUEST_H
#define __REQUEST_H

#include <vector>
#include <functional>

using namespace std;

namespace ramulator
{

class Request
{
public:
    bool is_first_command;
    long addr;
    // long addr_row;
    vector<int> addr_vec;
    // specify which core this request sent from, for virtual address translation
    int coreid;

    enum class Type
    {
        READ,
        WRITE,
        REFRESH,
        POWERDOWN,
        SELFREFRESH,
        GWRITE, 
        G_ACT0, 
        G_ACT1, 
        G_ACT2, 
        G_ACT3,
        G_ACT4,
        G_ACT5,
        G_ACT6,
        G_ACT7,
        COMP, 
        READRES,
        EXTENSION,
        MAX
    } type;

    string type_name[int(Type::MAX)] = {
        "READ",
        "WRITE",
        "REFRESH",
        "POWERDOWN",
        "SELFREFRESH",
        "GWRITE", 
        "G_ACT0", 
        "G_ACT1", 
        "G_ACT2", 
        "G_ACT3",
        "G_ACT4",
        "G_ACT5",
        "G_ACT6",
        "G_ACT7", 
        "COMP", 
        "READRES",
        "EXTENSION"
    };

    long arrive = -1;
    long depart = -1;
    function<void(Request&)> callback; // call back with more info

    Request(long addr, Type type, int coreid = 0)
        : is_first_command(true), addr(addr), coreid(coreid), type(type),
      callback([](Request& req){}) {}

    Request(long addr, Type type, function<void(Request&)> callback, int coreid = 0)
        : is_first_command(true), addr(addr), coreid(coreid), type(type), callback(callback) {}

    Request(vector<int>& addr_vec, Type type, function<void(Request&)> callback, int coreid = 0)
        : is_first_command(true), addr_vec(addr_vec), coreid(coreid), type(type), callback(callback) {}

    Request()
        : is_first_command(true), coreid(0) {}
};

} /*namespace ramulator*/

#endif /*__REQUEST_H*/

