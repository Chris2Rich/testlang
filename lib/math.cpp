inline constexpr double sqrt(double n, double res = 1){
    for(int i = 0; i < 1000; i++){
        res = (res / 2.0) + (n / (2.0 * res));
    }
    return res;
}
