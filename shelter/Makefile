CURRENTDIR := $(shell readlink -f .)
TOPDIR := $(shell readlink -f ..)
SGX_RA_TLS := $(TOPDIR)/ra-tls
SGX_RA_TLS_LIB := $(TOPDIR)/ra-tls/build/lib
SGX_RA_TLS_INC := $(TOPDIR)/ra-tls/build/include
SGX_SDK := /opt/intel/sgxsdk
SGX_DCAP_INC ?=
INCDIR ?=
DEBUG ?= 0
EXTRA_FLAGS ?=
WOLFSSL_RA_LIBS := ../ra-tls/build/lib/libwolfssl.a
WOLFSSL_RA_LIBS += ../ra-tls/build/lib/libra-challenger.a
export SGX_RA_TLS_LIB SGX_RA_TLS_INC

#ifdef DEBUG 	
#	@echo $CURRENTDIR
#	@echo $TOPDIR
#	@echo $SGX_RA_TLS_LIB
#	@echo $SGX_RA_TLS_INC
#	@echo $SGX_SDK
#endif
	
CFLAGS += -std=gnu99 -I$(SGX_RA_TLS_INC) -I$(SGX_SDK)/include -I$(INCDIR) $(SGX_DCAP_INC) -fPIC
#CFLAGSERRORS := -Wall -Wextra -Wwrite-strings -Wlogical-op -Wshadow -Werror
CFLAGS += $(CFLAGSERRORS) -g -O0 

CC ?= gcc
GO ?= go

all: shelter

ifneq ($(GO111MODULE),off)
  MOD_VENDOR := "-mod=vendor"
endif

ifeq ($(DEBUG),1)
  GCFLAGS=-gcflags "-N -l"
endif

COMMIT_NO := $(shell git rev-parse HEAD 2> /dev/null || true)
COMMIT ?= $(if $(shell git status --porcelain --untracked-files=no),"$(COMMIT_NO)-dirty","$(COMMIT_NO)")

# glibc-static is required for the static linkage
GO_BUILD := CGO_ENABLED=1 $(GO) build $(MOD_VENDOR) -buildmode=pie $(GCFLAGS) $(EXTRA_FLAGS) \
  -ldflags "$(EXTRA_LDFLAGS)"

shelter: $(WOLFSSL_RA_LIBS) $(CURRENTDIR)/verification/verification.a  $(CURRENTDIR)/utils/utils.a $(CURRENTDIR)/remoteattestation/remoteattestation.a $(CURRENTDIR)/racommand.o $(CURRENTDIR)/mrenclaveverifycomand.o $(CURRENTDIR)/main.o
	@echo $(GO_BUILD) -o $@ .
	$(GO_BUILD) -o $@ .

$(WOLFSSL_RA_LIBS):
	@echo $(MAKE) -C ../ra-tls
	$(MAKE) -C ../ra-tls
	
$(CURRENTDIR)/utils/utils.a:
	@echo $(MAKE) -C utils
	$(MAKE) -C utils
	
$(CURRENTDIR)/racommand.o: racommand.go
	$(GO_BUILD) -o $@ .

$(CURRENTDIR)/mrenclaveverifycomand.o: mrenclaveverifycomand.go
	$(GO_BUILD) -o $@ .	

$(CURRENTDIR)/main.o: main.go
	$(GO_BUILD) -o $@ .	

$(CURRENTDIR)/verification/verification.a:
	@echo $(MAKE) -C verification
	$(MAKE) -C verification

$(CURRENTDIR)/remoteattestation/remoteattestation.a: 
	@echo $(MAKE) -C remoteattestation
	$(MAKE) -C remoteattestation

clean:
	rm -f *.o shelter
	$(MAKE) -C ../ra-tls clean
	$(MAKE) -C verification clean
	$(MAKE) -C remoteattestation clean
	$(MAKE) -C utils clean

.PHONY: clean 