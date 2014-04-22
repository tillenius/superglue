FLAGS=-O3 -pedantic -Wall -Wno-long-long -I ../../include -pthread

ifneq ("$(wildcard $(MKL_ROOT)/lib/intel64/libmkl_core.a)","")

MKL_INC=-isystem $(MKL_ROOT)/include
MKL_LIBS= \
  -Wl,--start-group \
  $(MKL_ROOT)/lib/intel64/libmkl_intel_lp64.a \
  $(MKL_ROOT)/lib/intel64/libmkl_sequential.a \
  $(MKL_ROOT)/lib/intel64/libmkl_core.a \
  -ldl \
  -Wl,--end-group

cholesky_mkl: cholesky.cpp Makefile
	$(CXX) $(FLAGS) $(MKL_INC) -DUSE_MKL cholesky.cpp -o $@ $(MKL_LIBS) -lm

endif


ifneq ("$(wildcard $(ACML_ROOT)/gfortran64_fma4/lib/libacml.a)","")

ACML_INC=-isystem $(ACML_ROOT)/gfortran64_fma4/include
ACML_LIBS=-L$(ACML_ROOT)/gfortran64_fma4/lib -lacml

cholesky_acml: cholesky.cpp Makefile
	$(CXX) $(FLAGS) $(ACML_INC) -DUSE_PRIO -DUSE_ACML cholesky.cpp -o $@ $(ACML_LIBS) -lm

endif

cholesky:
	@echo "### Warning: could not find MKL or ACML. Neither MKL_ROOT nor ACML_ROOT set."
	@echo "Skipping Cholesky example."

.PHONY: cholesky
