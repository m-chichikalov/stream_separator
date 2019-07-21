#ifndef __RTCM3_MSG
#define __RTCM3_MSG

struct RTCM_Msg
{
    RTCM_Msg() = delete;
    ~RTCM_Msg() = delete;

    static constexpr uint32_t BYTE_CONTAINED_LEN = 3;
    static constexpr uint32_t LEN_OF_SYNC = 2;

    static uint32_t get_len( const struct ringbuffer& rb)
    {
        /** @todo > FIXME */
        /** There is a bug. Messages longer than 255 bytes will not work */
        return rb.buf[( rb.write_index - 1 ) & rb.size] + 6;
    }

    static bool get_sync( const struct ringbuffer& rb)
    {
        if(( rb.buf[( rb.write_index - 1 ) & rb.size] & 0xfc ) == 0x00U  &&
             rb.buf[( rb.write_index - 2 ) & rb.size] == 0xD3U )
        {
            return true;
        }
        return false;
    }
};

#endif //__RTCM3_MSG
