template <typename T>
struct stack {
private:
    T* items;
    int top;
    int capacity;
    
    void resize(int newCapacity) {
        T* newItems = new T[newCapacity];
        for (int i = 0; i < top + 1; i++) {
            newItems[i] = items[i];
        }
        delete[] items;
        items = newItems;
        capacity = newCapacity;
    }

public:
    stack(int initialCapacity = 10) : capacity(initialCapacity), top(-1) {
        items = new T[initialCapacity];
    }
    
    stack(const stack& other) : capacity(other.capacity), top(other.top) {
        items = new T[capacity];
        for (int i = 0; i <= top; i++) {
            items[i] = other.items[i];
        }
    }
    
    stack& operator=(const stack& other) {
        if (this != &other) {
            delete[] items;
            capacity = other.capacity;
            top = other.top;
            items = new T[capacity];
            for (int i = 0; i <= top; i++) {
                items[i] = other.items[i];
            }
        }
        return *this;
    }
    
    ~stack() {
        delete[] items;
    }
    
    void push(const T& item) {
        if (top == capacity - 1) {
            resize(capacity * 2);
        }
        items[++top] = item;
    }
    
    T pop() {
        if (Empty()) {
            return T();
        }
        return items[top--];
    }
    
    T& peek() {
        if (Empty()) {
            return T();
        }
        return items[top];
    }
    
    bool Empty() const {
        return top == -1;
    }
    
    int size() const {
        return top + 1;
    }
    
    void clear() {
        top = -1;
    }
};