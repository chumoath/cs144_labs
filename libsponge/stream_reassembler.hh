#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <iostream>
using namespace std;

/* 整体思路：
     用 一个 Cache 链表 控制 字节流的 缓存
     用 next_idx，标识 下一个写入 需要的 字节号
     _eof 和 _eof_bytes，用于标识 eof
        设置 eof 的条件：
          1. 之前收到过 eof 标记，使用 data.size() + index 即可知道 _eof_bytes
          2. next_idx == _eof_bytes
 */

/* 重点

  数据插入

    role:   负责 检验数据       重复    /    重叠    /    提前到达，调用 doInsertData 实际插入
            负责 获取有效数据
    return: 链表中第一个 Cache 的 idx，只要插入成功，一定有；没有插入，返回 -1

    size_t insertDataToList(const string & data, size_t index);




    重复           => 收到 next_idx 之前的数据，或者 数据是 空，返回 -1，表示无效

    重叠           => 收到了 包含 next_idx 的数据，对 data 进行切片

    提前到达       => 收到了 next_idx 之后 的数据，将 data 存入 cache


    role:    根据 有效数据的 开始 和 结束 idx，在 Cache 链表中 寻找 位置 进行 插入/合并(并删除)
    return:  first_iter, last_iter   =>  b_idx 和 e_idx 所对应的 Cache 的 迭代器，
                             status  =>  位置类型

      Position findPosition(const size_t b_idx, const size_t e_idx) const;


    role:    调用 findPostion 获取 位置信息，根据位置信息 将有效数据 插入 cache_list
    return:  链表中第一个元素的 idx，即已经收到的字节流的最开始位置，用于与 next_idx 比较

    size_t doInsertData(const size_t index, string::const_iterator b, string::const_iterator e);


    -------------------------------------------------------------------------------------------------------

    位置信息

    b_idx 和 e_idx 独立寻址，可方便处理各种边界情况


    - BAT EAT          =>     b_idx 和 e_idx 都可以在 链表中找到 相应的 cache，b_idx 或 e_idx 在 该 cache.idx 的 前面 或
  在 该 cache 内部
      - BIN    EIN

                ---------
                |       |   <-  b_idx     => first_iter
                ---------

                ...

                ---------
                |       |   <-  e_idx     => last_iter
                ---------




      - BIN    EOUT

                ---------
                |       |   <-  b_idx     => first_iter
                ---------

                ...

                ---------
                |       |
                ---------
                            <-  e_idx
                ---------
                |       |                 => last_iter
                ---------



      - BOUT   EIN

                            <-  b_idx
                ---------
                |       |                 => first_iter
                ---------

                ...

                ---------
                |       |   <-  e_idx     => last_iter
                ---------



      - BOUT   EOUT     => process in a same way

          - nosame iter
                              <-  b_idx
                  ---------
                  |       |                 => first_iter
                  ---------

                  ...

                  ---------
                  |       |
                  ---------
                              <-  e_idx
                  ---------
                  |       |                 => last_ter
                  ---------


          - same iter

                              <-  b_idx  <-  e_idx
                  ---------
                  |       |                 => first_iter  => last_iter
                  ---------




    - BAT ENO          =>     b_idx 可以，e_idx 不可以

      - BAT ENO BOUT   =>     b_idx 在 该 cache 的前面

                            <-  b_idx
                ---------
                |       |                => first_iter
                ---------

                ...

                ---------
                |       |
                ---------

                NULL        <-  e_idx




      - BAT ENO BIN    =>     b_idx 在 该 cache 的内部

                ---------
                |       |   <-  b_idx     => first_iter
                ---------

                ...

                ---------
                |       |
                ---------

                NULL        <-  e_idx



    - BNO ENO          =>     b_idx 不可以，e_idx 也不可以

                NULL
                            <-  b_idx   e_idx
*/

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.
    struct Cache {
        size_t idx{};
        string s{};
    };

    enum PositionStatus {
        // BAT EAT
        BOUTEOUT,
        BOUTEIN,
        BINEIN,
        BINEOUT,

        // BNO ENO   => NO, out of cache 
        BNOENO,

        // BAT ENO BOUT    => AT, in the cache list
        BATENOBOUT,
        // BAT ENO BIN
        BATENOBIN
    };

    struct Position {
        list<Cache>::iterator first_iter{};  // need to modify the elem
        list<Cache>::iterator last_iter{};
        PositionStatus status{};
    };

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes
    size_t _eof_bytes{0};
    bool _eof{false};

    size_t next_idx{0};
    shared_ptr<list<Cache>> cache_list;

    Position findPosition(const size_t b_idx, const size_t e_idx) const;
    size_t insertDataToList(const string &data, size_t index);
    size_t doInsertData(const size_t index, string::const_iterator b, string::const_iterator e);
    size_t getListAllChar() const;

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;

    size_t window_size(void) const{
		// entire window size => capacity

		// entire window size = _output_unread_bytes + remain window size 

		return _capacity - _output.buffer_size();
    }
	// entire cache is the window
	size_t ackno_index(void) const {
		return next_idx;
	}

  bool isToEof (void) const {
      if (!_eof) return false;

      return _eof_bytes == next_idx;
  }
	// for debug
	void dump_cache_graph(uint64_t _isn) {
		cout << "--------------------------------------------------------" << endl;
		
    for (auto & c : *cache_list) {
			cout << "|    ";

			cout << "begin: " << _isn + c.idx + 1;
			
			cout << "      ";
			
			cout << "end:" << _isn + c.idx + c.s.size() + 1;

      cout << "      ";

      cout << "size:" << c.s.size();

			cout << endl;
		}

		cout << "--------------------------------------------------------" << endl;
	}

};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
