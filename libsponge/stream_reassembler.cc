#include "stream_reassembler.hh"
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), cache_list(make_shared<list<Cache>>()) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // 先将 data 插入到 链表 中
    size_t cache_start_idx = insertDataToList(data, index);

    // cache_list 存储的第一个 string，其 idx 必须是 字节流 需要的
    if (cache_start_idx == next_idx) {
        const string &s = cache_list->front().s;

        // 写入的字节数
        size_t num = _output.write(s);
        next_idx += num;

        // 更新链表第一个成员
        if (num != s.size()) {
            cache_list->front().idx = next_idx;
            cache_list->front().s = string(cache_list->front().s.begin() + num, cache_list->front().s.end());
        } else
            cache_list->pop_front();
    }

    // 判断是否是 eof，获取 字节流结束的 字节号，并设置好 _eof
    //               这样可以支持 提前收到 eof 的数据，但是 前面的数据还没收到
    if (eof)
        _eof_bytes = index + data.size(), _eof = true;

    // 每次 put_string，都要检查是否 满足 eof 的条件
    //            1. 已经收到了 eof 的数据包
    //            2. 下一个待 reassemble 的 字节号 是  eof 的字节号
    if (_eof && next_idx == _eof_bytes)
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return getListAllChar(); }

bool StreamReassembler::empty() const { return cache_list->empty(); }

size_t StreamReassembler::insertDataToList(const string &data, const size_t index) {
    // 重复           => 收到 next_idx 之前的数据，或者 数据是 空
    if (index + data.size() <= next_idx || data.size() == 0)
        return -1;

    // 不重复 且 重叠 => 收到了 包含 next_idx 的数据
    if (index <= next_idx)
        // return doInsertData(index + (next_idx - index), data.cbegin() + (next_idx - index), data.cend());
        return doInsertData(next_idx, data.cbegin() + (next_idx - index), data.cend());

    // 提前到达 => 没有收到包含了 next_idx 的数据
    return doInsertData(index, data.cbegin(), data.cend());
}

// 必须进行拷贝，因为 传入的是 const 引入，不能修改其值, std::move(data) 会保留 底层const，所以最后得到的是 const
// 右值引用，无法进行移动
size_t StreamReassembler::doInsertData(const size_t index, string::const_iterator b, string::const_iterator e) {
    size_t b_idx = index;
    size_t e_idx = index + static_cast<size_t>(e - b);

    Position pos = findPosition(b_idx, e_idx);
    switch (pos.status)
    {
    case BOUTEOUT:
        cout << "BOUTEOUT" << endl;
        break;

    case BINEOUT:
        cout << "BINEOUT" << endl;
        break;

    case BOUTEIN:
        cout << "BOUTEIN" << endl;
        break;

    case BINEIN:
        cout << "BINEIN" << endl;
        break;
    
    case BNOENO:
        cout << "BNOENO" << endl;
        break;
    
    case BATENOBIN:
        cout << "BATENOBIN" << endl;
        break;

    case BATENOBOUT:
        cout << "BATENOBOUT" << endl;
        break;
    }

    switch (pos.status) {
        case BOUTEOUT: {
            // new one cache, store (b, e), delete [first_iter, last_iter), link to the location before the last_iter
            // cache need to set idx to b_idx
            Cache c;
            c.idx = b_idx;
            c.s = std::move(string(b, e));
            cache_list->erase(pos.first_iter, pos.last_iter);
            cache_list->insert(pos.last_iter, std::move(c));
            break;
        }
        case BINEOUT: {
            // a part of first_iter + store(b, e) to the first_iter, no need to link, and delete [++first_iter,
            // last_iter) no need to set idx, bacause idx should be the first_iter's idx

            pos.first_iter->s =
                string(pos.first_iter->s.begin(), pos.first_iter->s.begin() + (b_idx - pos.first_iter->idx)) +
                string(b, e);


            cache_list->erase(++pos.first_iter, pos.last_iter);
            break;
        }
        case BOUTEIN: {
            // store (b, e) + a part of last_iter to the last_iter, no need to link, and delete [first_iter, last_iter)
            // need to set idx to b_idx

            pos.last_iter->s =
                string(b, e) + string(pos.last_iter->s.begin() + (e_idx - pos.last_iter->idx), pos.last_iter->s.end());

            pos.last_iter->idx = b_idx;
            cache_list->erase(pos.first_iter, pos.last_iter);
            break;
        }
        case BINEIN: {
            // a part of first_iter + store(b, e) + a part of last_iter to the last_iter, no need to link, and delete
            // [first_iter, last_iter) need to set idx to the first_iter's idx

            pos.last_iter->s =
                string(pos.first_iter->s.begin(), pos.first_iter->s.begin() + (b_idx - pos.first_iter->idx)) +
                string(b, e) + string(pos.last_iter->s.begin() + (e_idx - pos.last_iter->idx), pos.last_iter->s.end());

            pos.last_iter->idx = pos.first_iter->idx;
            cache_list->erase(pos.first_iter, pos.last_iter);
            break;
        }
        case BNOENO:  // BNOENO
        {
            // iter lose the role
            // new a cache, store (b, e), push_back to the list
            // need set idx to b_idx
            Cache c;
            c.idx = b_idx;
            c.s = std::move(string(b, e));
            cache_list->push_back(std::move(c));
            break;
        }
        case BATENOBOUT: {
            // new one cache, store (b, e), delete [first_iter, list.end()], push_back to the list
            // need to set idx to b_idx
            Cache c;
            c.idx = b_idx;
            c.s = std::move(string(b, e));
            cache_list->erase(pos.first_iter, cache_list->end());
            cache_list->push_back(std::move(c));
            break;
        }

        case BATENOBIN: {
            // a part of first_iter + store (b, e) to the first_iter, delete [++first_iter, end())
            // no need to set idx bacause the first_iter's idx is the start idx of this cache
            pos.first_iter->s =
                string(pos.first_iter->s.begin(), pos.first_iter->s.begin() + (b_idx - pos.first_iter->idx)) +
                string(b, e);
            cache_list->erase(++pos.first_iter, cache_list->end());
            break;
        }
    }

    return cache_list->front().idx;
}

