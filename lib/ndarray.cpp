template <typename T>
struct ndarray {
private:
    float* items = {};
    int* shape = {0};
public:
    ndarray(int* s){
        shape = s;

    }
};