[![Build Status](https://travis-ci.org/irrdnet/irrd.svg?branch=master)](https://travis-ci.org/irrdnet/irrd)

This is the source distribution of the Internet Routing Registry Daemon (IRRd).

IRRd is jointly maintained by Merit Network, Inc. and NTT Communications, Inc.
employees.

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

Binaries are installed in /usr/local/sbin by default.

Send email to [irrd@rpsl.net](http://lists.rpsl.net/mailman/listinfo/irrd) for
assistance/bug/comments or use the [Github](https://github.com/irrdnet/irrd/issues) system to file an issue.
