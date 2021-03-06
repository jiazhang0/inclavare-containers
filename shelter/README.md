# shelter

Shelter is designed as a remote attestation tool for customer to verify if their workloads are loaded in a specified intel authorized sgx enclaved.

The verifying process is as below:
1. shelter setup a security channel based on mTLS with inclavared
2. inclavared will generate/retrieve the quote info of workload running enclave
3. inclavared will get IAS report by quote info from Intel authorized web server
4. inclavared will generate attestation verification report
5. shelter will verify the attestation verification report and mrenclave value by mTLS security channel
6. shelter will report the verifying result

## Prerequisite

Go 1.13.x or above.

## Build

Please follow the command to build Inclavare Containers from the latested source code on your system.

1. Download the latest source code of Inclavare Containers

```shell
mkdir -p "$WORKSPACE"
cd "$WORKSPACE"
git clone https://github.com/alibaba/inclavare-containers
```

2. Prepare the dependence libs required by shelter

### For EPID RA

```shell
cd $WORKSPACE/inclavare-containers/shelter
make
```

### For DCAP RA

Please refer to [this guide](https://github.com/intel/SGXDataCenterAttestationPrimitives/blob/master/README.md) to install DCAP. Note: If your platform is pre-product SGX platform (SBX), please follow this guide to resolve the quote verification problem on SBX platforms: https://github.com/alibaba/inclavare-containers/blob/master/hack/use-sbx-platform/README.md.

```shell
cd $WORKSPACE/inclavare-containers/shelter
make ECDSA=1
```

## Run

Before running shelter, make sure inclavared being luanched as server mode successfully in the same machine.
You can find the way to run inclavared by: https://github.com/alibaba/inclavare-containers/inclavared

1. check shelter support feature as below

```shell
shelter help
   NAME:
      shelter - shelter as a remote attestation tool for workload runing in enclave containers.

   USAGE:
      shelter [global options] command [command options] [arguments...]

   VERSION:
      0.0.1

   COMMANDS:
      remoteattestation  attest IAS report obtained by inclavared and setup TLS security channel with inclavared
      mrverify           download target source code to rebuild and caculate launch measurement based on software algorithm and then compare with launch measurement in sigsturct file
      help, h            Shows a list of commands or help for one command

   GLOBAL OPTIONS:
      --verbose      enable verbose output
      --help, -h     show help
      --version, -v  print the version
```

2. remote attestation for epid-sgx-ra & dcap-sgx-ra

```shell
shelter remoteattestation
```

3. verify workload integrity by launch measurement.
   The software algorithm refer to [skeleton](https://github.com/alibaba/inclavare-containers/tree/master/rune/libenclave/internal/runtime/pal/skeleton) project.

```shell
shelter mrverify
```

## Touble shooting

TODO
