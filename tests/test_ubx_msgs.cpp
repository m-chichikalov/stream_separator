#include "gtest/gtest.h"

/** including doubles */
#include "tests/test_doubles/ring_buffer/utils_ringbuffer.h"
#include "tests/test_doubles/queue/dummy_queue.hpp"
#include "tests/test_doubles/ubx_stream_separator.hpp"

/** including files under test */
#define CRITICAL_SECTION_ENTER(x)
#define CRITICAL_SECTION_LEAVE(x)
#define portYIELD_FROM_ISR(x)
typedef bool BaseType_t;
    #include "stream_separator.hpp"

namespace {
    const static uint32_t G_buffer_size = 128;
    static std::array<uint8_t, G_buffer_size> G_buffer{0};

    constexpr static std::array<uint8_t, 48> svin_48{ 0xb5, 0x62, 0x01, 0x3b, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x63, 0xde,
            0x1d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8a, 0x87 };

    constexpr std::array<uint8_t, 9> noise_9{ 0x62, 0x01, 0x3b, 0x28, 0x00, 0x90, 0x64, 0xde, 0x1d };
};

// Extra test fixture for convenient feeding the stream
struct Feed {
    struct msg {
        const uint8_t* stream;
        const uint32_t size;
    };

    std::deque<msg> msgs;

    template <size_t n>
    void add(const std::array<uint8_t, n>& arr, int32_t times = 1 )
    {
        msg new_msg{ arr.data(), arr.size()};
        for ( auto i = 0; i < times; i++ )
        {
            msgs.push_back(new_msg);
        }
    };

    template<class Q, class C>
    void feed_all( StreamSeparator<Q, C>& separator )
    {
        feed_next( separator, msgs.size());
    }

    template<class Q, class C>
    void push( StreamSeparator<Q, C>& separator, const msg& m )
    {
        for ( uint32_t i = 0; i < m.size; ++i )
        {
            separator.push( m.stream[i] );
        }
    }

    template<class Q, class C>
    void feed_next( StreamSeparator<Q, C>& separator, int32_t number = 1 )
    {
        for ( auto i = 0; i < number && msgs.size(); ++i )
        {
            push( separator, msgs[0] );
            msgs.pop_front();
        }
    }
};

class UBX_Msgs_CUT: public ::testing::Test
{
public:
    UBX_Msgs_CUT()
    {
        ubx_stream
            .buffer( G_buffer.data(), G_buffer_size )
            .set_timeout( 500 ) // ms.
            .create();
    };

protected:
    std::array<uint8_t, 1024> buff{0};
    Feed f;
    StreamSeparator<DummyQueue, UBX_Msg> ubx_stream;
};

TEST_F( UBX_Msgs_CUT, TwoCorrectMsgsInRow )
{
    f.add( svin_48, 2 );
    f.feed_all( ubx_stream );

    uint32_t len = ubx_stream.next( buff.data(), 1024 );
    EXPECT_EQ( len, uint32_t( 48 ));
    len = ubx_stream.next( buff.data(), 1024);
    EXPECT_EQ( len, uint32_t( 48 ));
};

TEST_F( UBX_Msgs_CUT, EmptyReturnZero )
{
    uint32_t len = ubx_stream.next( buff.data(), 1024);
    EXPECT_EQ( len, uint32_t( 0 ));
};

TEST_F( UBX_Msgs_CUT, NoiseBetweenMsgsShouldBeDiscarded )
{
    f.add( svin_48 );
    f.add( noise_9 );
    f.add( svin_48 );
    f.feed_all( ubx_stream );

    uint32_t len = ubx_stream.next( buff.data(), 1024 );
    EXPECT_EQ( len, uint32_t( 48 ));
    len = ubx_stream.next( buff.data(), 1024 );
    EXPECT_EQ( len, uint32_t( 48 ))  << "wrong 9 bytes was not discarded!!!";
};

TEST_F( UBX_Msgs_CUT, NoiseBeforeMsgsShouldBeDiscarded )
{
    f.add( noise_9 );
    f.add( svin_48 );
    f.feed_all( ubx_stream );

    uint32_t len = ubx_stream.next( buff.data(), 1024 );
    EXPECT_EQ( len, uint32_t( 48 )) << "wrong 9 bytes was not discarded!!!";
};

TEST_F( UBX_Msgs_CUT, ReturnedDataEqDataWereSent )
{
    f.add( svin_48 );
    f.feed_all( ubx_stream );

    uint32_t len = ubx_stream.next( buff.data(), 1024 );
    EXPECT_EQ( len, uint32_t( 48 ));

    EXPECT_EQ(strcmp((char*)buff.data(), (char*)svin_48.data()), 0 );
};

TEST_F( UBX_Msgs_CUT, BufferOverWrittenByDataLongerThanLenOfBuffer )
{
    f.add( svin_48 );

    // pushing 90 more bytes to reach buffer size; size=(128-1)
    f.add( noise_9, 10);
    f.feed_all( ubx_stream );

    uint32_t len = ubx_stream.next( buff.data(), 1024 );
    EXPECT_EQ( len, uint32_t( 48 ));
    EXPECT_EQ(strcmp((char*)buff.data(), (char*)svin_48.data()), 0 );
};
