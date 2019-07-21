#ifndef __UBX_MSG
#define __UBX_MSG

struct UBX_Msg
{
    UBX_Msg() = delete;
    ~UBX_Msg() = delete;

    static constexpr uint32_t BYTE_CONTAINED_LEN = 6;
    static constexpr uint32_t LEN_OF_SYNC = 2;

    static uint32_t get_len( const struct ringbuffer& rb)
    {
        uint8_t data[2];
        data[0] = rb.buf[( rb.write_index - 2 ) & rb.size];
        data[1] = rb.buf[( rb.write_index - 1 ) & rb.size];
        return *((uint16_t*)data) + 8;
    }

    static bool get_sync( const struct ringbuffer& rb)
    {
        if( rb.buf[( rb.write_index - 1 ) & rb.size] == 0x62  &&
            rb.buf[( rb.write_index - 2 ) & rb.size] == 0xb5 )
        {
            return true;
        }
        return false;
    }
};

#endif //__UBX_MSG
