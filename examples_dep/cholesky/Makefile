
FLAGS=-O3 -Wall -Wno-long-long -I ../../superglue -pthread

MKL_ROOT=/opt/intel/mkl

ifneq ("$(wildcard $(MKL_ROOT)/lib/intel64/libmkl_core.a)","")

MKL_INC=-I$(MKL_ROOT)/include
MKL_LIBS= \
  -Wl,--start-group \
  $(MKL_ROOT)/lib/intel64/libmkl_intel_lp64.a \
  $(MKL_ROOT)/lib/intel64/libmkl_sequential.a \
  $(MKL_ROOT)/lib/intel64/libmkl_core.a \
  -ldl \
  -Wl,--end-group

cholesky: cholesky.cpp Makefile
	$(CXX) $(FLAGS) $(MKL_INC) cholesky.cpp -o $@ $(MKL_LIBS) -lm

else

cholesky:
	@echo "### Warning: MKL not found. Edit Makefile to correct path."
	@echo "Skipping Cholesky example."

endif
