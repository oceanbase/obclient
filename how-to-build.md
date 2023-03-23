# How to build OBClient

## Prerequisites

For RedHat, CentOS, Anolis (), and Fedora operation systems, run this command to install dependencies:

```bash
yum install git gcc g++ cmake openssl openssl-libs openssl-devel ncurses-devel bison mariadb mariadb-libs mariadb-devel
```

For Ubuntu and Debian operation systems, run this command to install dependencies:

```bash
apt-get install git gcc g++ cmake openssl libssl-dev libncurses5-dev bison mysql-server mysql-client libmariadbd18 libmariadbd-dev
```

## Download the code

To download the code, follow these steps:

```bash
mkdir code
cd code
git clone https://github.com/oceanbase/obclient
git clone https://github.com/oceanbase/obconnector-c
cd obclient 
rm -rf libmariadb && cp -rf ../obconnector-c libmariadb
```

## Compile the code

To compile the code, follow these steps:

```bash
cd code/obclient
./build.sh
sudo make install
```

## (Optional) Set environment variables

When you run `make install` command, if you can not find the obclient binary file, try this step:

```bash
find / -name obclient
```

Add the obclient binary to your PATH.

- For Debian operation system, run this command to add PATH.

    ```bash
    export PATH=/app/mariadb/bin:$PATH
    ```

- For Ubuntu, run this command to add PATH.

    ```bash
    export PATH=/u01/obclient/bin:$PATH
    ```
