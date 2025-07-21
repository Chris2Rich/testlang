; ModuleID = 'StackLang'
source_filename = "StackLang"

declare void @push_multidim_array(i32, i32*, double*)

declare void @push_array_data(i32, double*)

declare void @matrix_multiply()

declare void @reshape_top(i32, i32*)

declare void @transpose_top()

declare void @push_double(double)

declare double @pop_double()

declare void @do_add()

declare void @do_sub()

declare void @do_mul()

declare void @do_div()

declare void @do_mod()

declare void @do_neg()

declare void @pop_and_print()

declare void @duplicate_top()

declare void @swap_top()

declare i8* @runtime_malloc(i64)

declare void @runtime_free(i8*)

define i32 @main() {
entry:
  %0 = call i8* @runtime_malloc(i64 24)
  %1 = bitcast i8* %0 to double*
  store double 1.000000e+00, double* %1, align 8
  %2 = getelementptr i8, i8* %0, i64 8
  %3 = bitcast i8* %2 to double*
  store double 3.000000e+00, double* %3, align 8
  %4 = getelementptr i8, i8* %0, i64 16
  %5 = bitcast i8* %4 to double*
  store double 9.000000e+00, double* %5, align 8
  call void @push_array_data(i32 3, double* nonnull %1)
  %6 = call i8* @runtime_malloc(i64 24)
  %7 = bitcast i8* %6 to double*
  store double 5.000000e+00, double* %7, align 8
  %8 = getelementptr i8, i8* %6, i64 8
  %9 = bitcast i8* %8 to double*
  store double 7.000000e+00, double* %9, align 8
  %10 = getelementptr i8, i8* %6, i64 16
  %11 = bitcast i8* %10 to double*
  store double 8.000000e+00, double* %11, align 8
  call void @push_array_data(i32 3, double* nonnull %7)
  call void @do_add()
  call void @push_double(double 2.000000e+00)
  call void @do_div()
  call void @pop_and_print()
  ret i32 0
}
