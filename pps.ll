; ModuleID = 'StackLang'
source_filename = "StackLang"

%Array = type { i32, double* }

declare void @push_double(double)

declare double @pop_double()

declare void @push_array(%Array)

declare %Array @pop_array()

declare void @print_double(double)

declare void @print_array(%Array)

define i32 @main() {
entry:
  %0 = call dereferenceable_or_null(24) i8* @malloc(i64 24)
  %1 = bitcast i8* %0 to double*
  store double 1.000000e+00, double* %1, align 8
  %2 = getelementptr double, double* %1, i64 1
  store double 3.000000e+00, double* %2, align 8
  %3 = getelementptr double, double* %1, i64 2
  store double 9.000000e+00, double* %3, align 8
  %4 = alloca %Array, align 8
  %5 = getelementptr inbounds %Array, %Array* %4, i64 0, i32 0
  %6 = getelementptr inbounds %Array, %Array* %4, i64 0, i32 1
  store i32 3, i32* %5, align 8
  %7 = bitcast double** %6 to i8**
  store i8* %0, i8** %7, align 8
  %8 = load %Array, %Array* %4, align 8
  call void @push_array(%Array %8)
  %9 = call i8* @malloc.1(i64 24)
  %10 = bitcast i8* %9 to double*
  store double 5.000000e+00, double* %10, align 8
  %11 = getelementptr i8, i8* %9, i64 8
  %12 = bitcast i8* %11 to double*
  store double 7.000000e+00, double* %12, align 8
  %13 = getelementptr i8, i8* %9, i64 16
  %14 = bitcast i8* %13 to double*
  store double 8.000000e+00, double* %14, align 8
  %15 = alloca %Array, align 8
  %16 = getelementptr inbounds %Array, %Array* %15, i64 0, i32 0
  %17 = getelementptr inbounds %Array, %Array* %15, i64 0, i32 1
  store i32 3, i32* %16, align 8
  %18 = bitcast double** %17 to i8**
  store i8* %9, i8** %18, align 8
  %19 = load %Array, %Array* %15, align 8
  call void @push_array(%Array %19)
  %20 = call double @pop_double()
  %21 = call double @pop_double()
  %22 = fadd double %20, %21
  call void @push_double(double %22)
  call void @push_double(double 2.000000e+00)
  %23 = call double @pop_double()
  %24 = call double @pop_double()
  %25 = fdiv double %24, %23
  call void @push_double(double %25)
  ret i32 0
}

declare i8* @malloc(i64)

declare i8* @malloc.1(i64)
