/**
 * @author m-chichikalov@outlook.com
 *
 * @license  This is free chunk of code, you can do with it whatever you want;
 *           There is no any warranty and it's posted in the hope that it will be useful.
 *
 * @brief Accumulated data from stream and collect in the queue the meta-data of msg and messages in ring buffer.
 *
 * Designed to be used with:
 * 1 - FreeRTOS C++ addons <a href="https://github.com/michaelbecker/freertos-addons">C++ addons</a>
 * 2 - Ring buffer from Atmel ASF-4.0 <a href="https://microchipdeveloper.com/atstart:start">Atmel ASF-4.0</a>
 *
 * push( byte ) should be used to push data into buffer, @warning designed to be called only from IRQ.
 *
 * next( buff, len_of_buff ) return length of message that was copied into provided by parameters buffer,
 * if message longer than provided buffer it's discarded.
 * return 0 if there is no messages in queue.
 *
 * flush() - clean the ring-buffer and reset queue.
 *
 *  next() blocks the thread for period of time which could be set by set_timeout().
 *
 *  It requires help functions provided as a class with static members
 *  BYTE_CONTAINED_LEN, LEN_OF_SYNC,
 *  uint32_t get_len( const struct ringbuffer& rb) and
 *  bool get_sync( const struct ringbuffer& rb)
 *
 *  @example help-class to parse UBX msgs from uart.
 *  The UBX msg has the next struct: SYNC1:SYNC2:CLASS:ID:LENGHT_L:LENGHT_H:.PAYLOAD_OF_LENGHT..:CS_L:CS_H
 *
 *  struct UBX_Msg
 *  {
 *      UBX_Msg() = delete;
 *      ~UBX_Msg() = delete;
 *
 *      static constexpr uint32_t BYTE_CONTAINED_LEN = 6;
 *      static constexpr uint32_t LEN_OF_SYNC = 2;
 *
 *      static uint32_t get_len( const struct ringbuffer& rb)
 *      {
 *          uint8_t data[2];
 *          data[0] = rb.buf[( rb.write_index - 2 ) & rb.size];
 *          data[1] = rb.buf[( rb.write_index - 1 ) & rb.size];
 *          return *((uint16_t*)data) + 8;
 *      }
 *
 *      static bool get_sync( const struct ringbuffer& rb)
 *      {
 *          if( rb.buf[( rb.write_index - 1 ) & rb.size] == 0x62  &&
 *              rb.buf[( rb.write_index - 2 ) & rb.size] == 0xb5 )
 *          {
 *              return true;
 *          }
 *          return false;
 *      }
 *  };
 *
 * @example Example of simple usual usage below:
 *
 *  // creation of object.
 *  StreamSeparator<cpp_freertos::Queue, UBX_Msg> ubx_stream;
 *
 *  // constructing
 *  ubx_stream
 *      .buffer( staticly_allocated_space_for_ringbuffer, size_of_it )
 *      .set_timeout( 500 ) // ms.
 *      .create();
 *
 *  // The IRQ handler should push received byte by calling
 *  ubx_stream.push( received_byte );
 *
 *  // inside thread the next msg could be read by calling next()
 *  // if there is no msgs, the thread will be blocked on the queue
 *  // for provided by set_timeout() time or first arrived msg,
 *  // whatever happens early.
 *  uint32_t len = ubx_stream.next( buffer, buff_size );
 *
 *  @TODO:
 *  - length of queue hard-coded
 *  - think about msg with fixed length? how we can process them?
 */


#ifndef __STREAM_SEPARATOR__
#define __STREAM_SEPARATOR__

#include "utils_ringbuffer.h"

template<class CQueue, class StreamConverter>
class StreamSeparator
{
public:
    /** @todo > the length of queue hardcoded, maybe setup */
    StreamSeparator():
        q_len_of_msg{ 10, sizeof(uint32_t)}
    {};
    ~StreamSeparator() {};

