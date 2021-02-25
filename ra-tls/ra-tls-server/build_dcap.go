// +build dcap

package main

/*
#cgo LDFLAGS: -L../build/lib -L/opt/intel/sgxsdk/lib64 -Llib -lra-tls-server -l:libcurl-wolfssl.a -l:libwolfssl.a -lsgx_uae_service -lsgx_urts -lz -lm
#cgo LDFLAGS: -lsgx_dcap_ql
*/
import "C"
