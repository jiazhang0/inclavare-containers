# Running Occlum with Docker and OCI Runtime rune

This user guide provides the steps to run Occlum with Docker and OCI Runtime `rune`.

[rune](https://github.com/alibaba/inclavare-containers/tree/master/rune) is a novel OCI Runtime used to run trusted applications in containers with the hardware-assisted enclave technology.

[Occlum](https://github.com/occlum/occlum) is a memory-safe, multi-process library OS for Intel SGX.

# Requirements

- Ensure that you have one of the following required operating systems to build a Occlum container image:
  - CentOS 8.1
  - Ubuntu 18.04-server

- Please follow [Intel SGX Installation Guide](https://download.01.org/intel-sgx/sgx-linux/2.9.1/docs/Intel_SGX_Installation_Guide_Linux_2.9.1_Open_Source.pdf) to install Intel SGX driver, Intel SGX SDK & PSW for Linuxw.
  - For CentOS 8.1, UAE service libraries are needed but may not installed if SGX PSW installer is used. Please manually install it:
    ```shell
    rpm -i libsgx-uae-service-2.9.101.2-1.el8.x86_64.rpm
    ```

- Install [enable_rdfsbase kernel module](https://github.com/occlum/enable_rdfsbase#how-to-build), allowing to use FSGSBASE instructions in Occlum.

- Install rune and occlum-pal from [Inclavare Container release page](https://github.com/alibaba/inclavare-containers/releases/).
  - For CentOS 8.1:
    ```shell
    sudo yum install -y libseccomp
    sudo rpm -i rune-0.4.0-1.el8.x86_64.rpm
    sudo rpm -i occlum-pal-0.15.1-1.el8.x86_64.rpm
    ```

    - For Ubuntu 18.04-server:
      ```shell
      sudo dpkg -i rune_0.4.0-1_amd64.deb
      dpkg -i occlum-pal_0.15.1-1_amd64.deb
      ```

# Building Occlum container image

## Launch Occlum SDK container image

```shell
mkdir "$HOME/rune_workdir"
docker run -it --privileged --device /dev/isgx \
  -v "$HOME/rune_workdir":/root/rune_workdir \
  occlum/occlum:0.15.1-centos8.1
```

## Prepare "hello world" demo program

[This tutorial](https://github.com/occlum/occlum#hello-occlum) can help you to create your first occlum build.

Assuming "hello world" demo program is built, execute the following command:

```shell
cp -a occlum_instance /root/rune_workdir
```

## Create Occlum container image

Now you can build your occlum container image in the $HOME/rune_workdir directory of your host system.

Type the following commands to create a `Dockerfile`:

```Dockerfile
cd "$HOME/rune_workdir/occlum_instance"
cat >Dockerfile <<EOF
FROM centos:8.1.1911

RUN mkdir -p /run/rune
WORKDIR /run/rune

COPY Occlum.json ./
COPY build ./build
COPY image ./image
COPY run ./run

ENTRYPOINT ["/bin/hello_world"]
EOF
```

then build the Occlum container image with the command:

```shell
docker build . -t occlum-app
```

# Configuring OCI Runtime rune for Docker

Add the assocated configuration for `rune` in dockerd config file, e.g, `/etc/docker/daemon.json`, on your system.

```json
{
	"runtimes": {
		"rune": {
			"path": "/usr/bin/rune",
			"runtimeArgs": []
		}
	}
}
```

then restart dockerd on your system.

You can check whether `rune` is correctly enabled or not with:

```shell
docker info | grep rune
```

The expected result would be:

```
Runtimes: rune runc
```

# Running Occlum container image

You need to specify a set of parameters to `docker run` to run:

```shell
docker run -it --rm --runtime=rune \
  -e ENCLAVE_TYPE=intelSgx \
  -e ENCLAVE_RUNTIME_PATH=/opt/occlum/build/lib/libocclum-pal.so.0.15.1 \
  -e ENCLAVE_RUNTIME_ARGS=./ \
  occlum-app
```

where:
- @ENCLAVE_TYPE: specify the type of enclave hardware to use, such as `intelSgx`.
- @ENCLAVE_PATH: specify the path to enclave runtime PAL to launch.
- @ENCLAVE_ARGS: specify the specific arguments to enclave runtime PAL, separated by the comma.
