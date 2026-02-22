#include <pthread.h>

#ifndef TS_QUEUE_HPP
#define TS_QUEUE_HPP

#define DEFAULT_BUFFER_SIZE 200

template <class T>
class TSQueue {
public:
	// constructor
	TSQueue();

	explicit TSQueue(int max_buffer_size);

	// destructor
	~TSQueue();

	// add an element to the end of the queue
	void enqueue(T item);

	// remove and return the first element of the queue
	T dequeue();

	// return the number of elements in the queue
	int get_size();
private:
	// the maximum buffer size
	int buffer_size;
	// the buffer containing values of the queue
	T* buffer;
	// the current size of the buffer
	int size;
	// the index of first item in the queue
	int head;
	// the index of last item in the queue
	int tail;

	// pthread mutex lock
	pthread_mutex_t mutex;
	// pthread conditional variable
	pthread_cond_t cond_enqueue, cond_dequeue;
};

// Implementation start

template <class T>
TSQueue<T>::TSQueue() : TSQueue(DEFAULT_BUFFER_SIZE) {
}

template <class T>
TSQueue<T>::TSQueue(int buffer_size) : buffer_size(buffer_size) {
	// TODO: implements TSQueue constructor
}

template <class T>
TSQueue<T>::~TSQueue() {
	// TODO: implenents TSQueue destructor
}

template <class T>
void TSQueue<T>::enqueue(T item) {
	// TODO: enqueues an element to the end of the queue
}

template <class T>
T TSQueue<T>::dequeue() {
	// TODO: dequeues the first element of the queue
}

template <class T>
int TSQueue<T>::get_size() {
	// TODO: returns the size of the queue
}

#endif // TS_QUEUE_HPP