    StreamSeparator& buffer( uint8_t* buf_, uint32_t size_ )
    {
        if ( ringbuffer_init( &rb, buf_, size_) != ERR_NONE )
        {
            ASSERT( false );
        }
        return *this;
    }
    StreamSeparator& set_timeout( uint32_t timeout_ )
    {
        timeout = timeout_;
        return *this;
    }

    /** create() calls the last in the chain and do sanity work */
    void create()
    {
        ASSERT( rb.buf && rb.size );
    }

    int32_t next ( uint8_t* buff_read_to, uint32_t size )
    {
        uint32_t retval{0};
        if ( q_len_of_msg.Dequeue( &retval, timeout ))
        {
            if ( retval > size )
            {
                /**
                 * There is a strategy to discard received msg
                 * when it's length more than receiver can accept.
                 */
                discard( retval );
                retval = 0;
            }
            else
            {
                uint32_t was_read{0};
                while ( was_read < retval) {
                    ringbuffer_get( &rb, &buff_read_to[was_read++]);
                }
            }
        }
        return retval;
    }

    void flush()
    {
        q_len_of_msg.Flush();
        ringbuffer_flush( &rb );
        AlgorithmState.count_received_chars = 0;
        AlgorithmState.state = State::LOOKING_FOR_SYNC;
    }

    void push ( uint8_t byte )
    {
        ringbuffer_put( &rb, byte );
        detect();
    }

private:
    CQueue q_len_of_msg;
    struct ringbuffer rb{};
    uint32_t timeout{};

    enum class State
    {
        LOOKING_FOR_SYNC = 0,
        WAITING_LENGTH,
        WAITING_FULL_MSG,
    };

    struct DetectAlgorithmState
    {
        State state;
        uint32_t count_received_chars;
        uint32_t full_msg_length;
    };

    DetectAlgorithmState AlgorithmState{ State::LOOKING_FOR_SYNC, 0, 0 };

    void discard ( uint32_t len_ )
    {
        CRITICAL_SECTION_ENTER();
            rb.read_index += len_;
        CRITICAL_SECTION_LEAVE();
    }

    void detect()
    {
        BaseType_t pxHigherPriorityTaskWoken = false;

        AlgorithmState.count_received_chars++;
        switch ( AlgorithmState.state )
        {
        case State::LOOKING_FOR_SYNC:
            if( StreamConverter::get_sync( rb ))
            {
                uint32_t counter_before_sync = AlgorithmState.count_received_chars - StreamConverter::LEN_OF_SYNC;
                if ( counter_before_sync != 0 )
                {
                    q_len_of_msg.EnqueueFromISR( &counter_before_sync, &pxHigherPriorityTaskWoken );
                    AlgorithmState.count_received_chars = StreamConverter::LEN_OF_SYNC;
                }
                AlgorithmState.state = State::WAITING_LENGTH;
            }
            break;

            /** @todo-> What about msgs with fixed length? */
        case State::WAITING_LENGTH:
            if ( AlgorithmState.count_received_chars == StreamConverter::BYTE_CONTAINED_LEN )
            {
                AlgorithmState.full_msg_length = StreamConverter::get_len( rb );
                AlgorithmState.state = State::WAITING_FULL_MSG;
            }
            break;

        case State::WAITING_FULL_MSG:
            if ( AlgorithmState.count_received_chars == AlgorithmState.full_msg_length )
            {
                q_len_of_msg.EnqueueFromISR( &AlgorithmState.count_received_chars, &pxHigherPriorityTaskWoken );
                AlgorithmState.count_received_chars = 0;
                AlgorithmState.state = State::LOOKING_FOR_SYNC;
            }
            break;

        default:
            ASSERT(false);
            break;
        }
        if( pxHigherPriorityTaskWoken )
        {
            portYIELD_FROM_ISR( pxHigherPriorityTaskWoken );
        }
    }
};

#endif //__STREAM_SEPARATOR__
