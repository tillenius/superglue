INCDIR=../../superglue/

MKL_ROOT=/opt/intel/mkl

ifneq ("$(wildcard /opt/intel/mkl/lib/intel64/libmkl_core.a)","")

MKL_INC=-I$(MKL_ROOT)/include
MKL_LIBS= \
  -Wl,--start-group \
  $(MKL_ROOT)/lib/intel64/libmkl_intel_lp64.a \
  $(MKL_ROOT)/lib/intel64/libmkl_sequential.a \
  $(MKL_ROOT)/lib/intel64/libmkl_core.a \
  -Wl,--end-group

cholesky: cholesky.cpp Makefile
	$(CXX) -O3 -Wall -I$(INCDIR) $(MKL_INC) cholesky.cpp -o $@ $(MKL_LIBS) -pthread -lm

else

cholesky:
	@echo "### Warning: MKL not found. Edit Makefile to correct path."
	@echo "Skipping Cholesky example."

endif
