The LocalConcurrencyDetection pass detects local concurrency errors in a MPI RMA function. 


### Prerequisites

#### CMake `>= 3.18`
#### LLVM `>= 9.0`


### Installation

#### To build LocalConcurrencyDetection llvm pass 

```bash
cd path_to_LocalConcurrencyDetection
mkdir build
cd build
cmake .. 
make
```

If CMake does not find LLVM, you can supply the path to your LLVM installation as follows  :

```bash
cmake .. -DLLVM_DIR=path_to_llvm_install/lib/cmake/llvm/
```

You can run 'ctest --verbose' to see the commands executing the tests
NOTE: ctest is not finished yet


### Run the pass on a single file

#### Using clang (not functional ATM)

```bash
cd path_to_LocalConcurrencyDetection
clang -Xclang -load -Xclang build/src/LocalConcurrencyDetection.* -c something.c
=
./build/lcd something.c
```

#### Using opt

```bash
cd path_to_LocalConcurrencyDetection
clang -c -emit-llvm something.c
opt -load  build/src/LocalConcurrencyDetection.* -lcd something.bc
=
./build/lcdBC something.bc
```
		
#### To have a look at the IR after the pass

```bash
cd path_to_LocalConcurrencyDetection
./lcdBC something.bc >  somethingINSTR.bc
llvm-dis somethingINSTR.bc
```
And open the generated file somethingINSTR.ll

#### To execute the program after the pass

```bash
cd path_to_LocalConcurrencyDetection
clang -c -emit-llvm something.c
opt -load build/src/LocalConcurrencyDetection.so -lcd something.bc -o somethingINSTR.bc
clang -c somethingINSTR.bc
OMPI_CC=clang mpicc somethingINSTR.o -L./lib/ -lrma_analyzer
mpirun -np 4 --oversubscribe ./a.out
```
