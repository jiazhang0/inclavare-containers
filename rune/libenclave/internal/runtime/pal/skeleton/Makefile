ifndef OBJCOPY
OBJCOPY := $(CROSS_COMPILE)objcopy
endif

OUTPUT ?= ./
SGX_SDK ?= /opt/intel/sgxsdk
RA_TLS := ../../../../../../ra-tls
LIBDIR := $(RA_TLS)/build/lib
INCDIR := $(RA_TLS)/build/include
RA_TLS_SERVER := $(RA_TLS)/ra-tls-server
TLS_SERVER ?=

HOST_CFLAGS := -Wall -Werror -g -fPIC -z noexecstack \
	       -Wno-unused-const-variable -std=gnu11 -I../kvmtool/include \
	       -I$(SGX_SDK)/include -I$(INCDIR)
ifdef TLS_SERVER
HOST_CFLAGS += -DTLS_SERVER
endif

ENCL_CFLAGS := -Wall -Werror -static -nostdlib -nostartfiles -fPIC \
	       -fno-stack-protector -mrdrnd -std=gnu11
HOST_LDFLAGS := -fPIC -shared -Wl,-Bsymbolic

LDFLAGS :=
ifdef TLS_SERVER
LDFLAGS += -L$(LIBDIR) -L$(SGX_SDK)/lib64 -L$(RA_TLS_SERVER)/lib \
	   -l:libra-tls-server.a -l:libcurl-wolfssl.a -l:libwolfssl.a \
	   -lsgx_uae_service -lsgx_urts -lsgx_dcap_ql
endif

PRODUCT_ENCLAVE ?=

TEST_CUSTOM_PROGS := $(OUTPUT)/encl.bin \
		     $(OUTPUT)/encl.ss \
		     $(OUTPUT)/aesm.pb-c.c \
		     $(OUTPUT)/liberpal-skeleton-v1.so \
		     $(OUTPUT)/liberpal-skeleton-v2.so \
		     $(OUTPUT)/liberpal-skeleton-v3.so \
		     $(OUTPUT)/signing_key.pem

all: $(TEST_CUSTOM_PROGS)

libvmm:
	$(MAKE) -C ../kvmtool all

../kvmtool/libvmm.a: libvmm

$(RA_TLS_SERVER)/lib/libra-tls-server.a:
	$(MAKE) -C $(RA_TLS)

RA_TLS_LIBS :=
ifdef TLS_SERVER
RA_TLS_LIBS += $(OUTPUT)/tls-server.o $(RA_TLS_SERVER)/lib/libra-tls-server.a
endif

$(OUTPUT)/liberpal-skeleton-v1.so: $(OUTPUT)/sgx_call.o $(OUTPUT)/liberpal-skeleton-v1.o \
				   $(OUTPUT)/liberpal-skeleton.o $(OUTPUT)/sgxutils.o \
				   $(OUTPUT)/aesm.o $(OUTPUT)/aesm.pb-c.o \
				   ../kvmtool/libvmm.a $(RA_TLS_LIBS)
	$(CC) $(HOST_LDFLAGS) -o $@ $^ -lprotobuf-c -lutil -lbfd

$(OUTPUT)/liberpal-skeleton-v1.o: liberpal-skeleton-v1.c liberpal-skeleton.c
	$(CC) $(HOST_CFLAGS) -c $< -o $@

$(OUTPUT)/liberpal-skeleton-v2.so: $(OUTPUT)/sgx_call.o $(OUTPUT)/liberpal-skeleton-v2.o \
				   $(OUTPUT)/liberpal-skeleton.o $(OUTPUT)/sgxutils.o \
				   $(OUTPUT)/aesm.o $(OUTPUT)/aesm.pb-c.o \
				   ../kvmtool/libvmm.a $(RA_TLS_LIBS)
	$(CC) $(HOST_LDFLAGS) -o $@ $^ -lprotobuf-c -lutil -lbfd

$(OUTPUT)/liberpal-skeleton-v2.o: liberpal-skeleton-v2.c liberpal-skeleton.c
	$(CC) $(HOST_CFLAGS) -c $< -o $@

