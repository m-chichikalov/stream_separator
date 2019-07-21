#ifndef QUEUE_HPP_
#define QUEUE_HPP_

#include <cstdlib>
#include <cstddef>
#include <deque>

class DummyQueue
{
public:
    DummyQueue(size_t length, size_t size_of_entry): length{length}
    {};

    bool Enqueue( void* item )
    {
        if ( queue.size() >= length )
            return false;

        queue.push_back(*((int*)item));
        return true;
    };
    bool Enqueue( void* item, uint32_t Timeout ) { return Enqueue( item ); };
    bool EnqueueFromISR( void* item, void* highTaskWaken ) { return Enqueue( item ); };

    bool Dequeue( void* item )
    {
        if ( queue.size() == 0 )
            return false;

        *((int*)item) = queue[0];
        queue.pop_front();
        return true;
    };
    bool Dequeue( void* item, uint32_t Timeout ) { return Dequeue( item ); };
//    Peek, bool( void* item, uint32_t Timeout );
//
//    IsEmpty, bool( void );
//    IsFull, bool( void );
//    Flash, void( void );
//
//    NumItems, uint32_t( void );
//    NumSpacesLeft, uint32_t( void );

    std::deque<int> queue;
    size_t length;
};


#endif // QUEUE_HPP_
