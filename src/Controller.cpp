#include "Controller.h"
#include "SALP.h"
#include "ALDRAM.h"
#include "GDDR6.h"
//#include "TLDRAM.h"

using namespace ramulator;

namespace ramulator
{

static vector<int> get_offending_subarray(DRAM<SALP>* channel, vector<int> & addr_vec){
    int sa_id = 0;
    auto rank = channel->children[addr_vec[int(SALP::Level::Rank)]];
    auto bank = rank->children[addr_vec[int(SALP::Level::Bank)]];
    auto sa = bank->children[addr_vec[int(SALP::Level::SubArray)]];
    for (auto sa_other : bank->children)
        if (sa != sa_other && sa_other->state == SALP::State::Opened){
            sa_id = sa_other->id;
            break;
        }
    vector<int> offending = addr_vec;
    offending[int(SALP::Level::SubArray)] = sa_id;
    offending[int(SALP::Level::Row)] = -1;
    return offending;
}


template <>
vector<int> Controller<SALP>::get_addr_vec(SALP::Command cmd, list<Request>::iterator req){
    if (cmd == SALP::Command::PRE_OTHER)
        return get_offending_subarray(channel, req->addr_vec);
    else
        return req->addr_vec;
}


template <>
bool Controller<SALP>::is_ready(list<Request>::iterator req){
    SALP::Command cmd = get_first_cmd(req);
    if (cmd == SALP::Command::PRE_OTHER){

        vector<int> addr_vec = get_offending_subarray(channel, req->addr_vec);
        return channel->check(cmd, addr_vec.data(), clk);
    }
    else return channel->check(cmd, req->addr_vec.data(), clk);
}

template <>
void Controller<ALDRAM>::update_temp(ALDRAM::Temp current_temperature){
    channel->spec->aldram_timing(current_temperature);
}

template<>
void Controller<GDDR6>::tick() {
    clk++;
    req_queue_length_sum += readq.size() + writeq.size() + pending.size() + pimq.size();
    read_req_queue_length_sum += readq.size() + pending.size();
    write_req_queue_length_sum += writeq.size();

    /*** 1. Serve completed reads ***/
    if (pending.size()) {
        Request& req = pending[0];
        if (req.depart <= clk) {
            /* this part is about stat
            if (req.depart - req.arrive > 1) { // this request really accessed a row
                read_latency_sum += req.depart - req.arrive;
                channel->update_serving_requests(
                    req.addr_vec.data(), -1, clk);
            }
            */
            //req.callback(req);
            //std::cout<<"at clk : "<<clk<<", "<<req.type_name[int(req.type)]<<" depart (host can see READRES data)"<<std::endl;
            pending.pop_front();
        }
    }

    /*** 2. Refresh scheduler ***/
    refresh->tick_ref();

    /*** 3. Should we schedule writes? ***/
    /*
    if (!write_mode) {
        // yes -- write queue is almost full or read queue is empty
        if (writeq.size() > int(wr_high_watermark * writeq.max) || readq.size() == 0)
            write_mode = true;
    }
    else {
        // no -- write queue is almost empty and read queue is not empty
        if (writeq.size() < int(wr_low_watermark * writeq.max) && readq.size() != 0)
            write_mode = false;
    }
    */
    /*** 4. Find the best command to schedule, if any ***/

    // First check the actq (which has higher priority) to see if there
    // are requests available to service in this cycle
    Queue* queue = &actq;
    typename GDDR6::Command cmd;
    auto req = scheduler->get_head(queue->q);

    bool is_valid_req = (req != queue->q.end());

    if(is_valid_req) {
        cmd = get_first_cmd(req);
        is_valid_req = is_ready(cmd, req->addr_vec);
    }

    if (!is_valid_req) {
        /*
        queue = !write_mode ? &readq : &writeq;

        if (otherq.size())
            queue = &otherq;  // "other" requests are rare, so we give them precedence over reads/writes
        */
       //std::cout<<"at "<<clk<<", refresh_mode : "<<refresh_mode<<", q : "<<otherq.size()<<", pimq : "<<pimq.size()<<std::endl;

        //refresh scheduling for Newton
        if (refresh_mode) {
            //std::cout<<"refresh queue size : "<<otherq.size()<<std::endl;
        }
        if (refresh_mode && !otherq.size())
            refresh_mode = false;

        if (!pimq.size() && otherq.size() && !refresh_mode)
            refresh_mode = true;

        queue = !refresh_mode? &pimq : &otherq;

        if (refresh_mode)
            req = scheduler->get_head(queue->q);
        else
            req = pim_scheduler->get_head(queue->q);

        is_valid_req = (req != queue->q.end());

        if(is_valid_req){
            cmd = get_first_cmd(req);
            is_valid_req = is_ready(cmd, req->addr_vec);
        }
    }

    if (!is_valid_req) {
        // we couldn't find a command to schedule -- let's try to be speculative
        /*
        auto cmd = T::Command::PRE;
        vector<int> victim = rowpolicy->get_victim(cmd);
        if (!victim.empty()){
            issue_cmd(cmd, victim);
        }
        */
        return;  // nothing more to be done this cycle
    }

    /*
    if (req->is_first_command) {
        req->is_first_command = false;
        int coreid = req->coreid;
        if (req->type == Request::Type::READ || req->type == Request::Type::WRITE) {
            channel->update_serving_requests(req->addr_vec.data(), 1, clk);
        }
        int tx = (channel->spec->prefetch_size * channel->spec->channel_width / 8);
        if (req->type == Request::Type::READ) {
            if (is_row_hit(req)) {
                ++read_row_hits[coreid];
                ++row_hits;
            } else if (is_row_open(req)) {
                ++read_row_conflicts[coreid];
                ++row_conflicts;
            } else {
                ++read_row_misses[coreid];
                ++row_misses;
            }
            read_transaction_bytes += tx;
        } else if (req->type == Request::Type::WRITE) {
            if (is_row_hit(req)) {
                ++write_row_hits[coreid];
                ++row_hits;
            } else if (is_row_open(req)) {
                ++write_row_conflicts[coreid];
                ++row_conflicts;
            } else {
                ++write_row_misses[coreid];
                ++row_misses;
            }
            write_transaction_bytes += tx;
        }
    }
    */

    // issue command on behalf of request
    issue_cmd(cmd, get_addr_vec(cmd, req));

    if (cmd == GDDR6::Command::PREA)
        num_prea += 1;
    if (cmd == GDDR6::Command::PRE)
        num_pre += 1;
    if (cmd == GDDR6::Command::REF)
        num_ref += 1;
    if (cmd == GDDR6::Command::GWRITE)
        num_gwrite += 1;
    if (cmd == GDDR6::Command::G_ACT0 || cmd == GDDR6::Command::G_ACT1
        || cmd == GDDR6::Command::G_ACT2 || cmd == GDDR6::Command::G_ACT3 || cmd == GDDR6::Command::G_ACT4 || cmd == GDDR6::Command::G_ACT5 || cmd == GDDR6::Command::G_ACT6 || cmd == GDDR6::Command::G_ACT7 )
        num_gact += 1;
    if (cmd == GDDR6::Command::COMP)
        num_comp += 1;
    if (cmd == GDDR6::Command::READRES)
        num_readres += 1;

    // check whether this is the last command (which finishes the request)
    //if (cmd != channel->spec->translate[int(req->type)]){

    if (cmd != channel->spec->translate[int(req->type)]) {
        return;
    }

    //for Newton
    if (cmd == GDDR6::Command::READRES) {
        //if issue READRES, turn on refresh_mode
        assert(!refresh_mode);
        refresh_mode = true;
        assert(req->type == Request::Type::READRES);
        req->depart = clk + channel->spec->readres_latency;
        pending.push_back(*req);
    }

    // set a future completion time for read requests
    if (req->type == Request::Type::READ) {
        req->depart = clk + channel->spec->read_latency;
        pending.push_back(*req);
    }

    if (req->type == Request::Type::WRITE) {
        //channel->update_serving_requests(req->addr_vec.data(), -1, clk);
        // req->callback(*req);
    }

    // remove request from queue
    queue->q.erase(req);
}

} /* namespace ramulator */