$(OUTPUT)/liberpal-skeleton.o: liberpal-skeleton.c
	$(CC) $(HOST_CFLAGS) -c $< -o $@

$(OUTPUT)/liberpal-skeleton-v3.so: $(OUTPUT)/sgx_call.o $(OUTPUT)/liberpal-skeleton-v3.o \
				   $(OUTPUT)/liberpal-skeleton.o $(OUTPUT)/sgxutils.o \
				   $(OUTPUT)/aesm.o $(OUTPUT)/aesm.pb-c.o \
				   ../kvmtool/libvmm.a $(RA_TLS_LIBS)
	$(CC) $(HOST_LDFLAGS) -o $@ $^ -lprotobuf-c -lutil -lbfd $(LDFLAGS)

$(OUTPUT)/liberpal-skeleton-v3.o: liberpal-skeleton-v3.c liberpal-skeleton.c
	$(CC) $(HOST_CFLAGS) -c $< -o $@

$(OUTPUT)/tls-server.o: tls-server.c
	$(CC) $(HOST_CFLAGS) -c $< -o $@

$(OUTPUT)/aesm.o: aesm.c
	$(CC) $(HOST_CFLAGS) -c $< -o $@

$(OUTPUT)/aesm.pb-c.o: aesm.pb-c.c
	$(CC) $(HOST_CFLAGS) -c $< -o $@

$(OUTPUT)/aesm.pb-c.c: aesm.proto
	@protoc-c --c_out=. $<

$(OUTPUT)/sgx_call.o: sgx_call.S
	$(CC) $(HOST_CFLAGS) -c $< -o $@

$(OUTPUT)/sgxutils.o: sgxutils.c
	$(CC) $(HOST_CFLAGS) -c $< -o $@

$(OUTPUT)/encl.bin: $(OUTPUT)/encl.elf $(OUTPUT)/sgxsign
	$(OBJCOPY) -O binary $< $@

$(OUTPUT)/encl.elf: encl.lds encl.c encl_bootstrap.S
	$(CC) $(ENCL_CFLAGS) -T $^ -o $@

$(OUTPUT)/signing_key.pem:
	openssl genrsa -3 -out $@ 3072

# If you want to prohibit enclave debug function, you need add '-N' argument in sgxsign.
# In addition, for Intel SGX1 without FLC, please replace signing_key with the product
# signing key applied to Intel.
ifeq ($(NO_DEBUGGER),1)
        NO_DEBUGGER_OPT := -N
else
        NO_DEBUGGER_OPT :=
endif

$(OUTPUT)/encl.ss: $(OUTPUT)/encl.bin $(OUTPUT)/signing_key.pem
	$(OUTPUT)/sgxsign $(NO_DEBUGGER_OPT) signing_key.pem $(OUTPUT)/encl.bin $(OUTPUT)/encl.ss

$(OUTPUT)/sgxsign: sgxsign.c sgxutils.c
	$(CC) -I../include -o $@ $^ -lcrypto

EXTRA_CLEAN := \
	$(OUTPUT)/encl.bin \
	$(OUTPUT)/encl.elf \
	$(OUTPUT)/encl.ss \
	$(OUTPUT)/sgx_call.o \
	$(OUTPUT)/aesm.o \
	$(OUTPUT)/aesm.pb-c.o \
	$(OUTPUT)/sgxutils.o \
	$(OUTPUT)/sgxsign \
	$(OUTPUT)/liberpal-skeleton*.o \
	$(OUTPUT)/tls-server.o \
	$(OUTPUT)/liberpal-skeleton*.so \
	$(OUTPUT)/signing_key.pem

DIRS_TO_CLEAN := ../kvmtool
ifdef TLS_SERVER
DIRS_TO_CLEAN += $(RA_TLS)
endif

clean:
	rm -f ${EXTRA_CLEAN}
	$(MAKE) -C $(DIRS_TO_CLEAN) clean

.PHONY: clean all
