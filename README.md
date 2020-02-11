About this legacy repository
============================

This is the source distribution of the legacy Internet Routing Registry Daemon
(IRRd). **Users of IRRd are strongly recommended to migrate to [IRRd version 4](https://github.com/irrdnet/irrd)!**

Legacy IRRd (up to and including version 3) were jointly maintained by Merit
Network, Inc. and NTT Communications, Inc. employees. The legacy IRR code base
is no longer maintained.

A user guide is included as part of the distribution as irrd-user.pdf.

IRRd depends on bison, flex and the glib2 development files.

To build and install the distribution, execute the following commands:

```
git clone https://github.com/irrdnet/irrd.git
cd irrd/src
./autogen.sh
./configure
make
make install
```

Ubuntu 12 notes
===============

```
sudo apt-get install byacc automake autoconf build-essential gnupg flex
```

Ubuntu 14 notes
===============

```
sudo apt-get install byacc automake autoconf build-essential gnupg flex libglib2.0-dev
```

Binaries are installed in /usr/local/sbin by default.