StreamReassembler::Position StreamReassembler::findPosition(const size_t b_idx, const size_t e_idx) const {
    list<Cache>::iterator it_b, it_e;  // OUT -> the next cache_iter     IN -> the cur cache_iter
    bool f_b = false, f_e = false;     // OUT ? true : false

    list<Cache>::iterator it;
    Position pos;

    for (it = cache_list->begin(); it != cache_list->end(); ++it) {
        size_t cur_b_idx = it->idx;
        size_t cur_e_idx = it->idx + it->s.size();

        if (b_idx < cur_b_idx) {  // OUT
            it_b = it;
            f_b = true;
            break;
        } else if (b_idx <= cur_e_idx) {  // IN
            it_b = it;
            break;
        }
    }

    // BNO ENO
    if (it == cache_list->end()) {
        pos.status = BNOENO;
        return pos;
    }

    for (it = cache_list->begin(); it != cache_list->end(); ++it) {
        size_t cur_b_idx = it->idx;
        size_t cur_e_idx = it->idx + it->s.size();

        if (e_idx < cur_b_idx) {  //  OUT
            it_e = it;
            f_e = true;
            break;
        } else if (e_idx <= cur_e_idx) {  //  IN
            it_e = it;
            break;
        }
    }

    // BAT ENO

    if (it == cache_list->end()) {
        // only first_iter delivery the role
        pos.first_iter = it_b;

        // BAT ENO BOUT
        if (f_b) {
            pos.status = BATENOBOUT;
            return pos;
        }

        // BAT ENO BIN
        else if (!f_b) {
            pos.status = BATENOBIN;
            return pos;
        }
    }

    // BAT EAT

    pos.first_iter = it_b;  // => first_iter, b_idx correspond to the cache
    pos.last_iter = it_e;   // => last_iter,  e_idx correspond to the cache

    // BOUT EOUT
    if (f_b && f_e)
        pos.status = BOUTEOUT;

    // BOUT EIN
    else if (f_b && !f_e)
        pos.status = BOUTEIN;

    // BIN EOUT
    else if (!f_b && f_e)
        pos.status = BINEOUT;

    // BIN EIN
    else if (!f_b && !f_e)
        pos.status = BINEIN;

    return pos;
}

size_t StreamReassembler::getListAllChar() const {
    size_t len = 0;
    for (auto it = cache_list->cbegin(); it != cache_list->cend(); ++it)
        len += it->s.size();

    return len;
}