#ifndef __GDDR6_H
#define __GDDR6_H

#include "DRAM.h"
#include "Request.h"
#include <vector>
#include <functional>

using namespace std;

namespace ramulator
{

class GDDR6
{
public:
    static string standard_name;
    enum class Org;
    enum class Speed;
    GDDR6(Org org, Speed speed);
    GDDR6(const string& org_str, const string& speed_str);

    static map<string, enum Org> org_map;
    static map<string, enum Speed> speed_map;

    /*** Level ***/
    enum class Level : int
    {
        Channel, Rank, BankGroup, Bank, Row, Column, MAX
    };

    static std::string level_str [int(Level::MAX)];

    /*** Command ***/
    enum class Command : int
    {
        ACT, PRE, PREA,
        RD,  WR,  RDA,  WRA,
        REF, PDE, PDX,  SRE, SRX,
        GWRITE, G_ACT0, G_ACT1, G_ACT2, G_ACT3, G_ACT4, G_ACT5, G_ACT6, G_ACT7, COMP, READRES, // for Newton
        MAX
    };

    string command_name[int(Command::MAX)] = {
        "ACT", "PRE", "PREA",
        "RD",  "WR",  "RDA",  "WRA",
        "REF", "PDE", "PDX",  "SRE", "SRX",
        "GWRITE", "G_ACT0", "G_ACT1", "G_ACT2", "G_ACT3", "G_ACT4", "G_ACT5", "G_ACT6", "G_ACT7", "COMP", "READRES" // for Newton
    };

    Level scope[int(Command::MAX)] = {
        Level::Row,    Level::Bank,   Level::Rank,
        Level::Column, Level::Column, Level::Column, Level::Column,
        Level::Rank,   Level::Rank,   Level::Rank,   Level::Rank,   Level::Rank,
        Level::Rank,   Level::BankGroup, Level::BankGroup, Level::BankGroup, Level::BankGroup, Level::BankGroup, Level::BankGroup, Level::BankGroup, Level::BankGroup, Level::Column, Level::Bank // for Newton
    };
    bool is_BG(Level level)
    {
        return false;
    }
    bool is_pim_opening(Command cmd)
    {
        switch(int(cmd)) {
            case int(Command::G_ACT0):
            case int(Command::G_ACT1):
            case int(Command::G_ACT2):
            case int(Command::G_ACT3):
            case int(Command::G_ACT4):
            case int(Command::G_ACT5):
            case int(Command::G_ACT6):
            case int(Command::G_ACT7):
                return true;
            default:
                return false;
        }
    }
    bool is_pim_accessing(Command cmd)
    {
        switch(int(cmd)) {
            case int(Command::COMP):
                return true;
            default:
                return false;
        }
    }
    bool is_opening(Command cmd)
    {
        switch(int(cmd)) {
            case int(Command::ACT):
            case int(Command::G_ACT0):
            case int(Command::G_ACT1):
            case int(Command::G_ACT2):
            case int(Command::G_ACT3):
            case int(Command::G_ACT4):
            case int(Command::G_ACT5):
            case int(Command::G_ACT6):
            case int(Command::G_ACT7):
                return true;
            default:
                return false;
        }
    }

    bool is_accessing(Command cmd)
    {
        switch(int(cmd)) {
            case int(Command::RD):
            case int(Command::WR):
            case int(Command::RDA):
            case int(Command::WRA):
            case int(Command::COMP):
            //TODO : case int(Command::GWRITE): have to include GWRITE to is_accessing()?
            //TODO : case int(Command::READRES): have to include READRES to is_accessing()?
                return true;
            default:
                return false;
        }
    }

    bool is_closing(Command cmd)
    {
        switch(int(cmd)) {
            case int(Command::RDA):
            case int(Command::WRA):
            case int(Command::PRE):
            case int(Command::PREA):
                return true;
            default:
                return false;
        }
    }

    bool is_refreshing(Command cmd)
    {
        switch(int(cmd)) {
            case int(Command::REF):
                return true;
            default:
                return false;
        }
    }


    /* State */
    enum class State : int
    {
        Opened, Closed, PowerUp, ActPowerDown, PrePowerDown, SelfRefresh, MAX
    } start[int(Level::MAX)] = {
        State::MAX, State::PowerUp, State::MAX, State::Closed, State::Closed, State::MAX
    };

    /* Translate */
    Command translate[int(Request::Type::MAX)] = {
        Command::RD,  Command::WR,
        Command::REF, Command::PDE, Command::SRE,
        Command::GWRITE, Command::G_ACT0, Command::G_ACT1, Command::G_ACT2, Command::G_ACT3, Command::G_ACT4, Command::G_ACT5, Command::G_ACT6, Command::G_ACT7, Command::COMP, Command::READRES // for Newton
    };

    /* Prerequisite */
    function<Command(DRAM<GDDR6>*, Command cmd, int)> prereq[int(Level::MAX)][int(Command::MAX)];

    // SAUGATA: added function object container for row hit status
    /* Row hit */
    function<bool(DRAM<GDDR6>*, Command cmd, int)> rowhit[int(Level::MAX)][int(Command::MAX)];
    function<bool(DRAM<GDDR6>*, Command cmd, int)> rowopen[int(Level::MAX)][int(Command::MAX)];

    /* Timing */
    struct TimingEntry
    {
        Command cmd;
        int dist;
        int val;
        bool sibling;
    };
    vector<TimingEntry> timing[int(Level::MAX)][int(Command::MAX)];

