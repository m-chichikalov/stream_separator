## Class for catching specific msgs from stream of bytes.

Implemented to process data from lower priorety thread and keep IRQ handler to run as short as possible
and be reusable as much as passible to use with different msg' configurations.

Accumulated data from stream and collect in the queue the meta-data about msg and messages themself as the stream of bytes in ring buffer.

Designed to be used with:
- FreeRTOS <a href="https://github.com/michaelbecker/freertos-addons">C++ addons</a>
- Ring buffer from <a href="https://microchipdeveloper.com/atstart:start">Atmel ASF-4.0</a>

`push( byte )` should be used to push data into buffer, @warning designed to be called only from IRQ.

`next( buff, len_of_buff )` return length of message that was copied into provided by parameters buffer,
if message longer than provided buffer it's discarded.
return 0 if there is no messages in queue.

`flush()` - clean the ring-buffer and reset queue.

`next()` blocks the thread for period of time which could be set by set_timeout().

It requires help functions provided as a class with static members
- `BYTE_CONTAINED_LEN`
- `LEN_OF_SYNC`,
- `uint32_t get_len( const struct ringbuffer& rb)` and
- `bool get_sync( const struct ringbuffer& rb)`

### The example of help-class to parse UBX msgs from uart.
The UBX msg has the next struct: SYNC1:SYNC2:CLASS:ID:LENGHT_L:LENGHT_H:.PAYLOAD_OF_LENGHT..:CS_L:CS_H
 
``` cpp
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
	       return ((uint16_t)data) + 8;
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
```
 
### Example of simple usual usage below:
``` cpp
	// creation of object.
	StreamSeparator<cpp_freertos::Queue, UBX_Msg> ubx_stream;

	// constructing
	ubx_stream
	   .buffer( staticly_allocated_space_for_ringbuffer, size_of_it )
	   .set_timeout( 500 ) // ms.
	   .create();

	// The IRQ handler should push received byte by calling
	ubx_stream.push( received_byte );

	// inside thread the next msg could be read by calling next()
	// if there is no msgs, the thread will be blocked on the queue
	// for provided by set_timeout() time or first arrived msg,
	// whatever happens early.
	uint32_t len = ubx_stream.next( buffer, buff_size );
```
### TODO:
- [ ] length of queue hard-coded
- [ ] think about msg with fixed length? how we can process them?