    /* Lambda */
    function<void(DRAM<GDDR6>*, int)> lambda[int(Level::MAX)][int(Command::MAX)];

    /* Organization */
    enum class Org : int
    {
        GDDR6_512Mb_x16, GDDR6_512Mb_x32,
        GDDR6_1Gb_x16,   GDDR6_1Gb_x32,
        GDDR6_2Gb_x16,   GDDR6_2Gb_x32,
        GDDR6_4Gb_x16,   GDDR6_4Gb_x32,
        GDDR6_8Gb_x16,   GDDR6_8Gb_x32,
        MAX
    };

    struct OrgEntry {
        int size;
        int dq;
        int count[int(Level::MAX)];
    } org_table[int(Org::MAX)] = {
        // fixed to have 1 rank
        // in GDDR6 the column address is unique for a burst. e.g. 64 column addresses correspond with
        // 256 column addresses actually. So we multiply 8 to the original address bit number in JEDEC standard
        {  512, 16, {0, 1, 4, 2, 1<<12, 1<<(7+3)}}, {  512, 32, {0, 1, 4, 2, 1<<12, 1<<(6+3)}},
        {1<<10, 16, {0, 1, 4, 4, 1<<12, 1<<(7+3)}}, {1<<10, 32, {0, 1, 4, 4, 1<<12, 1<<(6+3)}},
        {2<<10, 16, {0, 1, 4, 4, 1<<13, 1<<(7+3)}}, {2<<10, 32, {0, 1, 4, 4, 1<<13, 1<<(6+3)}},
        {4<<10, 16, {0, 1, 4, 4, 1<<14, 1<<(7+3)}}, {2<<10, 32, {0, 1, 4, 4, 1<<14, 1<<(6+3)}},
        {8<<10, 16, {0, 1, 8, 4, 1<<14, 1<<(8+3)}}, {8<<10, 32, {0, 1, 4, 4, 1<<14, 1<<(7+3)}}
    }, org_entry;

    void set_channel_number(int channel);
    void set_rank_number(int rank);

    /* Speed */
    enum class Speed : int
    {
        GDDR6_3500,
        GDDR6_4000, GDDR6_4500,
        GDDR6_5000, GDDR6_5500,
        GDDR6_6000, GDDR6_6500,
        GDDR6_7000,
        MAX
    };

    int prefetch_size = 16; // 8n prefetch QDR
    int channel_width = 64;

    struct SpeedEntry {
        int rate;
        double freq, tCK;
        int nBL, nCCDS, nCCDL;
        int nCL, nRCDR, nRCDW, nRP, nCWL;
        int nRAS, nRC;
        int nPPD, nRTP, nWTR, nWR;
        int nRRD, nFAW, n32AW;
        int nRFC, nREFI;
        int nPD, nXPN, nLK;
        int nCKESR, nXS, nXSDLL;
    } speed_table[int(Speed::MAX)] = {
        // scaling by comparing: GDDR6 (https://github.com/umd-memsys/DRAMsim3/blob/master/configs/GDDR6_8Gb_x16.ini) vs GDDR5 (https://github.com/umd-memsys/DRAMsim3/blob/master/configs/GDDR5_8Gb_x32.ini)
        {3500,
        7*500/4, 8.0/7,
        2, 2, 3,
        11, 11, 9, 11, 7,
        25, 35, // nRC
        1, 2, 4, 8,
        5, 1, 188,
        0, 0,
        10, 10, 0,
        0, 0, 0},
        {4000,
        8*500/4, 8.0/8,
        2, 2, 3,
        12, 12, 10, 12, 3,
        28, 40,
        1, 2, 5, 12,
        6, 23, 184,
        0, 0,
        10, 10, 0,
        0, 0, 0},
        {4500,  9*500/4,  8.0/9, 2, 2, 3, 14, 14, 12, 14, 4, 32, 46, 2, 2, 6, 14,  7, 26, 207, 0, 0, 10, 10, 0, 0, 0, 0},
        {5000, 10*500/4, 8.0/10, 2, 2, 3, 15, 15, 13, 15, 4, 35, 50, 2, 2, 7, 15,  7, 29, 230, 0, 0, 10, 10, 0, 0, 0, 0},
        {5500, 11*500/4, 8.0/11, 2, 2, 3, 17, 17, 14, 17, 5, 39, 56, 2, 2, 7, 17,  8, 32, 253, 0, 0, 10, 10, 0, 0, 0, 0},
        {6000, 12*500/4, 8.0/12, 2, 2, 3, 18, 18, 15, 18, 5, 42, 60, 2, 2, 8, 18,  9, 35, 276, 0, 0, 10, 10, 0, 0, 0, 0},
        {6500, 13*500/4, 8.0/13, 2, 2, 3, 20, 20, 17, 20, 5, 46, 66, 2, 2, 9, 20,  9, 38, 299, 0, 0, 10, 10, 0, 0, 0, 0},
        {7000,
        14*500/4, 8.0/14,
        2, 2, 3,
        21, 21, 18, 21, 6,
        49, 70,
        2, 2, 9, 21,
        10, 41, 322,
        0, 0,
        10, 10, 0,
        0, 0, 0}
    }, speed_entry;

    int read_latency;
    int readres_latency;

private:
    void init_speed();
    void init_lambda();
    void init_prereq();
    void init_rowhit();  // SAUGATA: added function to check for row hits
    void init_rowopen();
    void init_timing();
};

} /*namespace ramulator*/

#endif /*__GDDR6_H*/
